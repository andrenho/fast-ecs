#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>
using namespace std;

#include "fastecs.hh"

namespace ECS {

// {{{ TEST RAWDATA

bool destroyed = false;

class RawTest : public ::testing::Test {
protected:
    using RawData = ECS::Engine<int,ECS::NoGlobal,ECS::NoQueue,int>::RawData<int32_t, uint16_t, uint16_t>;
    RawData rd = {};

public:
    size_t e1 = 0, e2 = 0;

protected:
    RawTest() : e1(rd.add_entity()), e2(rd.add_entity()) {}
};


TEST_F(RawTest, add_entity) {
    EXPECT_EQ(e1, 0);
    EXPECT_EQ(rd.entity_size(e1), 4);

    EXPECT_EQ(e2, 1);
    EXPECT_EQ(rd.entity_size(e2), 4);

    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 4 }));
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 4, 0, 0, 0, 4, 0, 0, 0 }));

    EXPECT_EQ(rd.is_entity_valid(e1), true);
}

TEST_F(RawTest, add_components) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.add_component(e2, sizeof my, 7, &my);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 4 }));

    my.a = 33;
    rd.add_component(e1, sizeof my, 5, &my);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   5, 0,
        /* component 1:0 data*/  33, 0,
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 10 }));
    
    struct MySecondComponent {
        uint8_t b;
    };
    MySecondComponent my2 = { 13 };
    rd.add_component(e1, sizeof my2, 2, &my2);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           15, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   5, 0,
        /* component 1:0 data*/  33, 0,
        /* component 1:1 size */ 1, 0,
        /* component 1:1 id */   2, 0,
        /* component 1:1 data*/  13,
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 15 }));
}


TEST_F(RawTest, invalidate_components) {
    // add two components, to both entites
    struct MyComponent {
        uint16_t a;
        ~MyComponent() { destroyed = true; }
    };
    MyComponent my = { 42 };
    rd.add_component(e2, sizeof my, 7, &my);
    rd.add_component(e2, sizeof my, 6, &my);

    auto destructor = [](void* my) { static_cast<MyComponent*>(my)->~MyComponent(); };

    rd.invalidate_component(e2, 7, destructor);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           16, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   0xFF, 0xFF,
        /* component 1:0 data*/  42, 0,
        /* component 1:1 size */ 2, 0,
        /* component 1:1 id */   6, 0,
        /* component 1:1 data*/  42, 0 }));
    EXPECT_EQ(destroyed, true) << "destructor";

    my.a = 52;
    rd.add_component(e2, sizeof my, 4, &my);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           16, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   4, 0,
        /* component 1:0 data*/  52, 0,
        /* component 1:1 size */ 2, 0,
        /* component 1:1 id */   6, 0,
        /* component 1:1 data*/  42, 0 })) << "reuse component";
}

TEST_F(RawTest, InvalidateEntities) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.add_component(e1, sizeof my, 1, &my);
    rd.add_component(e1, sizeof my, 2, &my);
    rd.add_component(e2, sizeof my, 3, &my);

    // sanity check
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           16, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   1, 0,
        /* component 1:0 data*/  42, 0,
        /* component 1:1 size */ 2, 0,
        /* component 1:1 id */   2, 0,
        /* component 1:1 data*/  42, 0,
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   3, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 16 }));

    rd.invalidate_entity(e1);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           0xF0, 0xFF, 0xFF, 0xFF,  /* -16 */
        /* component 1:0 size */ 0xFF, 0xFF,
        /* component 1:0 id */   0xFF, 0xFF,
        /* component 1:0 data*/  0xFF, 0xFF,
        /* component 1:1 size */ 0xFF, 0xFF,
        /* component 1:1 id */   0xFF, 0xFF,
        /* component 1:1 data*/  0xFF, 0xFF,
        /* entity 1 */           10, 0, 0, 0,
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   3, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ rd.INVALIDATED_ENTITY, 16 }));

    EXPECT_ANY_THROW(rd.add_component(e1, sizeof my, 1, &my));
}


TEST_F(RawTest, compress) {
    size_t e3 = rd.add_entity();
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.add_component(e1, sizeof my, 1, &my);
    rd.add_component(e1, sizeof my, 2, &my);
    rd.add_component(e2, sizeof my, 3, &my);
    rd.add_component(e3, sizeof my, 4, &my);

    rd.invalidate_entity(e1);
    rd.invalidate_component(e2, 3, [](void*){});
    rd.compress();
    
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 1 */           4, 0, 0, 0,
        /* entity 2 */           10, 0, 0, 0,
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   4, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ rd.INVALIDATED_ENTITY, 0, 4, }));
}


TEST_F(RawTest, DifferentSizes) {
    ECS::Engine<int,ECS::NoGlobal,ECS::NoQueue,int>::RawData<int8_t, uint8_t, uint8_t> rd2;

    e1 = rd2.add_entity();
    e2 = rd2.add_entity();

    struct MyComponent {
        uint8_t a;
    };
    MyComponent my = { 42 };
    rd2.add_component(e2, sizeof my, 7, &my);

    EXPECT_EQ(rd2._ary, vector<uint8_t>({ 
        /* entity 0 */           1,
        /* entity 1 */           4,
        /* component 1:0 size */ 1,
        /* component 1:0 id */   7,
        /* component 1:0 data*/  42, }));
    EXPECT_EQ(rd2._entities, vector<size_t>({ 0, 1 }));
}


TEST_F(RawTest, InvalidSizes) {
    ECS::Engine<int,ECS::NoGlobal,ECS::NoQueue,int>::RawData<int16_t, uint8_t, uint8_t> rd2;
    e1 = rd2.add_entity();

    /*
    // ID too large - can't logically happen because it overflows the ID type
    struct Small {
        uint8_t a;
    };
    Small small = { 42 };
    EXPECT_ANY_THROW(rd2.add_component(e1, sizeof small, 300, &small));

    // component too large - can't logically happen because it overflows the size type
    struct Large {
        uint8_t big[300];
    };
    Large large;
    EXPECT_ANY_THROW(rd2.add_component(e1, sizeof large, 1, &large));
    */

    // entity too large
    ECS::Engine<int,ECS::NoGlobal,ECS::NoQueue,int>::RawData<int8_t, uint8_t, uint8_t> rd3;
    e1 = rd3.add_entity();
    struct Medium {
        uint8_t medium[100];
    };
    Medium medium;
    rd3.add_component(e1, sizeof medium, 1, &medium);
    EXPECT_ANY_THROW(rd3.add_component(e1, sizeof medium, 2, &medium));

    // entity does not exist
    EXPECT_ANY_THROW(rd3.add_component(255, sizeof medium, 1, &medium));
}


TEST_F(RawTest, IterateConst) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.add_component(e1, sizeof my, 7, &my);

    // non-const
    rd.for_each_entity([](size_t entity, uint8_t const*) { EXPECT_EQ(entity, 0); return true; });
    rd.for_each_component_in_entity(rd.entity_ptr(e1), [](decltype(rd)::Component* c, void* data, int32_t) {
        EXPECT_EQ(c->id, 7);
        MyComponent* my = reinterpret_cast<MyComponent*>(data);
        EXPECT_EQ(my->a, 42);
        return true;
    });

    // const
    ECS::Engine<int,ECS::NoGlobal,ECS::NoQueue,int>::RawData<int32_t, uint16_t, uint16_t> const& rdc = rd;
    rdc.for_each_entity([](size_t entity, uint8_t const*) { EXPECT_EQ(entity, 0); return true; });
    rdc.for_each_component_in_entity(rdc.entity_ptr(e1), [](decltype(rd)::Component const* c, void const* data, int32_t) {
        EXPECT_EQ(c->id, 7);
        MyComponent const* my = reinterpret_cast<MyComponent const*>(data);
        EXPECT_EQ(my->a, 42);
        return true;
    });
}

// }}}

//------------------------------------------------------------------------

struct EventTypeA { size_t id; };
struct EventTypeB { string abc; };
using Event = variant<EventTypeA, EventTypeB>;

class System {
public:
    virtual ~System() {}
};

int destroy_count = 0;

class EngineTest : public ::testing::Test {
protected:
    EngineTest() : e1(e.add_entity()), e2(e.add_entity()) {
        e.add_component<Position>(e1, 40.f, 50.f);
        e.add_component<Direction>(e1, 60.f);

        e.add_component<Direction>(e2, 70.f);
    }

public:
    struct Global {
        int x = 42;
        friend ostream& operator<<(ostream& out, Global const& global);
    };

    struct Position {
        float x, y;
        Position(float x, float y) : x(x), y(y) {}
        friend ostream& operator<<(ostream& out, Position const& pos);
    };

    struct Direction {
        float angle;
        Direction(float angle) : angle(angle) {}
    };

    struct Destructable {
        Destructable()  { ++destroy_count; }
        ~Destructable() { --destroy_count; }
    };

    struct Leak {
        struct X { int i; };
        map<string, vector<X>> text {};
    };

    using MyEngine = ECS::Engine<System, Global, Event, Position, Direction, Destructable, Leak>;
    MyEngine e = {};

    size_t e1, e2;
};

ostream& operator<<(ostream& out, EngineTest::Position const& pos) {
    out << "'Position': [ " << pos.x << ", " << pos.y << " ]";
    return out;
}

ostream& operator<<(ostream& out, EngineTest::Global const& global) {
    out << "'x' : " << global.x;
    return out;
}


TEST_F(EngineTest, Read) {
    EXPECT_TRUE(e.has_component<Position>(e1));
    EXPECT_TRUE(e.has_component<Direction>(e1));
    EXPECT_FALSE(e.has_component<Position>(e2));
    EXPECT_EQ(e.component_ptr<Position>(e2), nullptr);
    EXPECT_TRUE(e.has_component<Direction>(e2));

    int i=0;
    e.for_each<Position, Direction>([&](size_t entity, Position& pos, Direction& dir) {
        EXPECT_EQ(entity, e1);
        EXPECT_FLOAT_EQ(pos.x, 40.f);
        EXPECT_FLOAT_EQ(pos.y, 50.f);
        EXPECT_FLOAT_EQ(dir.angle, 60.f);
        pos.x = 42.f;
        ++i;
    });
    EXPECT_EQ(i, 1);

    EXPECT_FLOAT_EQ(e.component<Position>(e1).x, 42.f);
    EXPECT_FLOAT_EQ(e.component<Position>(e1).y, 50.f);

    i=0;
    float sum=0;
    e.for_each<Direction>([&](size_t, Direction& dir) {
        sum += dir.angle;
        ++i;
    });
    EXPECT_EQ(i, 2);
    EXPECT_FLOAT_EQ(sum, 130.f);
}


TEST_F(EngineTest, Const) {
    MyEngine const& ce = e;

    EXPECT_TRUE(ce.has_component<Position>(e1));
    EXPECT_FLOAT_EQ(ce.component<Position>(e1).y, 50.f);

    int i=0;
    ce.for_each<Position, Direction>([&](size_t entity, Position const& pos, Direction const& dir) {
        EXPECT_EQ(entity, e1);
        EXPECT_FLOAT_EQ(pos.x, 40.f);
        EXPECT_FLOAT_EQ(pos.y, 50.f);
        EXPECT_FLOAT_EQ(dir.angle, 60.f);
        ++i;
    });
    EXPECT_EQ(i, 1);
}


TEST_F(EngineTest, ComponentRemoval) {
	destroy_count = 0;
	e.add_component<Destructable>(e1);
    e.remove_component<Destructable>(e1);
    EXPECT_EQ(destroy_count, 0);

    e.remove_component<Direction>(e1);

    // sanity check

    int i=0; e.for_each<Direction>([&](size_t, Direction&) { ++i; });
    EXPECT_EQ(i, 1);

    i=0; e.for_each<Destructable>([&](size_t, Destructable&) { ++i; });
    EXPECT_EQ(i, 0);

    e.compress();

    i=0; e.for_each<Direction>([&](size_t, Direction&) { ++i; });
    EXPECT_EQ(i, 1);

    i=0; e.for_each<Destructable>([&](size_t, Destructable&) { ++i; });
    EXPECT_EQ(i, 0);
}


TEST_F(EngineTest, EntityRemoval) {
    destroy_count = 0;
	e.add_component<Destructable>(e1);
    e.remove_entity(e1);
    EXPECT_EQ(destroy_count, 0);

    EXPECT_ANY_THROW(e.component<Position>(e1));
}


TEST_F(EngineTest, Systems) {
    struct TestSystem : public System {
        TestSystem(int i) : i(i) {}
        void Execute(MyEngine&) { EXPECT_EQ(i, 2); }
        int i;
    };
    e.add_system<TestSystem>(2);
    EXPECT_EQ(e.system<TestSystem>().i, 2);
    EXPECT_EQ(e.systems().size(), 1); 
}

// add same component, reuse deleted component
TEST_F(EngineTest, ReaddReuse) {
    EXPECT_ANY_THROW(e.add_component<Direction>(e1, 70.f));
    e.remove_component<Direction>(e1);
    EXPECT_ANY_THROW(e.remove_component<Direction>(e1));
}

TEST_F(EngineTest, Queue) {
    e.send(EventTypeA { 12 });
    e.send(EventTypeA { 24 });
    e.send(EventTypeB { "Hello" });
    EXPECT_EQ(e.events<EventTypeA>().size(), 2);
    EXPECT_EQ(e.events<EventTypeA>().at(0).id, 12);
    EXPECT_EQ(e.events<EventTypeA>().at(1).id, 24);
    EXPECT_EQ(e.events<EventTypeA>().at(1).id, 24);
    EXPECT_EQ(e.events<EventTypeB>().at(0).abc, "Hello");
    e.clear_queue();
    EXPECT_TRUE(e.events<EventTypeA>().empty());
    EXPECT_TRUE(e.events<EventTypeB>().empty());
}

TEST_F(EngineTest, Named) {
    size_t e3 = e.add_entity("test");
    EXPECT_EQ(e.entity("test"), e3);
    EXPECT_ANY_THROW(e.entity("abc"));
    e.add_component<Position>("test", 40.f, 50.f);
    EXPECT_TRUE(e.has_component<Position>("test"));
    EXPECT_TRUE(e.has_component<Position>(e3));
}

TEST_F(EngineTest, Global) {
    EXPECT_EQ(e.global().x, 42);
    e.global().x = 24;
    EXPECT_EQ(e.global().x, 24);
}

TEST_F(EngineTest, Leak) {
    size_t id = e.add_entity();
    e.add_component<Leak>(id, Leak {});

    e.for_each<Leak>([&](size_t, Leak& leak) {
        leak.text["hello"] = {};
        leak.text["hello"].push_back({ 42 });
    });
    e.remove_entity(id);
}

TEST_F(EngineTest, Debugging) {
    e.examine_global(cout);
    e.examine(cout);
}

}  // namespace ECS


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
