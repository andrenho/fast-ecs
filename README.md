# fast-ecs
A C++14, fast, cache-friendly, storage-wise, header-only Entity Component System library.

# API

Engine management

```C++
Engine<>();		// create a new Engine with default sizes
Engine<E, C, I>(); 	// create Engine with specific types for entity size (E), component size (C) and component id (I)
void Reset();		// remove all entities, components and systems
```
