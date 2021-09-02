#pragma once

#include "light_source.h"
#include "dx/dx_buffer.h"
#include "dx/dx_texture.h"

struct dx_command_list;

struct common_material_info
{
	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> environment;
	ref<dx_texture> brdf;

	ref<dx_texture> tiledCullingGrid;
	ref<dx_buffer> tiledObjectsIndexList;
	ref<dx_buffer> tiledSpotLightIndexList;
	ref<dx_buffer> pointLightBuffer;
	ref<dx_buffer> spotLightBuffer;
	ref<dx_buffer> decalBuffer;
	ref<dx_buffer> pointLightShadowInfoBuffer;
	ref<dx_buffer> spotLightShadowInfoBuffer;

	ref<dx_texture> decalTextureAtlas;

	ref<dx_texture> shadowMap;

	ref<dx_texture> volumetricsTexture;

	// These two are only set, if the material is rendered after the opaque pass.
	ref<dx_texture> opaqueDepth;
	ref<dx_texture> worldNormals;

	dx_dynamic_constant_buffer cameraCBV;
	dx_dynamic_constant_buffer sunCBV;

	float environmentIntensity;
	float skyIntensity;
};


struct material_vertex_buffer_view
{
	D3D12_VERTEX_BUFFER_VIEW view;

	material_vertex_buffer_view() { view.SizeInBytes = 0; }
	material_vertex_buffer_view(const ref<dx_vertex_buffer>& vb) : view(vb->view) {}
	material_vertex_buffer_view(const dx_dynamic_vertex_buffer& vb) : view(vb.view) {}

	operator bool() const { return view.SizeInBytes > 0; }
	operator const D3D12_VERTEX_BUFFER_VIEW& () const { return view; }
};

struct material_vertex_buffer_group_view
{
	material_vertex_buffer_view positions;
	material_vertex_buffer_view others;

	material_vertex_buffer_group_view() { positions.view.SizeInBytes = 0; others.view.SizeInBytes = 0; }
	material_vertex_buffer_group_view(const material_vertex_buffer_view& positions, const material_vertex_buffer_view& others) : positions(positions), others(others) {}
	material_vertex_buffer_group_view(const vertex_buffer_group& vb) : positions(vb.positions), others(vb.others) {}

	operator bool() const { return positions && others; }
};

struct material_index_buffer_view
{
	D3D12_INDEX_BUFFER_VIEW view;

	material_index_buffer_view() { view.SizeInBytes = 0; }
	material_index_buffer_view(const ref<dx_index_buffer>& vb) : view(vb->view) {}
	material_index_buffer_view(const dx_dynamic_index_buffer& vb) : view(vb.view) {}

	operator bool() const { return view.SizeInBytes > 0; }
	operator const D3D12_INDEX_BUFFER_VIEW& () const { return view; }
};

#define PIPELINE_SETUP_DECL					static void setup(dx_command_list* cl, const common_material_info& materialInfo)
#define PIPELINE_SETUP_IMPL(name)			void name::setup(dx_command_list* cl, const common_material_info& materialInfo)

#define PIPELINE_RENDER_DECL				static void render(dx_command_list* cl, const mat4& viewProj, const default_render_command<material_t>& rc)
#define PIPELINE_RENDER_IMPL(name)			void name::render(dx_command_list* cl, const mat4& viewProj, const default_render_command<material_t>& rc)

#define PARTICLE_PIPELINE_RENDER_DECL		static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<material_t>& rc)
#define PARTICLE_PIPELINE_RENDER_IMPL(name)	void name::render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<material_t>& rc)

