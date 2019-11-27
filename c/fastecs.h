#ifndef FASTECS_H_
#define FASTECS_H_

#include <stddef.h>
#include <stdint.h>

typedef struct ECS ECS;

typedef int BUCKET;
typedef uintptr_t ENTITY;

ECS* ecs_new();
int  ecs_destroy(ECS* E);

BUCKET ecs_create_bucket(ECS* E);

ENTITY ecs_create_entity(ECS* E);
ENTITY ecs_create_entity_bucket(ECS* E, BUCKET bkt);

void  _ecs_add_component(ECS* E, ENTITY entity, uint64_t idx, void* object);
void* _ecs_get_component(ECS* E, ENTITY entity, uint64_t idx);

#define ecs_add_component(E, entity, Component, ...) (_ecs_add_component(E, entity, 0 /*TODO*/, &(Component) __VA_ARGS__))
#define ecs_get_component(E, entity, Component)  ((Component*)_ecs_get_component(E, entity, 0 /*TODO*/))

int ecs_system(ECS* E, int(*f)(ECS*, void*), void* data);

#endif

// vim: ts=4:sw=4:sts=4:expandtab:foldmethod=marker
