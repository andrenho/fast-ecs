#ifndef FASTECS_HH_
#define FASTECS_HH_

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#ifdef GTEST
#  include <gtest/gtest_prod.h>
#endif

namespace ECS {

class ECSError : public std::runtime_error {
public:
    explicit ECSError(std::string const& what_arg) : runtime_error(what_arg) {}
    explicit ECSError(const char* what_arg) : runtime_error(what_arg) {}
};

template<
    typename System,
    uint64_t max_components, uint64_t max_entity_size, uint64_t max_component_size,
    typename... Components>
class Engine {
private:
    // {{{ RAW DATA INTERFACE

    template<typename entity_size_t, typename component_id_t, typename component_size_t>
    class RawData {

        static_assert(std::is_signed<entity_size_t>::value, "Entity size type must be signed.");
        static_assert(std::is_unsigned<component_id_t>::value, "Component ID type must be unsigned.");
        static_assert(std::is_unsigned<component_size_t>::value, "Component size type must be signed.");

    public:
        struct Entity {
            entity_size_t sz;
            void*         data;
        };

        struct Component {
            component_size_t sz;
            component_id_t   id;
        };

        static constexpr size_t         INVALIDATED_ENTITY    = std::numeric_limits<size_t>::max();
        static constexpr component_id_t INVALIDATED_COMPONENT = std::numeric_limits<component_id_t>::max();

        size_t AddEntity() {
            // insert index in _entities
            _entities.push_back(_ary.size());
            // insert size in _ary
            entity_size_t sz = static_cast<entity_size_t>(sizeof(entity_size_t));
            _ary_append_bytes(&sz, sz, -1);
            return _entities.size()-1;
        }

        bool IsEntityValid(size_t entity) const {
            return GetEntitySize(entity) >= 0;
        }

        entity_size_t GetEntitySize(size_t entity) const {
            return *reinterpret_cast<entity_size_t const*>(&_ary[_entities[entity]]);
        }

        uint8_t* GetEntityPtr(size_t entity) {
            return &_ary[entity];
        }

        uint8_t const* GetEntityPtr(size_t entity) const {
            return &_ary[entity];
        }

        void InvalidateEntity(size_t entity) {
            // IMPORTANT: this doesn't call the destructor for the entities
            entity_size_t* entity_sz = reinterpret_cast<entity_size_t*>(&_ary[_entities[entity]]);
            memset(&_ary[_entities[entity] + sizeof(entity_size_t)], 0xFF, *entity_sz - sizeof(entity_size_t));
            *entity_sz = -(*entity_sz);
            _entities[entity] = INVALIDATED_ENTITY;
        }

        void AddComponent(size_t entity, component_size_t sz, component_id_t id, void* data) {
            // create component
            Component component { sz, id };
            // TODO - look for space for a existing component
            size_t idx = _find_space_for_component(entity, sizeof(Component) + sz);
            _ary_copy_bytes(&component, sizeof(Component), idx);
            _ary_copy_bytes(data, sz, idx + sizeof(Component));
        }

        template<typename F>
        void ForEachEntity(F const& f, bool skip_invalid = true) {
            if(_ary.empty()) {
                return;
            }
            size_t entity = 0;
            uint8_t *entity_ptr = &_ary[0],
                    *entity_end = &_ary[0] + _ary.size();
            while(entity_ptr < entity_end) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
                
                if((skip_invalid && entity_sz >= 0) || !skip_invalid) {
                    bool stop = f(entity, entity_ptr);
                    if(stop) {
                        return;
                    }
                }
                
                entity_ptr += abs(entity_sz);
                ++entity;
            }
        }

        template<typename F>
        void ForEachEntity(F const& f, bool skip_invalid = true) const {
            if(_ary.empty()) {
                return;
            }
            size_t entity = 0;
            uint8_t const* entity_ptr = &_ary[0],
                    *entity_end = &_ary[0] + _ary.size();
            while(entity_ptr < entity_end) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t const*>(entity_ptr);
                
                if((skip_invalid && entity_sz >= 0) || !skip_invalid) {
                    bool stop = f(entity, entity_ptr);
                    if(stop) {
                        return;
                    }
                }
                
                entity_ptr += abs(entity_sz);
                ++entity;
            }
        }

        // TODO - const version
        template<typename F>
        void ForEachComponentInEntity(uint8_t* entity_ptr, F const& f) {
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }
            uint8_t* initial_entity_ptr = entity_ptr;
            uint8_t* end = entity_ptr + entity_sz;
            entity_ptr += sizeof(entity_size_t);
            while(entity_ptr < end) {
                Component* component = reinterpret_cast<Component*>(entity_ptr);
                if(f(component, entity_ptr + sizeof(Component), static_cast<entity_size_t>(entity_ptr - initial_entity_ptr))) {
                    return;
                }
                entity_ptr += component->sz + sizeof(Component);
            }
        }

        template<typename F>
        void ForEachComponentInEntity(uint8_t const* entity_ptr, F const& f) const {
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t const*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }
            uint8_t const* initial_entity_ptr = entity_ptr;
            uint8_t const* end = entity_ptr + entity_sz;
            entity_ptr += sizeof(entity_size_t);
            while(entity_ptr < end) {
                Component const* component = reinterpret_cast<Component const*>(entity_ptr);
                if(f(component, entity_ptr + sizeof(Component), static_cast<entity_size_t>(entity_ptr - initial_entity_ptr))) {
                    return;
                }
                entity_ptr += component->sz + sizeof(Component);
            }
        }

        template<typename F>
        void InvalidateComponent(size_t entity, component_id_t id, F const& destructor) {
            uint8_t* entity_ptr = &_ary[_entities[entity]];

            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
            if(entity_sz < 0) {
                throw ECSError("Using a removed entity");
            }

            ForEachComponentInEntity(entity_ptr, [&](Component* component, void* data, entity_size_t) {
                if(id == component->id) {
                    destructor(data);
                    component->id = INVALIDATED_COMPONENT;
                    return true;  // stop searching
                }
                return false;
            });
        }

        void Compress() {
            vector<uint8_t> newary = {};
            newary.reserve(_ary.size());   // avoids multiple resizing - we shrink it later

            ForEachEntity([&](size_t entity, uint8_t* entity_ptr) {
                entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
                size_t current_newary = newary.size();
                // if entity is not invalidated, add it to the new ary
                if(entity_sz >= 0) {
                    entity_size_t current_sz = sizeof(entity_size_t);
                    newary.insert(end(newary), sizeof(entity_size_t), 0);  // placeholder
                    // if component is not invalidated, add it to the new ary
                    ForEachComponentInEntity(entity_ptr, [&](Component* component, void* data, entity_size_t) {
                        if(component->id != INVALIDATED_COMPONENT) {
                            size_t sz = newary.size();
                            newary.insert(end(newary), sizeof(Component) + component->sz, 0);
                            memcpy(&newary[sz], component, sizeof(Component));
                            memcpy(&newary[sz + sizeof(Component)], data, component->sz);
                            current_sz += static_cast<entity_size_t>(sizeof(Component) + component->sz);
                        }
                        return false;
                    });
                    // adjust entity size
                    entity_size_t* entity_sz_ptr = reinterpret_cast<entity_size_t*>(&newary[current_newary]);
                    *entity_sz_ptr = current_sz;
                    // adjust _entities
                    _entities[entity] = current_newary;
                }
                return false;
            }, false);

            newary.shrink_to_fit();
            _ary = move(newary);
        }

    private:
#ifdef GTEST
        FRIEND_TEST(RawTest, AddEntity);
        FRIEND_TEST(RawTest, AddComponents);
        FRIEND_TEST(RawTest, InvalidateComponents);
        FRIEND_TEST(RawTest, InvalidateEntities);
        FRIEND_TEST(RawTest, Compress);
        FRIEND_TEST(RawTest, DifferentSizes);
        FRIEND_TEST(RawTest, InvalidSizes);
        FRIEND_TEST(RawTest, IterateConst);
#endif

        std::vector<size_t> _entities = {};
        std::vector<uint8_t> _ary = {};

        template<typename PTR>
        void _ary_append_bytes(PTR* origin, size_t sz, ssize_t pos) {
            if(pos == -1) {
                pos = _ary.size();
            }
            _ary.insert(begin(_ary)+pos, sz, 0);
            _ary_copy_bytes(origin, sz, pos);
        }

        template<typename PTR>
        void _ary_copy_bytes(PTR* origin, size_t sz, size_t pos) {
            memcpy(&_ary[pos], reinterpret_cast<uint8_t*>(origin), sz);
        }

        size_t _find_space_for_component(size_t entity, size_t total_sz) {
            // find entity size
            if(entity >= _entities.size()) {
                throw ECSError("Entity does not exist");
            }
            size_t idx = _entities[entity];
            if(idx == INVALIDATED_ENTITY) {
                throw ECSError("Using a removed entity");
            }
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(&_ary[idx]);
            
            // check if we can reuse any inactive components
            uint8_t* entity_ptr = &_ary[idx];
            entity_size_t pos = -1;
            ForEachComponentInEntity(entity_ptr, [&](Component* component, void*, entity_size_t offset) -> bool {
                if(component->id == INVALIDATED_COMPONENT && component->sz <= entity_sz) {
                    pos = offset;
                    return true;
                }
                return false;
            });
            if(pos != -1) {
                return idx + static_cast<size_t>(pos);
            }

            // we can't reuse inactive components, so we open space in _ary
            if(entity_sz + total_sz > std::numeric_limits<entity_size_t>::max()) {
                throw ECSError("By adding this component, the entity would become too large.");
            }
            _ary.insert(begin(_ary) + idx + entity_sz, total_sz, 0);
            // adjust indexes
            for(auto it=begin(_entities)+entity+1; it != end(_entities); ++it) {
                *it += total_sz;
            }
            // adjust entity size
            entity_size_t* sz = reinterpret_cast<entity_size_t*>(&_ary[idx]);
            *sz = static_cast<entity_size_t>(entity_sz + total_sz);
            return idx + static_cast<size_t>(entity_sz);
        }
    };

    // }}}
/*
class RawData<entity_size_t, component_id_t, component_size_t> {
    void          AddEntity();
    bool          IsEntityValid(size_t entity) const;
    entity_size_t GetEntitySize(size_t entity) const;
    uint8_t*      GetEntityPtr(size_t) const;
    void          InvalidateEntity(size_t entity);
  
    void          AddComponent(size_t entity, component_size_t sz,
                               component_id_t id, void* data);
    void          InvalidateComponent(size_t entity, component_id_t id,
                                      function<void(void* data)> destructor);
  
    // return true to stop searching
    void          ForEachEntity(function<bool(size_t entity, uint8_t* entity_ptr)> f, 
                                bool skip_invalid = true);
    void          ForEachComponentInEntity(uint8_t* entity_ptr, 
                      function<bool(Component* c, uint8_t* data, entity_size_t pos)> f);
  
    void          Compress();
}
*/

};

}  // namespace ECS

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
