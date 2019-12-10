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

struct Global {
    int x = 42;
};
inline ostream& operator<<(ostream& os, Global const& g) { os << "x = " << g.x; return os; }

struct MessageTypeA { size_t id; };
struct MessageTypeB { string abc; };
using Message = variant<MessageTypeA, MessageTypeB>;

// }}}

TEST_CASE("entities") { 
    // {{{...

    enum class Pool : int { My };
    
    struct C {};
    ECS<NoGlobal, NoMessageQueue, Pool, C> ecs;

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

TEST_CASE("components") {
    // {{{ ...

    ECS<NoGlobal, NoMessageQueue, NoPool, Position, Direction> ecs;

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
    using MyECS = ECS<NoGlobal, NoMessageQueue, Pool, Position, Direction>;
    MyECS ecs;

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

    struct C {};
    ECS<Global, NoMessageQueue, NoPool, C> ecs;

    CHECK(ecs().x == 42);
    ecs().x = 24;
    CHECK(ecs().x == 24);

    // }}}
}

TEST_CASE("messages") {
    // {{{ ...
    struct C {};
    ECS<NoGlobal, Message, NoPool, C> ecs;

    ecs.add_message(MessageTypeA { 12 });
    ecs.add_message(MessageTypeA { 24 });
    ecs.add_message(MessageTypeB { "Hello" });
    CHECK(ecs.messages<MessageTypeA>().size() == 2);
    CHECK(ecs.messages<MessageTypeA>().at(0).id == 12);
    CHECK(ecs.messages<MessageTypeA>().at(1).id == 24);
    CHECK(ecs.messages<MessageTypeA>().at(1).id == 24);
    CHECK(ecs.messages<MessageTypeB>().at(0).abc == "Hello");

    ecs.clear_messages();
    CHECK(ecs.messages<MessageTypeA>().empty());
    CHECK(ecs.messages<MessageTypeB>().empty());
    // }}}
}

// {{{ helper for systems

struct C { int value = 0; };
using MyECS = ECS<NoGlobal, NoMessageQueue, NoPool, C>;

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
    
    MyECS ecs;

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
    CHECK(std::find_if(timer.begin(), timer.end(), [](SystemTime const& st) { return st.name == "my_add"; }) != timer.end());

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

    std::vector<SystemTime> timer_mt = ecs.timer_mt();
    CHECK(std::find_if(timer_mt.begin(), timer_mt.end(),
                [](SystemTime const& st) { return st.name == "wait1"; })->us > std::chrono::microseconds(0));
    //CHECK(std::find_if(timer_mt.begin(), timer_mt.end(),
    //            [](SystemTime const& st) { return st.name == "wait2"; })->us > std::chrono::microseconds(0));

    // }}}
}

// {{{ helper components

struct A { 
    int x;
};
inline ostream& operator<<(ostream& os, A const& a) { os << "x = " << a.x; return os; }

struct B { string y; };
inline ostream& operator<<(ostream& os, B const& b) { os << "y = '" << b.y << "'"; return os; }

// }}}

TEST_CASE("debugging") {
    // {{{ ...

    ECS<Global, Message, NoPool, A, B> ecs;

    auto e1 = ecs.add();
    e1.add<A>(24);
    e1.add<B>("hello");

    auto e2 = ecs.add();
    e2.add<A>(42);

    ecs.add_message(MessageTypeA { 10 });

    SUBCASE("debug") {
        cout << ecs.debug_all() << "\n";
    }

    SUBCASE("info") {
        CHECK(ecs.number_of_entities() == 2);
        CHECK(ecs.number_of_components() == 2);
        CHECK(ecs.number_of_message_types() == 2);
        CHECK(ecs.message_queue_size() == 1);

        ECS<Global, NoMessageQueue, NoPool, A, B> ecs2;
        CHECK(ecs2.number_of_message_types() == 0);
    }

    // }}}
}

// uncomment one of the following commented lines on order to have a static error:

struct GlobalNDC { GlobalNDC(int) {} };
// ECS<GlobalNDC, NoMessageQueue, NoPool, A> e1;

struct D { D(D const&) = delete; };
// ECS<NoGlobal, NoMessageQueue, NoPool, A, D> e1;

// ECS<NoGlobal, NoPool, int, A> e3;

// int test() { ECS<NoGlobal, NoMessageQueue, NoPool, A> e; e.add_component<B>(ecs::Entity { 0 }); };

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
