_A new version was released in 08/dec/2019. This version breaks compatibility with the previous one.
It simplifies many of the calls and allows for parallelization._

# fast-ecs
A C++17 fast, storage-wise, header-only Entity Component System library.

* fast-ecs is **fast** because it is cache-friendly: it organizes all the information it needs to iterate the entities and read the components in one single array. In my computer, it takes 0.005 us to iterate each entity (this is 0.000005ms!).
* fast-ecs is **storage-wise** because it organizes components in one single arrays without gaps. This way, no space is wasted.
* fast-ecs is **header-only** - just include the header below and you're good to go! No need to link to external libraries.
* fast-ecs is **multi-threaded** - systems that don't modify the components can run in parallel.

This library was tested with the `g++` and `clang++` compilers.

### [Download the header file here!](https://raw.githubusercontent.com/andrenho/fast-ecs/master/fastecs.hh)

# Example

```C++
#include <iostream>

#include "fastecs.hh"

//
// COMPONENTS
//

struct Position {
    float x, y;
};

struct Direction {
    float angle;
};

// 
// ENGINE
//

using MyECS = ecs::ECS<
    ecs::NoGlobal, 
    ecs::NoMessageQueue, 
    ecs::NoPool, 
    Position, Direction>;      // <-- Component list

//
// SYSTEMS
//

static void position_system(MyECS& ecs) {
    for (auto& e: ecs.entities<Position>()) {
        Position& pos = e.get<Position>();
        pos.x += 1;
        std::cout << "Entity " << e.id << " position.x was " << pos.x -1 <<
                     " but now is " << pos.x << ".\n";
    }
}

static void direction_system(MyECS const& ecs) {
    for (auto const& e : ecs.entities<Direction>()) {
        std::cout << "Entity " << e.id << " direction is " << e.get<Direction>().angle << ".\n";
    }
}

//
// MAIN PROCEDURE
//

int main()
{
    MyECS ecs;                                     // initialize engine

    auto e1 = ecs.add(),                           // create entities
         e2 = ecs.add();

    e1.add<Position>(20.f, 30.f);		   // add components
    e1.add<Direction>(1.2f);

    e2.add<Position>(40.f, 50.f);
    e2.get<Position>().x = 100.f;

    ecs.run_mutable("position", position_system);  // run system that changes the components
    ecs.run_st("direction", direction_system);     // run read-only system
}
```

The result is:

```
Entity 0 position.x was 20 but now is 21.
Entity 1 position.x was 100 but now is 101.
Entity 0 direction is 1.2.
```

# API

## Engine management

```C++
Engine<Global, Event, Pool, Components...>();   // create a new Engine
// `Global` is a class for storing global data. In needs to be default constructive.
//     Use `ecs::NoGlobal` if you don't want to use any systems.
// `Event` is a variant type that contains all event types. It must be a std::variant<...>.
//     Use `ecs::NoEventQueue` if you don't want to use an event queue.
// `Pool` is an enum that contains the list of pools.
//     Use `ecs::NoPool` if you don't want to use pools.
// `Components...` is a list of all components (structs) that can be added to the Engine.
//     They need to be copyable.

// Since you'll want to use the engine declaration everywhere
// (pass to functions, etc), it is better to use a type-alias:
using MyEngine = ecs::Engine<Global, Event, Pool, Position, Direction>;
```

## Entity management

```C++
Entity add([Pool pool]);      // Create a new entity, and return that entity identifier
                              // Optionally, the entity can be added to a pool different than the default.
void   remove(Entity ent);    // delete an entity
```

`ecs::Entity` is simply a wrapper around a `size_t`, as the entity is simply a number. The
real number can be read by using the entity `id` field.

Pools can be used to separate entities and its components in different memory blocks. This
is useful if there are different types of entities that have completely different uses (such
as units vs particles, for example). This way, when the entities are iterated, they can optionally
be iterated for just a single pool.

The type `Entity` is a wrapper around an id, that allows to perform some operations on the entity,
according to the following signature:

```C++
class Entity {
    size_t     id;         // id number of the entity
    Pool       pool;       // pool the entity was added to

    Component& add<Component>(...);   // Add a component to the entity, creating it with `...` parameters
    Component& get<Component>();      // Return a previously created component
    Component* get_ptr<Component>();  // Return a pointer to a component, or nullptr if the component
                                      // doesn't exists
    Component  remove<Component>();   // Remove a component
    bool       has<Component>();      // Returns true if the entity has the component.
    string     debug();               // Returns a textual description of the entity.
}
```

## Component management

```C++
// A component is simply a struct.

struct Position {
    double x, y;
};

// More complex components can be used. The destructor will be 
// called when the component is destroyed.

struct Polygon {
    std::vector<Point> points = {};
    std::string        description;
};
```

Components need to be copyable.

Avoid using pointers in components, as it defeats the porpouse of the high speed array of this library.
However, there are occasions where pointers are necessary - for example, when using an underlying 
C library or inheritance. In this cases, since components need to be copyable, it is recommended to use
`shared_ptr` instead of `unique_ptr` (obviously, never use naked pointers). A `unique_ptr` can be used,
but then all constructors (move, copy and assignment) need to be provided.

Also, remember that entities and components might be moved within the array, so pointers to the
components won't work. Always refer to the entities by their id.

## Iterating over entities

```C++
vector<Entity> entities<Components...>([Pool pool]);  // Iterate over entities that contains these components.
                                                      // If pool is not provided, iterate over all of them.
						      // If components are not provided, iterate over all entnties.

// Example:
for (auto& e: ecs.entities<Position, Direction>()) {
    // do something
}

// There's also a const version
for (auto const& e: ecs.entities<Position, Direction>()) {
    // do something - can't modify the components
}

// Iterate over all entities
for (auto& e: ecs.entities()) {
    // do something
}
```

The `entities` method is the central piece of this ECS library, and a lot of care has been taken
to make sure that it is as fast as possible.

## Systems

Systems in `fast-ecs` have the following philosophy:
  - run the systems that won't modify the components in parallel, and then the systems that modify components single-thread;
  - this can be achieved by having the read-only systems to send messages, and then receive them in the single-threaded systems,
    and then modify the components accordingly.

```C++
// Systems can be functions or methods. They always need to receive the ECS object as its
// first parameter:

void my_system(MyECS& e, int x) {
    // do something
}

struct MySystemClass {
    void my_system(MyECS& e, int x) {
        // do something
    }
}

// To execute the read-only systems in parallel, use the following method in ECS
// to start the system in its own thread. Be aware that the entities will be const.

void run_mt(string name, F function_ptr, Parameters...);              // function version
void run_mt(string name, Object obj, F function_ptr, Parameters...);  // method version

// example, using the systems defined above

ecs.run_mt("my_system", my_system, 10);  // function version

MySystemClass sys;                   // method version
ecs.run_mt("my_system", sys, &MySystemClass::my_system, 10);

ecs.join();     // threads MUST be joined before start to execute single-treaded systems

// a method called `run_st` is provided to do the same, single threaded.

// To execute the systems in a single thread, with the abiliy to read and write
// entities, use the following:

void run_mutable(string name, F function_ptr, Parameters...);              // function version
void run_mutable(string name, Object obj, F function_ptr, Parameters...);  // method version

// the methods on `run_mt` can be run single-threaded (for debugging, for example)
// simply by setting ECS as single thread:

ecs.set_threading(Threading::Single);
```

It is possible to have ECS to calculate automatically the time it takes to run each system.

```C++
// on the beginning of the frame, call
ecs.start_frame();

// then, after running the systems, get an average of the last system execution time by using
map<string, milliseconds> timer_st();    // returns time of the single-threaded systems
map<string, milliseconds> timer_mt();    // returns time of the multithreaded systems

// The timers can be reset every X frames. For example, in a 60 FPS game, to get the 
// average time for the last second, use:

if (frame_counter % 60 == 0)
    ecs.reset_timer();
```

## Globals

Globals can be used for an unique piece of information that is shared between
all system. The global type is set on the engine initialization, and it can
be replaced by `ecs::NoGlobal` if it is not used.

The Global is initialized along with the engine. If parameters are given to
the engine constructure, these parameters are passed to the Global constructor.

```C++
struct GlobalData {
    int x;
};

using MyEngine = ecs::Engine<GlobalData, ecs::NoMessageQueue, ecs::NoPool, MyComponent>;
MyEngine e(42);

std::cout << e().x << "\n";    // result: 42
e().x = 8;
std::cout << e().x << "\n";    // result: 8
```

## Message queues

Message queues can be used by a system to send messages to all systems. The message
type must be a `std::variant` that contains all message types.

```C++
#include <variant>

// Define the message types.
struct MessageDialog { std::string msg; };
struct MessageKill   { ecs::Entity id; };
using MessageType = std::variant<MessageDialog, MessageKill>;

// Create engine passing this type.
using MyEngine = ecs::Engine<ecs::NoGlobal, MessageType, ecs::NoPool, MyComponents...>;
MyEngine e;

// Send a message to all systems.
e.add_message(MessageDialog { "Hello!" });

// In the system, `messages` can be used to read each of the messages in the event queue.
// This will not clear the events from the queue, as other system might want to read it as well.
for (auto const& msg: ecs.messages<MessageDialog>()) {
    // do something with `msg`...
}

// At the end of each loop, the queue must be cleared.
ecs.clear_messages();
```

## Component printing

To be able to print a component, the `operator<<` function must be implemented. Example:

```C++
struct Direction {
    float angle;
};

ostream& operator<<(ostream& out, Direction const& dir) {
    out << "'Direction': " << dir.angle;
    return out;
}

// Then, to print all components of an entity:
cout << entity.debug() << "\n";

// If the method `operator<<` is implemented to the Global type, global data
// can be printed with:
cout << e.debug_global() << "\n";

// If you want to print all components of all entities, and the global data:
cout << e.debug_all() << "\n";

// The result is:
{ '0':
  { 'Direction': 50 },
},
```

If the `operator<<` is not implemented for a component, the class name will be printed instead.

## Additional info:

The following methods provide additional info about the engine:

```C++
size_t number_of_entities();
size_t number_of_components();
size_t number_of_event_types();
size_t event_queue_size();
```

# Destruction order

The library is destructed in the following order:

1. Components
2. Event queue
3. Global

This means that, if a library you are using is in a system or in global, and the components
contain the elements of the library, the elements are guaranteed to be destrcted before the
library.
