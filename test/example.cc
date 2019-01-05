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

using MyEngine = ecs::Engine<class System, ecs::NoGlobal, ecs::NoEventQueue, Position, Direction>;

//
// SYSTEMS
//

class System { 
public:
    virtual void Execute(MyEngine& e) = 0;
    virtual ~System() {}
};

class PositionSystem : public System {
public:
    void Execute(MyEngine& e) override {
        e.for_each<Position>([](MyEngine&, ecs::Entity const& entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity.get() << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public System {
public:
    void Execute(MyEngine& e) override {
        e.for_each<Direction>([](MyEngine&, ecs::Entity const& entity, Direction& dir) {
            std::cout << "Entity " << entity.get() << " direction is " << dir.angle << ".\n";
        });
    }
};

//
// MAIN PROCEDURE
//

int main()
{
    MyEngine e;

    auto e1 = e.add_entity(),
         e2 = e.add_entity();

    e.add_component(e1, Position { 20.f, 30.f });
    e.add_component(e1, Direction { 1.2f });

    e.add_component(e2, Position { 40.f, 50.f });
    e.component<Position>(e2).x = 100.f;

    e.add_system<PositionSystem>();
    e.add_system<DirectionSystem>();

    for(auto& sys: e.systems()) {
        sys->Execute(e);
    }
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
