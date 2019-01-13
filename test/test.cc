#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "fastecs.hh"

using namespace std;
using namespace ecs;

TEST_CASE("entities") { 
    // {{{...
    
    struct C {};
    Engine<NoSystem, NoGlobal, NoEventQueue, C> e;

    CHECK(e.number_of_entities() == 0);

    Entity id = e.add_entity();
    CHECK(id.get() == 0);
    CHECK(e.number_of_entities() == 1);

    Entity id2 = e.add_entity("test");
    CHECK(id2.get() == 1);
    CHECK(e.number_of_entities() == 2);

    SUBCASE("entity active")  {
        CHECK(e.is_entity_active("test"));
        e.set_entity_active("test", false);
        CHECK(!e.is_entity_active(id2));
        CHECK(!e.is_entity_active("test"));
        CHECK(e.is_entity_active(id));
    }

    SUBCASE("debugging info") {
        e.set_entity_debugging_info(id, "debugging_info");
        CHECK(e.entity_debugging_info(id).value() == "debugging_info");
        CHECK(!e.entity_debugging_info(id2));
    }

    SUBCASE("remove entity") {
        e.remove_entity(id);
        CHECK(e.number_of_entities() == 1);

        CHECK_THROWS(e.is_entity_active(id));
        CHECK_THROWS(e.set_entity_active(id, false));
        CHECK_THROWS(e.set_entity_debugging_info(id, ""));
        CHECK_THROWS(e.entity_debugging_info(id));
        CHECK_THROWS(e.remove_entity(id));
    }

    // }}}
}

TEST_CASE("components") {
    // {{{ ...

    struct A { int x; };
    struct B { string y; };

    Engine<NoSystem, NoGlobal, NoEventQueue, A, B> e;
    Entity id = e.add_entity(),
           id2 = e.add_entity(),
           id3 = e.add_entity(),
           id4 = e.add_entity();

    A& a = e.add_component<A>(id, 42);
    CHECK(a.x == 42);
    e.add_component<B>(id, "hello");
    CHECK(e.add_component<A>(id3, 44).x == 44);
    CHECK(e.add_component<A>(id2, 43).x == 43);

    SUBCASE("add component") {
        CHECK_THROWS(e.add_component<A>(id, 43));

        CHECK(e.components_are_sorted<A>());
    }

    SUBCASE("component_ptr") {
        CHECK(e.component_ptr<A>(id)->x == 42);
        CHECK(e.component_ptr<A>(id2)->x == 43);
        CHECK(e.component_ptr<A>(id3)->x == 44);

        CHECK(!e.component_ptr<A>(id4));
    }

    SUBCASE("component / has_component") {
        CHECK(e.has_component<A>(id));
        CHECK(e.component<A>(id).x == 42);
        CHECK(!e.has_component<A>(id4));
        CHECK_THROWS(e.component<A>(id4));
    }

    SUBCASE("is string still valid?") {
        CHECK(e.component<B>(id).y == "hello");
    }

    SUBCASE("remove component") {
        e.remove_component<A>(id);
        CHECK_THROWS(e.remove_component<A>(id));
        CHECK(e.component<B>(id).y == "hello");
        CHECK(e.component<A>(id2).x == 43);
        CHECK_THROWS(e.component<A>(id));
    }
    
    SUBCASE("remove entity") {
        e.remove_entity(id);
        CHECK_THROWS(e.component<A>(id));
        CHECK_THROWS(e.component<B>(id));
    }

    // }}}
}

TEST_CASE("iteration") {
    // {{{ ...

    struct A { int x; };
    struct B { string y; };

    using MyEngine = Engine<NoSystem, NoGlobal, NoEventQueue, A, B>;
    MyEngine e;
    Entity id = e.add_entity();

    e.add_component<A>(id, 42);
    e.add_component<B>(id, "hello");

    Entity id2 = e.add_entity(),
           id3 = e.add_entity();
    e.add_component<A>(id2, 43);
    e.add_component<B>(id3, "world");

    SUBCASE("iterate one component") {
        int sum = 0;
        e.for_each<A>([&sum](MyEngine&, Entity const&, A& a) {
            sum += a.x;
        });
        CHECK(sum == (42 + 43));

        std::string s;

        e.for_each<B>([&s](MyEngine&, Entity const&, B& b) {
            s += b.y;
        });
        CHECK(s == "helloworld");
    }

    SUBCASE("itereate more than one compoenent") {
        std::string s = "";
        int sum = 0;
        e.for_each<A, B>([&sum, &s](MyEngine&, Entity const&, A& a, B& b) {
            sum += a.x;
            s += b.y;
        });
        CHECK(sum == 42);
        CHECK(s == "hello");
    }

    SUBCASE("const iteration") {
        MyEngine const& ce = e;
        std::string s = "";
        int sum = 0;
        ce.for_each<A, B>([&sum, &s](MyEngine const&, Entity const&, A const& a, B const& b) {
            sum += a.x;
            s += b.y;
        });
        CHECK(sum == 42);
        CHECK(s == "hello");
    }

    SUBCASE("activate/deactivate") {
        int sum = 0;
        e.for_each<A>([&sum](MyEngine&, Entity const&, A& a) { sum += a.x; });
        CHECK(sum == (42 + 43));

        e.set_entity_active(id, false);
        sum = 0;
        e.for_each<A>([&sum](MyEngine&, Entity const&, A& a) { sum += a.x; });
        CHECK(sum == 43);

        sum = 0;
        e.for_each<A>([&sum](MyEngine&, Entity const&, A& a) { sum += a.x; }, true);
        CHECK(sum == (42 + 43));

        e.set_entity_active(id, true);
        sum = 0;
        e.for_each<A>([&sum](MyEngine&, Entity const&, A& a) { sum += a.x; });
        CHECK(sum == (42 + 43));
    }

    // }}}
}

TEST_CASE("engine copyable") {
    // {{{ ...

    struct A { int x; };
    struct B { string y; };

    using MyEngine = Engine<NoSystem, NoGlobal, NoEventQueue, A, B>;
    MyEngine e;
    Entity id = e.add_entity();

    e.add_component<A>(id, 42);
    e.add_component<B>(id, "hello");

    MyEngine b = e;
    
    SUBCASE("copy successful") {
        CHECK(b.component<A>(id).x == 42);
        CHECK(b.component<B>(id).y == "hello");
    }

    // }}}
}

TEST_CASE("systems") {
    // {{{ ...
    class System {
    public:
        virtual ~System() {}
    };

    struct C {};
    using MyEngine = Engine<System, NoGlobal, NoEventQueue, C>;
    MyEngine e;

    struct TestSystem : public System {
        TestSystem(int i) : i(i) {}
        void Execute(MyEngine&) { CHECK(i == 2); }
        int i;
    };

    SUBCASE("add system") {   
        e.add_system<TestSystem>(2);
        CHECK(e.system<TestSystem>().i == 2);
        CHECK(e.systems().size() == 1); 
        CHECK_THROWS(e.add_system<TestSystem>(2));
    }
    
    SUBCASE("remove system") {
        e.remove_system<TestSystem>();
        CHECK(e.systems().size() == 0); 
        CHECK_NOTHROW(e.add_system<TestSystem>(2));
    }

    // }}}
}

TEST_CASE("global") {
    // {{{ ...

    struct Global {
        int x = 42;
    };

    struct C {};
    using MyEngine = Engine<NoSystem, Global, NoEventQueue, C>;
    MyEngine e;

    CHECK(e.global().x == 42);
    e.global().x = 24;
    CHECK(e.global().x == 24);

    // }}}
}

TEST_CASE("events") {
    // {{{ ...

    struct EventTypeA { size_t id; };
    struct EventTypeB { string abc; };
    using Event = variant<EventTypeA, EventTypeB>;

    struct C {};
    using MyEngine = Engine<NoSystem, NoGlobal, Event, C>;
    MyEngine e;

    e.send_event(EventTypeA { 12 });
    e.send_event(EventTypeA { 24 });
    e.send_event(EventTypeB { "Hello" });
    CHECK(e.event_queue<EventTypeA>().size() == 2);
    CHECK(e.event_queue<EventTypeA>().at(0).id == 12);
    CHECK(e.event_queue<EventTypeA>().at(1).id == 24);
    CHECK(e.event_queue<EventTypeA>().at(1).id == 24);
    CHECK(e.event_queue<EventTypeB>().at(0).abc == "Hello");
    e.clear_queue();
    CHECK(e.event_queue<EventTypeA>().empty());
    CHECK(e.event_queue<EventTypeB>().empty());

    // }}}
}

class System {
public:
    virtual ~System() {}
};

struct TestSystem : public System {
    TestSystem(int i) : i(i) {}
    int i;
};

struct Global {
    int x = 42;
};
inline ostream& operator<<(ostream& os, Global const& g) { os << "x = " << g.x; return os; }

struct EventTypeA { size_t id; };
struct EventTypeB { string abc; };
using Event = variant<EventTypeA, EventTypeB>;

struct A { 
    int x;
};
inline ostream& operator<<(ostream& os, A const& a) { os << "x = " << a.x; return os; }

struct B { string y; };
inline ostream& operator<<(ostream& os, B const& b) { os << "y = '" << b.y << "'"; return os; }

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

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
