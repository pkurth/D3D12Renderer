#pragma once

#include "scene.h"

uint64 serializeEntityToMemory(scene_entity entity, void* memory, uint64 maxSize);
bool deserializeEntityFromMemory(scene_entity entity, void* memory, uint64 size);

