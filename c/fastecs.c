#include "fastecs.h"

ECS*
ecs_new()
{
    return NULL;
}

int
ecs_destroy(ECS* E)
{
    return 0;
}

BUCKET
ecs_create_bucket(ECS* E)
{
    return 0;
}

ENTITY
ecs_create_entity(ECS* E)
{
    return 0;
}

ENTITY
ecs_create_entity_bucket(ECS* E, BUCKET bkt)
{
    return 0;
}

void 
_ecs_add_component(ECS* E, ENTITY entity, uint64_t idx, void* object)
{
}

void*
_ecs_get_component(ECS* E, ENTITY entity, uint64_t idx)
{
    return 0;
}

int
ecs_system(ECS* E, int(*f)(ECS*, void*), void* data)
{
    return 0;
}

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
