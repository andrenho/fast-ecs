# fast-ecs
A C++14, fast, cache-friendly, storage-wise, header-only Entity Component System library.

# API

Engine management:

```C++
Engine<>();         // create a new Engine with default sizes
Engine<E, C, I>();  // create Engine with specific types for entity size (E), component size (C) and component id (I)
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

Iterating overall entities: 
```C++
void ForEach<C...>([](ECS::Entity, ...);

// Example:
e.ForEach<Position, Direction>([](ECS::Entity ent, Position& pos, Direction& dir) {
    // do something
});
```


