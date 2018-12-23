#include <iostream>

#include "fastecs.hh"

//
// COMPONENTS
//

struct Position {
    Position(float x, float y) : x(x), y(y) {}
    float x, y;
};

struct Direction {
    Direction(float angle) : angle(angle) {}
    float angle;
};

// 
// ENGINE
//

using MyEngine = ECS::Engine<class System, ECS::NoGlobal, ECS::NoQueue, Position, Direction>;

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
        e.for_each<Position>([](size_t entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public System {
public:
    void Execute(MyEngine& e) override {
        e.for_each<Direction>([](size_t entity, Direction& dir) {
            std::cout << "Entity " << entity << " direction is " << dir.angle << ".\n";
        });
    }
};

//
// MAIN PROCEDURE
//

int main()
{
    MyEngine e;

    size_t e1 = e.add_entity(),
           e2 = e.add_entity();

    e.add_component<Position>(e1, 20.f, 30.f);
    e.add_component<Direction>(e1, 1.2f);

    e.add_component<Position>(e2, 40.f, 50.f);
    e.component<Position>(e2).x = 100.f;

    e.add_system<PositionSystem>();
    e.add_system<DirectionSystem>();

    for(auto& sys: e.systems()) {
        sys->Execute(e);
    }
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
