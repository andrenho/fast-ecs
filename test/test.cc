#include "fastecs.hh"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

using namespace std;
using namespace ecs;

// {{{ helper structs

struct Position {
    int x, y;
    Position(Position const& pos) : x(pos.x), y(pos.y) {
        cout << "Position copied.\n";
    }

    Position(int x, int y) : x(x), y(y) {}
};

struct Direction {
    std::string dir;
};

// }}}

/*
TEST_CASE("entities") { 
    // {{{...

    enum class Pool : int { My };
    
    struct C {};
    ECS<NoGlobal, NoEventQueue, Pool, C> ecs(Threading::Single);

    auto e1 = ecs.add();
    CHECK(e1.id == 0);

    auto e2 = ecs.add();
    CHECK(e2.id == 1);
    
    Entity e3 = ecs.add(Pool::My);
    CHECK(e3.id == 2);

    size_t count = 0;
    for (auto const& e : ecs.entities()) {
        CHECK((e == e1 || e == e2 || e == e3));
        ++count;
    }
    CHECK(count == 3);

    count = 0;
    for (auto const& e : ecs.entities(Pool::My)) {
        CHECK((e != e1 && e != e2 && e == e3));
        ++count;
    }
    CHECK(count == 1);

    ecs.remove(e1);

    count = 0;
    for (auto const& e : ecs.entities()) {
        CHECK((e != e1 && (e == e2 || e == e3)));
        ++count;
    }
    CHECK(count == 2);

    // }}}
}
*/

TEST_CASE("components") {
    // {{{ ...

    ECS<NoGlobal, NoEventQueue, NoPool, Position, Direction> ecs(Threading::Single);

    // set component
    Entity e1 = ecs.add();
    e1.add<Position>(4, 5);
    e1.add<Direction>("N");
    CHECK(e1.get<Position>().x == 4);
    CHECK(e1.get<Position>().y == 5);

    // set component values
    e1.get<Position>().y = 10;
    CHECK(e1.get<Position>().y == 10);

    // set component values (pointer)
    e1.get_ptr<Position>()->y = 20;
    CHECK(e1.get_ptr<Position>()->y == 20);

    // has component
    Entity e2 = ecs.add();
    CHECK(e1.has<Position>());
    CHECK(!e2.has<Position>());

    // remove component
    e1.remove<Position>();
    CHECK(!e1.has<Position>());

    // remove entity
    ecs.remove(e1);
    CHECK_THROWS(e1.get<Position>());

    // }}}
}

TEST_CASE("iterate components") {
    // {{{ ...

    enum class Pool { My };
    using MyECS = ECS<NoGlobal, NoEventQueue, Pool, Position, Direction>;
    MyECS ecs(Threading::Single);

    Entity e1 = ecs.add();
    e1.add<Position>(34, 10);
    e1.add<Direction>("N");
    
    Entity e2 = ecs.add(Pool::My);
    e2.add<Position>(12, 20);

    size_t count = 0;
    for (auto const& e : ecs.entities<Position>()) {
        CHECK((e == e1 || e == e2));
        ++count;
    }
    CHECK(count == 2);

    count = 0;
    for (auto const& e : ecs.entities<Position>(Pool::My)) {
        CHECK(e == e2);
        ++count;
    }
    CHECK(count == 1);

    count = 0;
    for (auto const& e : ecs.entities<Position, Direction>()) {
        CHECK(e == e1);
        ++count;
    }
    CHECK(count == 1);

    // const iteration
    const MyECS &ecs_const = ecs;

    count = 0;
    for ([[maybe_unused]] auto const& _ : ecs_const.entities())
        ++count;
    CHECK(count == 2);
    
    count = 0;
    for (auto const& e : ecs_const.entities<Position>(Pool::My)) {
        CHECK(e == e2);
        ++count;
    }
    CHECK(count == 1);
    
    // for (auto& e : ecs_const.entities<Position>(Pool::My)) { e.add(Position {0,0}); }   // will give an error

    // }}}
}

TEST_CASE("globals") {
    // {{{ ...

    struct Global {
        int x = 42;
    };

    struct C {};
    ECS<Global, NoEventQueue, NoPool, C> ecs(Threading::Single);

    CHECK(ecs().x == 42);
    ecs().x = 24;
    CHECK(ecs().x == 24);

    // }}}
}

TEST_CASE("messages") {
    // {{{ ...
    struct EventTypeA { size_t id; };
    struct EventTypeB { string abc; };
    using Event = variant<EventTypeA, EventTypeB>;

    struct C {};
    ECS<NoGlobal, Event, NoPool, C> ecs(Threading::Single);

    ecs.add_message(EventTypeA { 12 });
    ecs.add_message(EventTypeA { 24 });
    ecs.add_message(EventTypeB { "Hello" });
    CHECK(ecs.messages<EventTypeA>().size() == 2);
    CHECK(ecs.messages<EventTypeA>().at(0).id == 12);
    CHECK(ecs.messages<EventTypeA>().at(1).id == 24);
    CHECK(ecs.messages<EventTypeA>().at(1).id == 24);
    CHECK(ecs.messages<EventTypeB>().at(0).abc == "Hello");

    ecs.clear_messages();
    CHECK(ecs.messages<EventTypeA>().empty());
    CHECK(ecs.messages<EventTypeB>().empty());
    // }}}
}

// {{{ helper for systems

struct C { int value = 0; };
using MyECS = ECS<NoGlobal, NoEventQueue, NoPool, C>;

static void my_add(MyECS const&, int& x) {
    ++x;
}

static void change_c(MyECS& ecs) {
    for (auto& e : ecs.entities<C>())
        ++e.get<C>().value;
}

// }}}

TEST_CASE("systems") {
    // {{{ ...
    
    MyECS ecs(Threading::Multi);

    Entity e1 = ecs.add();
    e1.add<C>();

    struct Adder {
        int x = 0;
        void internal_add(MyECS const&) {
            ++this->x;
        }
    };

    ecs.start_frame();
    
    // single threaded
    
    int x = 0;
    ecs.run_st("my_add", my_add, x);
    CHECK(x == 1);
    auto timer = ecs.timer_st();
    CHECK(timer.find("my_add") != timer.end());

    Adder adder;
    ecs.run_st("internal_add", adder, &Adder::internal_add);
    CHECK(adder.x == 1);

    // ecs.run_st("change_c", change_c);   // this line should give an error

    // mutable

    ecs.run_mutable("change_c", change_c);
    CHECK(e1.get<C>().value == 1);

    // multithreaded

    struct Wait {
        static void add(MyECS const&, int thnum, int* x) {
            for (int i=0; i < 20; ++i) {
                ++(*x);
                cout << "Thread " << thnum << ": " << *x << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

    ecs.reset_timer();
    ecs.start_frame();
    int x1 = 0, x2 = 0;
    ecs.run_mt("wait1", Wait::add, 1, &x1);
    ecs.run_mt("wait2", Wait::add, 2, &x2);
    ecs.join();
    CHECK(x1 > 0);
    CHECK(x2 > 0);

    auto timer_mt = ecs.timer_mt();
    CHECK(timer_mt.at("wait1") > std::chrono::milliseconds(0));
    CHECK(timer_mt.at("wait2") > std::chrono::milliseconds(0));

    // }}}
}

/*  TODO

TEST_CASE("debugging") {
    // {{{ ...

    using MyEngine = Engine<System, Global, Event, A, B>;
    MyEngine e;

    Entity id = e.add_entity();
    e.add_component<A>(id, 24);
    e.add_component<B>(id, "hello");

    e.add_entity("myent");
    e.add_component<A>("myent", 24);
    e.set_entity_debugging_info("myent", "Debugging info");

    e.add_system<TestSystem>(2);

    e.send_event(EventTypeA { 10 });

    SUBCASE("debug") {
        cout << e.debug_all() << "\n";
    }

    SUBCASE("info") {
        CHECK(e.number_of_entities() == 2);
        CHECK(e.number_of_components() == 2);
        CHECK(e.number_of_systems() == 1);
        CHECK(e.number_of_event_types() == 2);
        CHECK(e.event_queue_size() == 1);

        Engine<System, Global, NoEventQueue, A, B> e2;
        CHECK(e2.number_of_event_types() == 0);
    }

    // }}}
}

// uncomment one of the following commented lines on order to have a static error:

struct GlobalNDC { GlobalNDC(int) {} };
// Engine<NoSystem, GlobalNDC, NoEventQueue, A> e1;

struct D { D(D const&) = delete; };
// Engine<NoSystem, NoGlobal, NoEventQueue, A, D> e2;

// Engine<NoSystem, NoGlobal, int, A> e3;

// int test() { Engine<NoSystem, NoGlobal, NoEventQueue, A> e; e.add_component<B>(ecs::Entity { 0 }); };

*/

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
