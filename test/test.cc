#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#define private public

namespace ECS {

class Engine {

private:
    template<typename entity_size_t    = uint32_t,
             typename component_id_t   = uint16_t,
             typename component_size_t = uint16_t>
    class RawData {
    public:
        struct Entity {
            entity_size_t sz;
            void*         data;
        };

        struct Component {
            component_size_t sz;
            component_id_t   id;
        };

        static constexpr component_id_t INVALIDATED_COMPONENT = std::numeric_limits<component_id_t>::max();

        // will always add an entity to the end of the vector
        size_t AddEntity() {
            // insert index in _entities
            _entities.push_back(_ary.size());
            // insert size in _ary
            entity_size_t sz = static_cast<entity_size_t>(sizeof(entity_size_t));
            _ary_append_bytes(&sz, sz, -1);
            return _entities.size()-1;
        }

        // will try to reuse an entity
        size_t AddEntity(size_t expected_size) {
            // TODO
            return 0;
        }

        entity_size_t GetEntitySize(size_t entity) {
            return *reinterpret_cast<entity_size_t*>(&_ary[_entities[entity]]);
        }

        void InvalidateEntity(Entity* entity) {
            // TODO
        }

        void RemoveIdentity(Entity* entity) {
            // TODO
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
        void ForEachComponent(uint8_t* entity_ptr, F const& f) {
            entity_size_t entity_sz = *reinterpret_cast<entity_size_t*>(entity_ptr);
            uint8_t* end = entity_ptr + entity_sz;
            entity_ptr += sizeof(entity_size_t);
            while(entity_ptr < end) {
                Component* component = reinterpret_cast<Component*>(entity_ptr);
                if(f(component, entity_ptr + sizeof(Component))) {
                    return;
                }
                entity_ptr += component->sz;
            }
        }

        void InvalidateComponent(size_t entity, component_id_t id) {
            uint8_t* entity_ptr = &_ary[_entities[entity]];
            ForEachComponent(entity_ptr, [&id](Component* component, void*) {
                if(id == component->id) {
                    component->id = INVALIDATED_COMPONENT;
                    return true;  // stop searching
                }
                return false;
            });
        }

    private:
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
            // adjust entity size
            size_t idx = _entities[entity];
            entity_size_t* entity_sz = reinterpret_cast<entity_size_t*>(&_ary[idx]);
            // TODO - reuse inactive components
            // open space in _ary
            _ary.insert(begin(_ary) + idx + (*entity_sz), total_sz, 0);
            // adjust indexes
            for(auto it=begin(_entities)+entity+1; it != end(_entities); ++it) {
                *it += total_sz;
            }
            // adjust entity size
            *entity_sz += total_sz;

            return idx + (*entity_sz) - total_sz;
        }
    };

};

}  // namespace ECS

#undef private

//-----------------------------------------------------

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

using namespace std;

TEST_CASE("Add raw entities", "[entities]") {

    using RawData = ECS::Engine::RawData<>;
    RawData rd;
    size_t e = rd.AddEntity();
    REQUIRE(e == 0);

    REQUIRE(rd.GetEntitySize(e) == 4);

    e = rd.AddEntity();
    REQUIRE(e == 1);

    REQUIRE(rd.GetEntitySize(e) == 4);

    REQUIRE(rd._entities == vector<size_t>({ 0, 4 }));
    REQUIRE(rd._ary == vector<uint8_t>({ 4, 0, 0, 0, 4, 0, 0, 0 }));

    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.AddComponent(e, sizeof my, 7, &my);
    REQUIRE(rd._ary == vector<uint8_t>({ 
        /* entity 0 */ 4, 0, 0, 0, 
        /* entity 1 */ 10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    REQUIRE(rd._entities == vector<size_t>({ 0, 4 }));

    my.a = 33;
    rd.AddComponent(0, sizeof my, 5, &my);
    REQUIRE(rd._ary == vector<uint8_t>({ 
        /* entity 0 */ 10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   5, 0,
        /* component 1:0 data*/  33, 0,
        /* entity 1 */ 10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    REQUIRE(rd._entities == vector<size_t>({ 0, 10 }));

    rd.InvalidateComponent(e, sizeof(MyComponent));
    REQUIRE(rd._ary == vector<uint8_t>({ 
        /* entity 0 */ 10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   5, 0,
        /* component 1:0 data*/  33, 0,
        /* entity 1 */ 10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   0xFF, 0xFF,
        /* component 1:0 data*/  42, 0 }));
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
