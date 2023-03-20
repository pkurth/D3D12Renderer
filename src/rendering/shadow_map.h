#pragma once

#include "scene/scene.h"
#include "light_source.h"
#include "render_pass.h"

void renderSunShadowMap(directional_light& sun, sun_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache);
spot_shadow_info renderSpotShadowMap(const spot_light_cb& spotLight, uint32 lightID, spot_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache, uint32 resolution);
point_shadow_info renderPointShadowMap(const point_light_cb& pointLight, uint32 lightID, point_shadow_render_pass* renderPass, game_scene& scene, bool invalidateCache, uint32 resolution);


struct shadow_render_data
{
	mat4 transform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct shadow_pipeline
{
	PIPELINE_RENDER_DECL(shadow_render_data);

	struct single_sided;
	struct double_sided;
};

struct shadow_pipeline::single_sided : shadow_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct shadow_pipeline::double_sided : shadow_pipeline
{
	PIPELINE_SETUP_DECL;
};



struct point_shadow_pipeline
{
	PIPELINE_RENDER_DECL(shadow_render_data);

	struct single_sided;
	struct double_sided;
};

struct point_shadow_pipeline::single_sided : point_shadow_pipeline
{
	PIPELINE_SETUP_DECL;
};

struct point_shadow_pipeline::double_sided : point_shadow_pipeline
{
	PIPELINE_SETUP_DECL;
};


void initializeShadowPipelines();
