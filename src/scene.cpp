#include "pch.h"
#include "scene.h"

scene_entity scene::createEntity()
{
	return scene_entity(registry.create(), &registry);
}
