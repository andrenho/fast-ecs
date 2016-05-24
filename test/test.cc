#include <vector>
using namespace std;

#include <gtest/gtest.h>

#include "fastecs.hh"

namespace ECS {

// {{{ TEST RAWDATA

class RawTest : public ::testing::Test {
protected:
    using RawData = ECS::Engine<int,int>::RawData<int32_t, uint16_t, uint16_t>;
    RawData rd = {};

public:
    size_t e1 = 0, e2 = 0;

protected:
    RawTest() : e1(rd.AddEntity()), e2(rd.AddEntity()) {}
};


TEST_F(RawTest, AddEntity) {
    EXPECT_EQ(e1, 0);
    EXPECT_EQ(rd.GetEntitySize(e1), 4);

    EXPECT_EQ(e2, 1);
    EXPECT_EQ(rd.GetEntitySize(e2), 4);

    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 4 }));
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 4, 0, 0, 0, 4, 0, 0, 0 }));

    EXPECT_EQ(rd.IsEntityValid(e1), true);
}

TEST_F(RawTest, AddComponents) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.AddComponent(e2, sizeof my, 7, &my);
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ 0, 4 }));

    my.a = 33;
    rd.AddComponent(e1, sizeof my, 5, &my);
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
    rd.AddComponent(e1, sizeof my2, 2, &my2);
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


bool destroyed = false;
TEST_F(RawTest, InvalidateComponents) {
    // add two components, to both entites
    struct MyComponent {
        uint16_t a;
        ~MyComponent() { destroyed = true; }
    };
    MyComponent my = { 42 };
    rd.AddComponent(e2, sizeof my, 7, &my);
    rd.AddComponent(e2, sizeof my, 6, &my);

    auto destructor = [](void* my) { static_cast<MyComponent*>(my)->~MyComponent(); };

    rd.InvalidateComponent(e2, 7, destructor);
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
    rd.AddComponent(e2, sizeof my, 4, &my);
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
    rd.AddComponent(e1, sizeof my, 1, &my);
    rd.AddComponent(e1, sizeof my, 2, &my);
    rd.AddComponent(e2, sizeof my, 3, &my);

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

    rd.InvalidateEntity(e1);
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

    EXPECT_ANY_THROW(rd.AddComponent(e1, sizeof my, 1, &my));
}


TEST_F(RawTest, Compress) {
    size_t e3 = rd.AddEntity();
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.AddComponent(e1, sizeof my, 1, &my);
    rd.AddComponent(e1, sizeof my, 2, &my);
    rd.AddComponent(e2, sizeof my, 3, &my);
    rd.AddComponent(e3, sizeof my, 4, &my);

    rd.InvalidateEntity(e1);
    rd.InvalidateComponent(e2, 3, [](void*){});
    rd.Compress();
    
    EXPECT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 1 */           4, 0, 0, 0,
        /* entity 2 */           10, 0, 0, 0,
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   4, 0,
        /* component 1:0 data*/  42, 0 }));
    EXPECT_EQ(rd._entities, vector<size_t>({ rd.INVALIDATED_ENTITY, 0, 4, }));
}


TEST_F(RawTest, DifferentSizes) {
    ECS::Engine<int,int>::RawData<int8_t, uint8_t, uint8_t> rd2;

    e1 = rd2.AddEntity();
    e2 = rd2.AddEntity();

    struct MyComponent {
        uint8_t a;
    };
    MyComponent my = { 42 };
    rd2.AddComponent(e2, sizeof my, 7, &my);

    EXPECT_EQ(rd2._ary, vector<uint8_t>({ 
        /* entity 0 */           1,
        /* entity 1 */           4,
        /* component 1:0 size */ 1,
        /* component 1:0 id */   7,
        /* component 1:0 data*/  42, }));
    EXPECT_EQ(rd2._entities, vector<size_t>({ 0, 1 }));
}


TEST_F(RawTest, InvalidSizes) {
    ECS::Engine<int,int>::RawData<int16_t, uint8_t, uint8_t> rd2;
    e1 = rd2.AddEntity();

    /*
    // ID too large - can't logically happen because it overflows the ID type
    struct Small {
        uint8_t a;
    };
    Small small = { 42 };
    EXPECT_ANY_THROW(rd2.AddComponent(e1, sizeof small, 300, &small));

    // component too large - can't logically happen because it overflows the size type
    struct Large {
        uint8_t big[300];
    };
    Large large;
    EXPECT_ANY_THROW(rd2.AddComponent(e1, sizeof large, 1, &large));
    */

    // entity too large
    ECS::Engine<int,int>::RawData<int8_t, uint8_t, uint8_t> rd3;
    e1 = rd3.AddEntity();
    struct Medium {
        uint8_t medium[100];
    };
    Medium medium;
    rd3.AddComponent(e1, sizeof medium, 1, &medium);
    EXPECT_ANY_THROW(rd3.AddComponent(e1, sizeof medium, 2, &medium));

    // entity does not exist
    EXPECT_ANY_THROW(rd3.AddComponent(255, sizeof medium, 1, &medium));
}


TEST_F(RawTest, IterateConst) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.AddComponent(e1, sizeof my, 7, &my);

    // non-const
    rd.ForEachEntity([](size_t entity, uint8_t const*) { EXPECT_EQ(entity, 0); return true; });
    rd.ForEachComponentInEntity(rd.GetEntityPtr(e1), [](decltype(rd)::Component* c, void* data, int32_t) {
        EXPECT_EQ(c->id, 7);
        MyComponent* my = reinterpret_cast<MyComponent*>(data);
        EXPECT_EQ(my->a, 42);
        return true;
    });

    // const
    ECS::Engine<int,int>::RawData<int32_t, uint16_t, uint16_t> const& rdc = rd;
    rdc.ForEachEntity([](size_t entity, uint8_t const*) { EXPECT_EQ(entity, 0); return true; });
    rdc.ForEachComponentInEntity(rdc.GetEntityPtr(e1), [](decltype(rd)::Component const* c, void const* data, int32_t) {
        EXPECT_EQ(c->id, 7);
        MyComponent const* my = reinterpret_cast<MyComponent const*>(data);
        EXPECT_EQ(my->a, 42);
        return true;
    });
}

// }}}

class EngineTest : public ::testing::Test {
protected:
    class System {
    };

    struct Position {
        float x, y;
        Position(float x, float y) : x(x), y(y) {}
    };

    struct Direction {
        float angle;
        Direction(float angle) : angle(angle) {}
    };

    using MyEngine = ECS::Engine<System, Position, Direction>;
    MyEngine e = {};
};

TEST_F(EngineTest, Add) {
    size_t e1 = e.AddEntity();
    e.AddComponent<Position>(e1, 40.f, 50.f);
    e.AddComponent<Direction>(e1, 60.f);
}

}  // namespace ECS


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
