# fast-ecs
A C++14 fast, storage-wise, header-only Entity Component System library.

* fast-ecs is **fast** because it is cache-friendly: it organizes all the information it needs to iterate the entities and read the components in one single array. In my computer, it takes 0.005 us to iterate each entity (this is 0.000005ms!).
* fast-ecs is **storage-wise** because it organizes all components in one single array without gaps. This way, no space is wasted.
* fast-ecs is **header-only** - just include the header below and you're good to go! No need to link to external libraries.

### [Download the header file here!](https://raw.githubusercontent.com/andrenho/fast-ecs/master/fastecs.hh)

# Example

```C++
#include <iostream>

#include "fastecs.hh"

//
// COMPONENTS
//

struct Position {
    Position(float x, float y) : x(x), y(y) {}
    float x, y;
    COMP_ID(0);     // uniquely identify the component
};

struct Direction {
    Direction(float angle) : angle(angle) {}
    float angle;
    COMP_ID(1);
};

//
// SYSTEMS
//

class System { 
public:
    virtual void Execute(ECS::Engine<System>& e) = 0;
    virtual ~System() {}
};

class PositionSystem : public System {
public:
    void Execute(ECS::Engine<System>& e) override {
        e.ForEach<Position>([](ECS::Entity entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public System {
public:
    void Execute(ECS::Engine<System>& e) override {
        e.ForEach<Direction>([](ECS::Entity entity, Direction& dir) {
            std::cout << "Entity " << entity << " direction is " << dir.angle << ".\n";
        });
    }
};

//
// MAIN PROCEDURE
//

int main()
{
    ECS::Engine<System> e;

    ECS::Entity e1 = e.CreateEntity(),
                e2 = e.CreateEntity();

    e.AddComponent<Position>(e1, 20.f, 30.f);
    e.AddComponent<Direction>(e1, 1.2f);

    e.AddComponent<Position>(e2, 40.f, 50.f);
    e.GetComponent<Position>(e2).x = 100.f;

    e.AddSystem<PositionSystem>();
    e.AddSystem<DirectionSystem>();

    for(auto& sys: e.Systems()) {
        sys->Execute(e);
    }
}

/* Result:

Entity 0 position.x was 20 but now is 21.
Entity 1 position.x was 100 but now is 101.
Entity 0 direction is 1.2.
*/
```

# API

Engine management:

```C++
Engine<System>();   // create a new Engine
// System is the parent class for all systems. It can be left blank if systems are not used.

void Reset();       // remove all entities, components and systems
```

Entity management:

```C++
Entity CreateEntity();             // create a new entity
void   RemoveEntity(Entity ent);   // delete an entity
size_t EntityCount();              // return the number of entities
```

Component management:


```C++
// Every component is a class/struct must contain a unique ID. Example:

struct Position {
    double x, y;
    COMP_ID(1);    // a different identifier must be set for each component
}
```

```C++
C&   AddComponent<C>(Entity ent, ...);   // add a new component to an entity, calling its constructor
void RemoveComponent<C>(Entity ent);     // remove component from entity
void RemoveAllComponents();              // clear an entity (good for entity reuse without deleting)

bool HasComponent<C>(Entity ent);        // return true if entity contains a component
C&   GetComponent<C>(Entity ent);        // return a reference to a component

// When getting a component, it can be edited directly:
e.GetComponent<Position>().x = 10;
```

Iterating over entities: 

```C++
void ForEach<C...>([](ECS::Entity, ...);

// Example:
e.ForEach<Position, Direction>([](ECS::Entity ent, Position& pos, Direction& dir) {
    // do something
});
```

Systems:

```C++
// all systems must inherit from a single class
struct System {
    virtual void Execute(Engine<System>& e) = 0;
    virtual ~System() {}
}

struct MySystem : public System {
    void Execute(Engine<System>& e) override {
        // do something
    }
};

System&         AddSystem<S>(...);    // add a new system (call constructor)
System&         GetSystem<S>();       // return a reference to a system
vector<System*> Systems();            // return a vector of systems to iterate, example:

for(auto& sys: e.Systems()) {
    sys->Execute(e);
}

// You can only add one system of each type (class).
```

Debugging:

```C++
// for debugging, you need to compile with -DDEBUG or use
#define DEBUG 1

// each component needs to have a method with the following signature
struct Component {
    string to_str() const;
    // ...
};

// then, to inspect all the entities
void Examine<C...>();	 // where "C..." is a list of all components

// example:
e.Examine<Position, Direction>();
```