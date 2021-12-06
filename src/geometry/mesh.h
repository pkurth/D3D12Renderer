#pragma once

#include "physics/bounding_volumes.h"
#include "dx/dx_buffer.h"
#include "animation/animation.h"
#include "geometry.h"

struct pbr_material;


struct submesh
{
	submesh_info info;
	bounding_box aabb; // In composite's local space.
	trs transform;

	ref<pbr_material> material;
	std::string name;
};

struct composite_mesh
{
	std::vector<submesh> submeshes;
	animation_skeleton skeleton;
	dx_mesh mesh;
	bounding_box aabb;

	fs::path filepath;
	uint32 flags;
};


ref<composite_mesh> loadMeshFromFile(const fs::path& sceneFilename, bool loadSkeleton = true, bool loadAnimations = true, uint32 flags = mesh_creation_flags_default);

// Same function but with different default flags (includes skin).
inline ref<composite_mesh> loadAnimatedMeshFromFile(const fs::path& sceneFilename, bool loadSkeleton = true, bool loadAnimations = true, uint32 flags = mesh_creation_flags_animated)
{
	return loadMeshFromFile(sceneFilename, loadSkeleton, loadAnimations, flags);
}

struct raster_component
{
	ref<composite_mesh> mesh;
};
