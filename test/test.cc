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

        size_t AddEntity(Entity** entity) {
        }

        Entity* GetEntity(size_t id) {
        }

        void InvalidateEntity(Entity* entity) {
        }

        void RemoveIdentity(Entity* entity) {
        }

    private:
        std::vector<size_t> _entities = {};
        std::vector<uint8_t> _mega_array = {};
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
    RawData::Entity* entity;
    size_t n = rd.AddEntity(&entity);
    REQUIRE(n == 0);

}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
