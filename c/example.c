#include "fastecs.h"

typedef struct {
    int x, y;
} Position;

typedef struct {
    int angle;
} Direction;

static int position_system(ECS* E, void* data)
{
    return 0;
}

static int direction_system(ECS* E, void* data)
{
    return 0;
}

int main()
{
    ECS* E = ecs_new();

    BUCKET bkt = ecs_create_bucket(E);

    ENTITY e1 = ecs_create_entity(E);
    ENTITY e2 = ecs_create_entity_bucket(E, bkt);

    ecs_add_component(E, e1, Position, { .x = 4, .y = 8 });
    ecs_add_component(E, e1, Direction, { .angle = 90 });

    ecs_add_component(E, e2, Position, { .x = 8, .y = 16 });
    ecs_get_component(E, e2, Position)->y = 2;

    ecs_system(E, position_system, NULL);
    ecs_system(E, direction_system, NULL);

    ecs_destroy(E);

    return 0;
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
