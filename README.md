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
};

struct Direction {
    Direction(float angle) : angle(angle) {}
    float angle;
};

// 
// ENGINE
//

using MyEngine = ECS::Engine<class System, Position, Direction>;

//
// SYSTEMS
//

class System { 
public:
    virtual void Execute(MyEngine& e) = 0;
    virtual ~System() {}
};

class PositionSystem : public System {
public:
    void Execute(MyEngine& e) override {
        e.ForEach<Position>([](size_t entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public System {
public:
    void Execute(MyEngine& e) override {
        e.ForEach<Direction>([](size_t entity, Direction& dir) {
            std::cout << "Entity " << entity << " direction is " << dir.angle << ".\n";
        });
    }
};

//
// MAIN PROCEDURE
//

int main()
{
    MyEngine e;

    size_t e1 = e.AddEntity(),
           e2 = e.AddEntity();

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
```

The result is:

```
Entity 0 position.x was 20 but now is 21.
Entity 1 position.x was 100 but now is 101.
Entity 0 direction is 1.2.
```

# API

Engine management:

```C++
Engine<System, Components...>();   // create a new Engine
// `System` is the parent class for all systems. 
//     Use "void" if you don't want to use any systems.
// `Components...` is a list of all components (structs) 
//     that can be added to the Engine.

// Since you'll want to use the engine declaration everywhere
// (pass to functions, etc), it is better to use a type-alias:
using MyEngine = ECS::Engine<System, Position, Direction>;
```

Entity management:

```C++
size_t AddEntity();                // create a new entity, and return that entity identifier number
void   RemoveEntity(Entity ent);   // delete an entity
```

Component management:


```C++
// A component is simply a struct.

struct Position {
    double x, y;
    Position(double x, double y) : x(x), y(y) {}
};

// More complex components can be used. The destructor will be 
// called when the component is destroyed.

struct Polygon {
    vector<Point> points = {};
};

```

```C++
C&   AddComponent<C>(size_t entity, ...);   // add a new component to an entity, calling its constructor
void RemoveComponent<C>(size_t entity);     // remove component from entity

bool HasComponent<C>(size_t entity);        // return true if entity contains a component
C&   GetComponent<C>(size_t entity);        // return a reference to a component

// When getting a component, it can be edited directly:
e.GetComponent<Position>(my_entity).x = 10;
```

Iterating over entities: 

```C++
void ForEach<C...>([](size_t entity, ...);

// Example:
e.ForEach<Position, Direction>([](size_t ent, Position& pos, Direction& dir) {
    // do something
});
```

Management:

After deleting and readding many entities and components, the component array will become fragmented. So, eventually, is a good idea to compress the array, removing the spaces that were left:

```C++
void Compress();
```


Systems:

```C++
// all systems must inherit from a single class
struct System {
    virtual void Execute(MyEngine& e) = 0;
    virtual ~System() {}
}

struct MySystem : public System {
    void Execute(MyEngine& e) override {
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

# Component printing

To be able to print a component, the `operator<<` must be implemented. Example:

```C++
struct Direction {
    float angle;
    friend ostream& operator<<(ostream& out, Direction const& dir);
};

ostream& operator<<(ostream& out, Direction const& dir) {
    out << "Direction: " << dir.angle << " rad";
    return out;
}

// Then, to print all components of an entity:
e.Examine(cout, my_entity);

// If you want to print all components of all entities:
e.Examine(cout);
```

If the `operator<<` is not implemented for a component, the class name will be print instead.
