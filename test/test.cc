#include <vector>
using namespace std;

#include <gtest/gtest.h>

#include "fastecs.hh"

namespace ECS {

class RawTest : public ::testing::Test {
protected:
    using RawData = ECS::Engine::RawData<>;
    RawData rd = {};

public:
    size_t e1 = 0, e2 = 0;

protected:
    RawTest() : e1(rd.AddEntity()), e2(rd.AddEntity()) {}
};

TEST_F(RawTest, AddEntity) {
    ASSERT_EQ(e1, 0);
    ASSERT_EQ(rd.GetEntitySize(e1), 4);

    ASSERT_EQ(e2, 1);
    ASSERT_EQ(rd.GetEntitySize(e2), 4);

    ASSERT_EQ(rd._entities, vector<size_t>({ 0, 4 }));
    ASSERT_EQ(rd._ary, vector<uint8_t>({ 4, 0, 0, 0, 4, 0, 0, 0 }));

    ASSERT_EQ(rd.IsEntityValid(e1), true);
}

TEST_F(RawTest, AddComponents) {
    struct MyComponent {
        uint16_t a;
    };
    MyComponent my = { 42 };
    rd.AddComponent(e2, sizeof my, 7, &my);
    ASSERT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    ASSERT_EQ(rd._entities, vector<size_t>({ 0, 4 }));

    my.a = 33;
    rd.AddComponent(e1, sizeof my, 5, &my);
    ASSERT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   5, 0,
        /* component 1:0 data*/  33, 0,
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   7, 0,
        /* component 1:0 data*/  42, 0 }));
    ASSERT_EQ(rd._entities, vector<size_t>({ 0, 10 }));
    
    struct MySecondComponent {
        uint8_t b;
    };
    MySecondComponent my2 = { 13 };
    rd.AddComponent(e1, sizeof my2, 2, &my2);
    ASSERT_EQ(rd._ary, vector<uint8_t>({ 
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
    ASSERT_EQ(rd._entities, vector<size_t>({ 0, 15 }));
}


bool destroyed = false;
TEST_F(RawTest, RemoveComponents) {
    // add two components, to both entites
    struct MyComponent {
        uint16_t a;
        ~MyComponent() { destroyed = true; }
    };
    MyComponent my = { 42 };
    rd.AddComponent(e2, sizeof my, 7, &my);

    auto destructor = [](void* my) { static_cast<MyComponent*>(my)->~MyComponent(); };

    rd.InvalidateComponent(e2, 7, destructor);
    ASSERT_EQ(rd._ary, vector<uint8_t>({ 
        /* entity 0 */           4, 0, 0, 0, 
        /* entity 1 */           10, 0, 0, 0, 
        /* component 1:0 size */ 2, 0,
        /* component 1:0 id */   0xFF, 0xFF,
        /* component 1:0 data*/  42, 0 }));
    ASSERT_EQ(destroyed, true);
}


}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
