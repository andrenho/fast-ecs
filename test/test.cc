#include <cstdint>
#include <cstdlib>
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

        // will always add an entity to the end of the vector
        size_t AddEntity() {
            _entities.push_back(_ary.size());
            entity_size_t sz = static_cast<entity_size_t>(sizeof(entity_size_t));
            std::copy(&sz, &sz + sizeof(entity_size_t), std::back_inserter(_ary));
            return _entities.size()-1;
        }

        // will try to reuse an entity
        size_t AddEntity(size_t expected_size) {
        }

        Entity* GetEntity(size_t entity) {
            return reinterpret_cast<Entity*>(&_ary[entity]);
        }

        void InvalidateEntity(Entity* entity) {
        }

        void RemoveIdentity(Entity* entity) {
        }

    private:
        std::vector<size_t> _entities = {};
        std::vector<uint8_t> _ary = {};
    };

};

}  // namespace ECS

#undef private

//-----------------------------------------------------

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE("Manage raw entities", "[entities]") {

    using RawData = ECS::Engine::RawData<>;
    RawData rd;
    size_t e = rd.AddEntity();
    REQUIRE(e == 0);

    RawData::Entity* ent = rd.GetEntity(e);
    REQUIRE(ent->sz == 4);
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
