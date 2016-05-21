#include <iostream>

#include "fastecs.hh"

//
// COMPONENTS
//

struct Position {
    Position(float x, float y) : x(x), y(y) {}
    float x, y;
    COMP_ID(0);     // uniquely identify the component
};

struct Direction {
    Direction(float angle) : angle(angle) {}
    float angle;
    COMP_ID(1);
};

//
// SYSTEMS
//

class PositionSystem : public ECS::System<> {

    void Execute(ECS::Engine<>& e) override {
        e.ForEach<Position>([](ECS::Entity entity, Position& pos) {
            pos.x += 1;
            std::cout << "Entity " << entity << " position.x was " << pos.x -1 <<
                         " but now is " << pos.x << ".\n";
        });
    }
};

class DirectionSystem : public ECS::System<> {

    void Execute(ECS::Engine<>& e) override {
        e.ForEach<Direction>([](ECS::Entity entity, Direction& dir) {
            std::cout << "Entity " << entity << " direction is " << dir.angle << ".\n";
        });
    }

};

//
// MAIN PROCEDURE
//

int main()
{
    ECS::Engine<> e;

    ECS::Entity e1 = e.CreateEntity(),
                e2 = e.CreateEntity();

    e.AddComponent<Position>(e1, 20.f, 30.f);
    e.AddComponent<Direction>(e1, 1.2f);

    e.AddComponent<Position>(e2, 40.f, 50.f);
    e.GetComponent<Position>(e2).x = 100.f;

    e.AddSystem<PositionSystem>();
    e.AddSystem<DirectionSystem>();

    for(auto& sys: e.Systems()) {
        sys->Execute(e);
    }
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
