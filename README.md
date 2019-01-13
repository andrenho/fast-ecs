# fast-ecs
A C++17 fast, storage-wise, header-only Entity Component System library.

* fast-ecs is **fast** because it is cache-friendly: it organizes all the information it needs to iterate the entities and read the components in one single array. In my computer, it takes 0.005 us to iterate each entity (this is 0.000005ms!).
* fast-ecs is **storage-wise** because it organizes all components in one single array without gaps. This way, no space is wasted.
* fast-ecs is **header-only** - just include the header below and you're good to go! No need to link to external libraries.

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

using MyEngine = ecs::Engine<
	class System, ecs::NoGlobal, ecs::NoEventQueue, 
	Position, Direction            // <-- component list
>;

//
// SYSTEMS
//

class System { 
public:
    virtual void execute(MyEngine& e) = 0;
    virtual ~System() {}
};

class PositionSystem : public System {
public:
    void execute(MyEngine& e) override {
        e.for_each<Position>([](MyEngine&, ecs::Entity const& entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity.get() << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public System {
public:
    void execute(MyEngine& e) override {
        e.for_each<Direction>([](MyEngine&, ecs::Entity const& entity, Direction& dir) {
            std::cout << "Entity " << entity.get() << " direction is " << dir.angle << ".\n";
        });
    }
};

//
// MAIN PROCEDURE
//

int main()
{
    MyEngine e;

    ecs::Entity e1 = e.add_entity(),
                e2 = e.add_entity();

    e.add_component<Position>(e1, 20.f, 30.f);
    e.add_component<Direction>(e1, 1.2f);

    e.add_component<Position>(e2, 40.f, 50.f);
    e.component<Position>(e2).x = 100.f;

    e.add_system<PositionSystem>();
    e.add_system<DirectionSystem>();

    for(auto& sys: e.systems()) {
        sys->execute(e);
    }
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
Engine<System, Global, Event, Components...>();   // create a new Engine
// `System` is the parent class for all systems. 
//     Use `ecs::NoSystem` if you don't want to use any systems.
// `Global` is a class for storing global data. In needs to be default constructive.
//     Use `ecs::NoGlobal` if you don't want to use any systems.
// `Event` is a variant type that contains all event types. It must be a std::variant<...>.
//     Use `ecs::NoEventQueue` if you don't want to use an event queue.
// `Components...` is a list of all components (structs) that can be added to the Engine.
//     They need to be copyable.

// Since you'll want to use the engine declaration everywhere
// (pass to functions, etc), it is better to use a type-alias:
using MyEngine = ecs::Engine<System, Global, Event, Position, Direction>;
```

## Entity management

```C++
ecs::Entity add_entity(std::string name=""); // create a new entity, and return that entity identifier
void        remove_entity(Entity ent);       // delete an entity
```

`ecs::Entity` is simply a wrapper around a `size_t`, as the entity is simply a number. The
real number can be read by using the entity `get()` method.

A name can be given to the entity. This is useful for one-of-a-kind entites. All later request
can be referred using this name, such as:

```C++
e.add_entity("my_name");
e.remove_entity("my_name");

ecs::Entity entity_id = e.entity("my_name);
```

The library also provides a entity called `ecs::InvalidEntity`, that can be used to represent
uninitialized references to entities.

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

Avoid using pointers in components, as it defeats the porpouse of the high speed array of this library.

Also, remember that entities and components might be moved within the array, so pointers to the components won't work. Always refer to the entities by their identifier (`ecs::Entity`) or name (`std::string`).

```C++
C&   add_component<C>(ecs::Entity entity, ...);   // add a new component to an entity, calling its constructor
C&   add_component(ecs::Entity entity, C&& c);    // add a new existing component to an entity
void remove_component<C>(ecs::Entity entity);     // remove component from entity

bool has_component<C>(ecs::Entity entity);        // return true if entity contains a component
C&   component<C>(ecs::Entity entity);            // return a reference to a component

// When getting a component, it can be edited directly:
e.component<Position>(my_entity).x = 10;

// `component_ptr` will return a pointer for the component, or nullptr if it doesn't exist.
// Thus, it can be used as a faster combination of `has_component` and `component`.
C*   component_ptr<C>(ecs::Entity entity);
```

## Iterating over entities

```C++
void for_each<C...>([](Engine<...>& e, ecs::Entity entity, ...), bool iterate_inactive=false);

// Example:
e.for_each<Position, Direction>([](MyEngine& e, ecs::Entity const& ent, Position& pos, Direction& dir) {
    // do something
});

// There's also a const version of ForEach: 
e.for_each<Position, Direction>([](MyEngine const& e, ecs::Entity const& ent, Position const& pos, Direction const& dir) {
    // do something
});

// Iterate over all entities
e.for_each([](MyEngine& e, ecs::Entity const& ent) {
    // do something
});
```

The `for_each` function is the central piece of this ECS library, and a lot of care has been taken
to make sure that it is as fast as possible.

Entities can be set as active or inactive. They are created active by default. An inactive entity will
not be iterated in `for_each`. This is useful in a game, for example, when there is a very large map
but only the entities close to the player will move.

Entities can be activated/deactivated with the following functions:

```C++
void set_entity_active(ecs::Entity ent, bool active);
bool is_entity_active(ecs::Entity ent);
```

To iterate over all entities, active and inactive, pass `iterate_inactive` as true in the call to `for_each`.

## Systems

```C++
// all systems must inherit from a single class
struct System {
    virtual void execute(MyEngine& e) = 0;
    virtual ~System() {}
}

struct MySystem : public System {
    void execute(MyEngine& e) override {
        // do something
    }
};

System&         add_system<S>(...);    // add a new system (call constructor)
System const&   system<S>() const;     // return a reference to a system
vector<System*> systems();             // return a vector of systems to iterate, example:
void            remove_system<S>();    // remove a previously added system

/* The method `system<T>` returns a const referente to a system. It is used for one
   system to read information from another system. To make one system modify data
   in another system, use Events.  */

for(auto& sys: e.systems()) {
    sys->execute(e);
}

// You can only add one system of each type (class).
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

using MyEngine = ecs::Engine<class System, GlobalData, ecs::NoQueue, MyComponent>;
MyEngine e(42);

std::cout << e.global().x << "\n";    // result: 42
e.global().x = 8;
std::cout << e.global().x << "\n";    // result: 8
```

## Event queues

Events queues can be used by a system to send messages to all systems. The message
type must be a `std::variant` that contains all message types.

```C++
#include <variant>

// Define the message types.
struct EventDialog { std::string msg; };
struct EventKill   { ecs::Entity id; };
using EventType = std::variant<EventDialog, EventKill>;

// Create engine passing this type.
using MyEngine = ecs::Engine<System, EventType, MyComponents...>;
MyEngine e;

// Send an event to all systems.
e.send_event(EventDialog { "Hello!" });

// In the system, `events` can be used to read each of the messages in the event queue.
// This will not clear the events from the queue, as other system might want to read it as well.
for (EventDialog const& ev: e.event_queue<EventDialog>()) {
    // do something with `ev`...
}

// At the end of each loop, the queue must be cleared.
for(auto& sys: e.systems())
    sys->execute(e);
e.clear_queue();
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
cout << e.debug_entity(my_entity) << "\n";

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

A phrase describing the entity can be added with the function below. It'll be printed when
the entity is debugged:

```C++
void set_entity_debugging_info(Entity ent, std::string text);
```

If the `operator<<` is not implemented for a component, the class name will be printed instead.

By default, only the active entities will be printed when debugging all. To print everything,
call `e.debug_all(true);`.

## Additional info:

The following methods provide additional info about the engine:

```C++
size_t number_of_entities();
size_t number_of_components();
size_t number_of_event_types();
size_t number_of_systems();
size_t event_queue_size();
```
