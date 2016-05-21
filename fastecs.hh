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

    System&     AddSystem<S>(...);
    System&     GetSystem<S>();
    vector<>    Systems();
*/

/* STORAGE SPECIFICATION
 *
 * _components:
 *   - Entity size
 *   - Entity data:
 *      - Component size
 *      - Component ID
 *      - Component data
 */

#ifndef ENGINE_HH_
#define ENGINE_HH_

#include <algorithm>
#include <cassert>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef DEBUG
#  include <string>
#  include <sstream>
#endif

#define COMP_ID(x) static constexpr size_t _COMP_ID = (x);

struct test_debug;   // for unit testing

namespace ECS {

typedef size_t Entity;

class System {
protected:
    System() {}
public:
    virtual ~System() {}
};

template<typename entity_size_t=uint32_t, typename component_size_t=uint16_t, typename component_id_t=uint16_t>
class Engine {
    friend ::test_debug;  // for unit testing
    
    static constexpr Entity         ENTITY_DELETED    = std::numeric_limits<Entity>::max();
    static constexpr component_id_t COMPONENT_DELETED = std::numeric_limits<component_id_t>::max();
    static constexpr size_t         UNUSED_COMPONENT_NOT_FOUND = std::numeric_limits<size_t>::max();

public:
    //
    // ENGINE MANAGEMENT
    //
    void Reset() {{{
        _components.clear();   _components.shrink_to_fit();
        _entity_index.clear(); _entity_index.shrink_to_fit();
        _systems.clear();      _systems.shrink_to_fit();
    }}}

    //
    // ENTITY MANAGEMENT
    //
    Entity CreateEntity() {{{
        // add a new entity to the index
		_entity_index.push_back(_components.size());

        // resize component vector to open space for the new entity size
		_components.resize(_components.size() + sizeof(entity_size_t), 0);

        // set size to 4 (or other size)
        SetEntitySize(_components.size() - sizeof(entity_size_t), sizeof(entity_size_t));
		return _entity_index.size() - 1;
	}}}

    void RemoveEntity(Entity e) {{{
        // find index
        size_t idx = _entity_index[e];
        if(idx == ENTITY_DELETED) {
            throw std::runtime_error("Entity was already deleted.");
        }

        // get entity size
        entity_size_t sz = GetEntitySize(idx);

        // erase entity components
        _components.erase(begin(_components) + idx, begin(_components) + idx + sz);

        // adjust index pointers
        for(Entity i=e+1; i < _entity_index.size(); ++i) {
            if(_entity_index[i] != ENTITY_DELETED) {
                _entity_index[i] -= sz;
            }
        }
        
        // mark entity as deleted
        _entity_index[e] = ENTITY_DELETED;
    }}}

    size_t EntityCount() const {{{ 
        size_t total = 0;
        for(auto const& idx: _entity_index) {
            if(GetEntitySize(idx) != ENTITY_DELETED) {
                ++total;
            }
        }
		return total;
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
        static_assert(C::_COMP_ID != COMPONENT_DELETED, 
                "Please do not use the maximum value as the component ID - this is reserved for deleted components.");

		// find entity index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];
        if(idx == ENTITY_DELETED) {
            throw std::runtime_error("Entity was deleted.");
        }

		// find entity current size
        entity_size_t sz = GetEntitySize(idx);

		// calculate new size
		size_t extend_size = sizeof(component_size_t) + sizeof(component_id_t) + sizeof(C);
		if(sz + extend_size >= std::numeric_limits<entity_size_t>::max()) {
			throw std::runtime_error("Entity size exceeded.");
		}

        // can we reuse an existing, deleted component in this entity?
        size_t comp_pos = FindUnusedComponent(idx, &extend_size);
        if(comp_pos == UNUSED_COMPONENT_NOT_FOUND) {
            // open space
		    comp_pos = CreateSpace(idx + sz, extend_size, entity);
            sz += static_cast<entity_size_t>(extend_size);
        }
		
		// change entity size
        SetEntitySize(idx, sz);

		// add component size and type
        SetComponentSize(comp_pos, static_cast<component_size_t>(extend_size));
        SetComponentId(comp_pos + sizeof(component_size_t), static_cast<component_id_t>(C::_COMP_ID));

		// initialize component and copy it to the vector
		size_t pos = comp_pos + sizeof(component_size_t) + sizeof(component_id_t);
		C component(pars...);
        return SetComponentData<C>(pos, component);
	}}}

    template<typename C> void RemoveComponent(Entity entity) {{{
        FindComponent<void*, C>(entity, 
                [&](entity_size_t i, bool& ret) {
                    // set as deleted
                    SetComponentId(i + sizeof(component_size_t), COMPONENT_DELETED);
                    // find component size
                    component_size_t sz = GetComponentSize(i);
                    sz = static_cast<component_size_t>(sz - sizeof(component_size_t) - sizeof(component_id_t));
                    // clear memory area
                    memset(&_components[i + sizeof(component_size_t) + sizeof(component_id_t)], 0, sz);

                    ret = true; return nullptr;
                },
                []() -> void* {
                    throw std::runtime_error("Entity does not contain this component.");
                }
        );
    }}}

    void RemoveAllComponents(Entity entity) {{{
        // find index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];
        if(idx == ENTITY_DELETED) {
            throw std::runtime_error("Entity was deleted.");
        }

		// find entity size
		entity_size_t sz = GetEntitySize(idx);

        // find component
        entity_size_t i = static_cast<entity_size_t>(idx + sizeof(entity_size_t));
        while(i < (idx + sz)) {
            // set as deleted
            SetComponentId(i + sizeof(component_size_t), COMPONENT_DELETED);
            // find component size
            component_size_t sz = GetComponentSize(i);
            sz = static_cast<component_size_t>(sz - sizeof(component_size_t) - sizeof(component_id_t));
            // clear memory area
            memset(&_components[i + sizeof(component_size_t) + sizeof(component_id_t)], 0, sz);
           
            component_size_t csz = GetComponentSize(i);
            i += csz;
        }
    }}}

    template<typename C> bool HasComponent(Entity entity) const {{{
        return FindComponent<bool, C>(entity, 
                [](entity_size_t, bool& ret){ ret = true; return true; }, 
                [](){ return false; }
        );
    }}}

    template<typename C> C const& GetComponent(Entity entity) const {{{
        return FindComponent<C const&, C>(entity, 
                [&](entity_size_t i, bool& ret) -> C const& {
                    component_id_t id = GetComponentId(i + sizeof(component_size_t));
                    if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                        ret = true;
                        return *reinterpret_cast<C const*>(&_components[i + sizeof(component_size_t) + sizeof(component_id_t)]);
                    }
                    ret = false;
                    return *reinterpret_cast<C const*>(0);  // shouldn't be used
                },
                []() -> C& {
                    throw std::runtime_error("Entity does not contain this component.\n");
                }
        );
	}}}

    template<typename C> C& GetComponent(Entity entity) {{{
        // call the const version of this method
        return const_cast<C&>(static_cast<Engine const*>(this)->GetComponent<C>(entity));
    }}}

    //
    // ITERATION
    //
    // {{{ ITERATING

    template<typename... C, typename F> void ForEach(F const& user_function) {
        size_t idx = 0;
        Entity entity = 0;

        // iterate each entity
        while(idx < _components.size()) {
            entity_size_t esz = GetEntitySize(idx);

            // Here, we prepare for a longjmp. If the component C is not found
            // when ForEachParameter is called, it calls longjmp, which skips
            // calling the user function.
            jmp_buf env_buffer;
            int val = setjmp(env_buffer);

            // Call the user function. `val` is 0 when the component is found.
            if(val == 0) {
                user_function(*this, entity, ForEachParameter<C>(idx, esz, env_buffer)...);
            }

            // advance index pointer to the next entity
            idx += esz;
            ++entity;
        }
    }
    
private:
    template<typename C> C& ForEachParameter(size_t idx, entity_size_t esz, jmp_buf env_buffer) {
        size_t stop = idx + esz;
        idx += sizeof(entity_size_t);

        // iterate each component
        while(idx < stop) {
            
            // check if it's the correct ID
            component_id_t id = GetComponentId(idx + sizeof(component_size_t));
            if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                return *reinterpret_cast<C*>(&_components[idx + sizeof(component_size_t) + sizeof(component_id_t)]);
            }

            // if not, advance index pointer to the next component
            component_size_t csz = GetComponentSize(idx);
            idx += csz;
        }

        // component was not found, so we longjmp, so that the function
        // is not executed
        longjmp(env_buffer, 1);
    }

public:

    // }}}

    //
    // DEBUGGING
    //
    // {{{ DEBUGGING
#ifdef DEBUG
    //
    // DEBUGGING
    //
    template<typename... C> std::string Examine(Entity ent=std::numeric_limits<component_size_t>::max()) const {
        std::string s;
        if(ent == std::numeric_limits<component_size_t>::max()) {
            for(Entity i=0; i<_entity_index.size(); ++i) {
                s += Examine<C...>(i);
            }
        } else {
            std::stringstream ss;
            if(_entity_index[ent] != std::numeric_limits<size_t>::max()) {
                ss << "Entity " << ent << " ->\n";
                [&](...){ }(ExamineComponent<C>(ss, ent)...);
                s = ss.str();
            }
        }
        return s;
    }
private:
    template<typename C> int ExamineComponent(std::stringstream& ss, Entity ent) const {
        if(HasComponent<C>(ent)) {
            C const& comp = GetComponent<C>(ent);
            ss << "  " << comp.to_str() << "\n";
        }
        return 42;
    }
public:
#endif

    std::vector<uint8_t> const& ComponentVector() const { return _components; }
    // }}}

    // 
    // SYSTEM MANAGEMENT
    //
    // {{{ Manage systems

    template<typename S, typename... P> S& AddSystem(P ...pars) {
        _systems.push_back(std::make_unique<S>(pars...));
        return *static_cast<S*>(_systems.back().get());
    }

    template<typename S> S& GetSystem() {
        for(auto& sys: _systems) {
            S* s = dynamic_cast<S*>(sys.get());
            if(s) {
                return *s;
            }
        }
        throw std::runtime_error("System not found.");
    }

    std::vector<std::unique_ptr<System>> const& Systems() const { return _systems; }
    // }}}

    // 
    // PRIVATE
    //
private:
	std::vector<size_t>  _entity_index = {};
	std::vector<uint8_t> _components = {};
    std::vector<std::unique_ptr<System>> _systems = {};

    size_t FindUnusedComponent(size_t idx, size_t *extend_size) {{{
		entity_size_t sz = GetComponentSize(idx);
        size_t i = idx + sizeof(entity_size_t);
        while(i < (idx + sz)) {
            component_id_t id = GetComponentId(i + sizeof(component_size_t));
            component_size_t csz = GetComponentSize(i);

            if(id == COMPONENT_DELETED && csz <= *extend_size) {
                *extend_size = csz;
                return i;
            }

            i += csz;
        }
        return UNUSED_COMPONENT_NOT_FOUND;
    }}}

	size_t CreateSpace(size_t pos, size_t sz, Entity entity) {{{
        _components.insert(begin(_components) + pos, sz, 0);
        for(Entity e=entity+1; e < _entity_index.size(); ++e) {
            if(_entity_index[e] != std::numeric_limits<size_t>::max()) {
                _entity_index[e] += sz;
            }
        }
        return pos;
	}}}

    template<typename R, typename C, typename F1, typename F2> R FindComponent(Entity entity, F1 const& if_found, F2 const& if_not_found) const {{{
        // find index
		assert(entity < _entity_index.size());
		size_t idx = _entity_index[entity];

		// find entity size
		entity_size_t sz = GetEntitySize(idx);

        // find component
        entity_size_t i = static_cast<entity_size_t>(idx + sizeof(entity_size_t));
        while(i < (idx + sz)) {
            component_id_t id = GetComponentId(i + sizeof(component_size_t));
            if(id == static_cast<component_id_t>(C::_COMP_ID)) {
                bool r = false;
                R v = if_found(i, r);
                if(r) { 
                    return v;
                }
            }
           
            component_size_t csz = GetComponentSize(i);
            i += csz;
        }
        return if_not_found();   
    }}}

    // {{{ GET/SET DATA IN _COMPONENTS

    void SetEntitySize(size_t pos, entity_size_t sz) {
        memcpy(&_components[pos], &sz, sizeof(entity_size_t));
    }
    
    entity_size_t GetEntitySize(size_t idx) const {
		entity_size_t sz = 0;
		memcpy(&sz, &_components[idx], sizeof(entity_size_t));
        return sz;
    }

    void SetComponentSize(size_t pos, component_size_t sz) {
		memcpy(&_components[pos], &sz, sizeof(component_size_t));
    }

    component_size_t GetComponentSize(size_t pos) const {
        component_size_t sz = 0;
        memcpy(&sz, &_components[pos], sizeof(component_size_t));
        return sz;
    }

    void SetComponentId(size_t pos, component_id_t id) {
		memcpy(&_components[pos], &id, sizeof(component_id_t));
    }

    component_id_t GetComponentId(size_t pos) const {
        component_id_t id;
        memcpy(&id, &_components[pos], sizeof(component_id_t));
        return id;
    }

    template<typename C> C& SetComponentData(size_t pos, C& component) {
		memcpy(&_components[pos], &component, sizeof(C));
        return *reinterpret_cast<C*>(&_components[pos]);
    }

    // }}}
};

}  // namespace ECS

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
