#pragma once

#include "dx/dx.h"
#include "dx/dx_texture.h"
#include "dx/dx_command_list.h"
#include "render_utils.h"

#include <unordered_map>


#define SHADOW_MAP_WIDTH 6144
#define SHADOW_MAP_HEIGHT 6144

struct render_resource_desc
{
	const wchar* name;
	D3D12_RESOURCE_DESC desc;
	D3D12_RESOURCE_STATES initialState;
};

struct render_resources
{
	static void initializeGlobalResources();

	// Call from renderer in beginFrame().
	static void declareTemporaryResourceNeeds(uint64 id, const std::vector<render_resource_desc>& descs);

	// Call from central place after all renderers have declared their needs.
	static void evaluate();

	// Call from renderer in endFrame().
	static const std::vector<ref<dx_texture>>& getTemporaryResources(uint64 id, dx_command_list* cl);

	static dx_cpu_descriptor_handle nullTextureSRV;
	static dx_cpu_descriptor_handle nullBufferSRV;

	static dx_rtv_descriptor_handle nullScreenVelocitiesRTV;
	static dx_rtv_descriptor_handle nullObjectIDsRTV;

	static ref<dx_texture> brdfTex;
	static ref<dx_texture> whiteTexture;
	static ref<dx_texture> blackTexture;
	static ref<dx_texture> blackCubeTexture;
	static ref<dx_texture> noiseTexture;

	static ref<dx_texture> shadowMap;
	static ref<dx_texture> staticShadowMapCache;

private:
	struct temporary_render_resources
	{
		uint64 hash;
		uint64 alignment;
		uint64 totalAllocationSize;
		std::vector<uint64> offsetsInBytes;
		std::vector<render_resource_desc> descs;
		std::vector<ref<dx_texture>> textures;
	};

	static void getAllocationInfo(temporary_render_resources& resources);
	static uint64 hashResourceRequirements(const std::vector<render_resource_desc>& descs);

	static void allocateResources(temporary_render_resources& resources);

	static dx_heap resourceHeap;
	static std::unordered_map<uint64, temporary_render_resources> resourceMap;

	static bool dirty;
};
