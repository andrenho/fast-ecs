#include <iostream>

#include "fastecs.hh"

//
// COMPONENTS
//

struct Position {
    float x, y;
};

struct Direction {
    float angle;
};

// 
// ENGINE
//

struct Message {};

using Messages = std::variant<Message>;

using MyECS = ecs::ECS<
    ecs::NoGlobal, 
    Messages,
    ecs::NoPool, 
    Position, Direction>;

//
// SYSTEMS
//

static void position_system(MyECS& ecs) {
    for (auto& e: ecs.entities<Position>()) {
        Position& pos = e.get<Position>();
        pos.x += 1;
        std::cout << "Entity " << e.id << " position.x was " << pos.x -1 <<
                     " but now is " << pos.x << ".\n";
    }
    ecs.add_message(Message {});
}

static void direction_system(MyECS const& ecs) {
    for (auto const& e : ecs.entities<Direction>()) {
        std::cout << "Entity " << e.id << " direction is " << e.get<Direction>().angle << ".\n";
    }
}

//
// MAIN PROCEDURE
//

int main()
{
    MyECS ecs;

    auto e1 = ecs.add(),
         e2 = ecs.add();

    e1.add<Position>(20.f, 30.f);
    e1.add<Direction>(1.2f);

    e2.add<Position>(40.f, 50.f);
    e2.get<Position>().x = 100.f;

    ecs.run_mutable("position", position_system);
    ecs.run_st("direction", direction_system);

    for (auto const& msg: ecs.messages<Message>())
        printf("X");
    printf("\n");

    ecs.run_mutable("position", position_system);

    for (auto const& msg: ecs.messages<Message>())
        printf("Y");
    printf("\n");
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
