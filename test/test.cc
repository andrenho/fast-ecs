#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "fastecs.hh"

using namespace std;
using namespace ecs;

// {{{ helper structs

struct Position {
    double x, y;
};

struct Direction {
    std::string dir;
};

// }}}

TEST_CASE("entities") { 
    // {{{...

    enum class Pool : int { My };
    
    struct C {};
    ECS<NoGlobal, NoEventQueue, Pool, C> ecs(Threading::Single);

    CHECK(ecs.number_of_entities() == 0);

    Entity e1 = ecs.add();
    CHECK(e1.id == 0);
    CHECK(ecs.number_of_entities() == 1);

    Entity e2 = ecs.add();
    Entity e3 = ecs.add(Pool::My);

    size_t count = 0;
    for (Entity const& e : ecs.entities()) {
        CHECK((e == e1 || e == e2 || e == e3));
        ++count;
    }
    CHECK(count == 3);

    count = 0;
    for (Entity const& e : ecs.entities(Pool::My)) {
        CHECK((e != e1 && e != e2 && e == e3));
        ++count;
    }
    CHECK(count == 1);

    ecs.remove(e1);

    count = 0;
    for (Entity const& e : ecs.entities()) {
        CHECK((e != e1 && (e == e2 || e == e3)));
        ++count;
    }
    CHECK(count == 2);

    // }}}
}

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
    ECS<NoGlobal, NoEventQueue, Pool, Position, Direction> ecs(Threading::Single);

    Entity e1 = ecs.add();
    e1.add<Position>(34, 10);
    e1.add<Direction>("N");
    
    Entity e2 = ecs.add(Pool::My);
    e2.add<Position>(12, 20);

    size_t count = 0;
    for (Entity& e : ecs.entities<Position>()) {
        CHECK((e == e1 || e == e2));
        ++count;
    }
    CHECK(count == 2);

    count = 0;
    for (Entity& e : ecs.entities<Position>(Pool::My)) {
        CHECK(e == e2);
        ++count;
    }
    CHECK(count == 1);

    count = 0;
    for (Entity& e : ecs.entities<Position, Direction>()) {
        CHECK(e == e1);
        ++count;
    }
    CHECK(count == 2);

    // const iteration
    const ECS ecs_const = ecs;
    
    count = 0;
    for (Entity const& e : ecs_const.entities<Position>(Pool::My)) {
        CHECK(e == e2);
        ++count;
    }
    CHECK(count == 1);
    
    //for (Entity& e : ecs_const.entities<Position>(Pool::My)) {}   // will give an error

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

struct C { int value = 0; };
using MyECS = ECS<NoGlobal, NoEventQueue, NoPool, C>;

void my_add(MyECS const& ecs, int& x) {
    ++x;
}

void change_c(MyECS& ecs) {
    for (Entity& e : ecs.entities<C>())
        ++e.get<C>().value;
}

TEST_CASE("systems") {
    // {{{ ...
    
    MyECS ecs(Threading::Multi);

    Entity e1 = ecs.add();
    e1.add<C>();

    struct Adder {
        int x = 0;
        void internal_add(MyECS const& ecs) {
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
        static void add(MyECS const&, int& x) {
            for (int i=0; i < 20; ++i) {
                ++x;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    };

    ecs.reset_timer();
    ecs.start_frame();
    int x1 = 0, x2 = 0;
    ecs.run_mt("wait1", Wait::add, x1);
    ecs.run_mt("wait2", Wait::add, x2);
    ecs.join();
    CHECK(x1 > 0);
    CHECK(x2 > 0);

    auto timer_mt = ecs.timer_mt();
    CHECK(timer_mt.at("wait1") > 0);
    CHECK(timer_mt.at("wait2") > 0);

    // }}}
}

// TODO - debugging

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
