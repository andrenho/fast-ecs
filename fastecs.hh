/* ENGINE INTERFACE

  Entity management:

    Entity      CreateEntity();
    void        RemoveEntity(Entity entity);
    size_t      EntityCount();

  Component management:

    Component&  AddComponent<C>(Entity entity);
    void        RemoveComponent<C>(Entity entity);
    bool        HasComponent<C>(Entity entity);
    Component&  GetComponent<C>(Entity entity);

  Iterating:

    void        ForEach<C...>(function<void(Engine&, Entity, C...)>);

  Systems:

    void        AddSystem<S>(...);
    System&     GetSystem<S>();
    vector<>    Systems();
*/

#ifndef ENGINE_HH_
#define ENGINE_HH_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <vector>

/*
typedef uint16_t component_id_t;
typedef uint32_t entity_size_t;
*/

#define COMP_ID(x) static constexpr size_t _COMP_ID = (x);

typedef size_t Entity;

template<typename entity_size_t=uint32_t, typename component_size_t=uint16_t, typename component_id_t=uint16_t>
class Engine {
	friend struct test_debug;  // for debugging
public:
    //
    // ENTITY MANAGEMENT
    //
    Entity CreateEntity() {{{
		_entity_index.push_back(_components.size());
        entity_size_t sz = sizeof(entity_size_t);
		_components.resize(sz, 0);
        memcpy(&_components[_components.size()-sz], &sz, sz);
		return _entity_index.size() - 1;
	}}}

    void RemoveEntity() { }
    size_t EntityCount() {{{ 
		return _entity_index.size(); 
	}}}

    //
    // COMPONENT MANAGEMENT
    //
    template<typename C, typename... P> C& AddComponent(Entity entity, P... pars) {{{
		// assertions
		static_assert(sizeof(C) <= std::numeric_limits<component_size_t>::max(), 
                "Component size > maximum component size");
		static_assert(C::_COMP_ID <= std::numeric_limits<component_id_t>::max(), 
                "Component ID > maximum component id");
        static_assert(C::_COMP_ID != std::numeric_limits<component_id_t>::max(), 
                "Please do not use the maximum value as the component ID - this is reserved for deleted components.");

		// find index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];

		// find entity current size
		entity_size_t sz = 0;
		memcpy(&sz, &_components[idx], sizeof(entity_size_t));

		// calculate new size
		size_t extend_size = sizeof(component_size_t) + sizeof(component_id_t) + sizeof(C);
		if(sz + extend_size >= std::numeric_limits<entity_size_t>::max()) {
			fprintf(stderr, "Entity size exceeded.\n");
			abort();
		}

		// open space
		size_t comp_pos = ChangeSpace(idx, extend_size);
		
		// change entity size
		sz += static_cast<entity_size_t>(extend_size);
		memcpy(&_components[idx], &sz, sizeof(entity_size_t));

		// add component size
		component_size_t csz = static_cast<component_size_t>(extend_size);
		memcpy(&_components[comp_pos], &csz, sizeof(component_size_t));

		// add component type
		component_id_t cid = static_cast<component_id_t>(C::_COMP_ID);
		memcpy(&_components[comp_pos + sizeof(component_id_t)], &cid, sizeof(component_id_t));

		// initialize component and copy it to the vector
		C component(pars...);
		size_t pos = comp_pos + sizeof(component_size_t) + sizeof(component_id_t);
		memcpy(&_components[pos], &component, sizeof(C));
        return *reinterpret_cast<C*>(&_components[pos]);
	}}}

    template<typename C> void RemoveComponent(Entity entity) {{{
        FindComponent<void*, C>(entity, 
                [&](entity_size_t i, bool& ret) {
                    component_id_t deleted = std::numeric_limits<component_id_t>::max();
                    memcpy(&_components[i + sizeof(component_size_t)], &deleted, sizeof(component_id_t));
                    ret = true;
                    return nullptr;
                },
                []() -> void* {
                    fprintf(stderr, "Entity does not contain this component.\n");
                    abort();
                }
        );
    }}}

    template<typename C> bool HasComponent(Entity entity) {{{
        return FindComponent<bool, C>(entity, 
                [](entity_size_t, bool& ret){ ret = true; return true; }, 
                [](){ return false; }
        );
    }}}

    template<typename C> C& GetComponent(Entity entity) {{{
        FindComponent<C&, C>(entity, 
                [&](entity_size_t i, bool& ret) -> C& {
                    component_id_t id = 0;
                    memcpy(&id, &_components[i + sizeof(component_size_t)], sizeof(component_id_t));
                    if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                        ret = true;
                        return *reinterpret_cast<C*>(&_components[i + sizeof(component_size_t) + sizeof(component_id_t)]);
                    }
                    ret = false;
                    return *reinterpret_cast<C*>(0);  // shouldn't be used
                },
                []() -> C& {
                    fprintf(stderr, "Entity does not contain this component.\n");
                    abort();
                }
        );

        // find index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];

		// find entity size
		entity_size_t sz = 0;
		memcpy(&sz, &_components[idx], sizeof(entity_size_t));

        // find component
        entity_size_t i = static_cast<entity_size_t>(idx + sizeof(entity_size_t));
        while(i < (idx + sz)) {
            component_id_t id = 0;
            memcpy(&id, &_components[i + sizeof(component_size_t)], sizeof(component_id_t));
            if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                return *reinterpret_cast<C*>(&_components[i + sizeof(component_size_t) + sizeof(component_id_t)]);
            }
           
            component_size_t csz = 0;
            memcpy(&csz, &_components[i], sizeof(component_id_t));
            i += csz;
        }
        
        fprintf(stderr, "Entity does not contain this component.\n");
        abort();
	}}}

    //
    // ITERATING
    //
    template<typename F, typename... C> void ForEach(F const& f) {}

private:
	std::vector<size_t>  _entity_index = {};
	std::vector<uint8_t> _components = {};

	size_t ChangeSpace(size_t pos, size_t sz) {{{
        (void) pos;
		if(sz > 0) {
			size_t n = _components.size();
			_components.resize(_components.size() + sz);
			return n;
		}
		return 0;  // TODO
	}}}


    template<typename R, typename C, typename F1, typename F2> 
    R FindComponent(Entity entity, F1 const& if_found, F2 const& if_not_found) {{{
        // find index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];

		// find entity size
		entity_size_t sz = 0;
		memcpy(&sz, &_components[idx], sizeof(entity_size_t));

        // find component
        entity_size_t i = static_cast<entity_size_t>(idx + sizeof(entity_size_t));
        while(i < (idx + sz)) {
            component_id_t id = 0;
            memcpy(&id, &_components[i + sizeof(component_size_t)], sizeof(component_id_t));
            if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                bool r = false;
                R v = if_found(i, r);
                if(r) { 
                    return v;
                }
            }
           
            component_size_t csz = 0;
            memcpy(&csz, &_components[i], sizeof(component_id_t));
            i += csz;
        }
        return if_not_found();   
    }}}
};

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
