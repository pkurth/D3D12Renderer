#pragma once

#include "bounding_volumes.h"

enum bvh_node_type : uint16
{
	bvh_leaf_node,
	bvh_inner_node,
};

enum bvh_volume_type : uint16
{
	bvh_aabb_volume,
	bvh_sphere_volume
};

struct bvh_node
{
	union
	{
		bounding_box aabb;
		bounding_sphere sphere;
	};
	bvh_node_type nodeType;
	bvh_volume_type volumeType;

	union
	{
		struct
		{
			uint32 firstElement;
			uint32 numElements;
		};
		struct
		{
			uint32 leftChild;
			uint32 rightChild;
		};
	};
};

struct world_bvh
{
	std::vector<uint16> objectIDs;
	std::vector<bvh_node> nodes;
	uint32 root;
};


