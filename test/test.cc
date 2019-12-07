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

    enum class Pool { My };
    
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

TEST_CASE("systems") {
    // {{{ ...
    
    MyECS ecs(Threading::Single);

    Entity e1 = ecs.add();
    e1.add<C>();

    struct Adder {
        int x = 0;

        void internal_add(MyECS const& ecs) {
            ++this->x;
        }
    };

    ecs.start_frame();
    
    int x = 0;
    ecs.run_st(my_add, x);
    CHECK(x == 1);

    Adder adder;
    ecs.run_st(adder, &Adder::internal_add);

    // }}}
}

/*

-- {{{ systems
do
   ecs:start_frame()

   -- run singlethreaded
   local t = { value = 0 }
   ecs:run_st('adder 1', adder, t)
   assert(t.value == 1)

   -- check timer
   assert(ecs:timer()[1].name == 'adder 1')

   -- singlethread non-mutable
   function change_stuff()
      e1.comp.value = 2
   end
   assert_error(function() ecs:run_st('change_stuff', change_stuff) end)
   assert(e1.comp.value == 1)

   -- multithread
   function add_wait(t)
      for i = 1,20 do
         t.value = t.value + 1
         love.timer.sleep(0.001)
      end
   end
   local t1 = { value = 0 }
   local t2 = { value = 0 }

   ECS.multithreaded = false

   ecs:reset_timer()
   ecs:start_frame()
   ecs:run_mt('multithread', {
      { 'wait1', add_wait, t1 },
      { 'wait2', add_wait, t2 },
   })
   ecs:join()
   assert(t1.value > 0)
   assert(t2.value > 0)

   local timer = ecs:timer()
   assert(timer[1].name == 'wait1')
   assert(timer[1].time > 0)
   assert(timer[2].name == 'wait2')
   assert(timer[2].time > 0)

   -- mutable
   ecs:run_mutable('change_stuff', change_stuff)
   assert(e1.comp.value == 2)
end

-- }}}

-- {{{ print components, values

do
   local ecs = ECS({
      position = {
         x = 'number',
         y = 'number?'
      },
      direction = {
         dir = 'string'
      }
   }, false)
   
   -- set component
   local e1 = ecs:add()
   e1.position = { x = 34, y = 10 }
   e1.direction = { dir = 'N' }

   p(e1.position)
   p(e1)

   ecs.test = { hello = 'world' }
   ecs.value = 42
   p(ecs)
end

-- }}}

*/

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
