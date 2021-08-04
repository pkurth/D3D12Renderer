#include "pch.h"
#include "renderer_base.h"
#include "geometry/geometry.h"
#include "render_resources.h"

#include "particles_rs.hlsli"


dx_pipeline renderer_base::depthOnlyPipeline;
dx_pipeline renderer_base::animatedDepthOnlyPipeline;
dx_pipeline renderer_base::shadowPipeline;
dx_pipeline renderer_base::pointLightShadowPipeline;
		   
dx_pipeline renderer_base::textureSkyPipeline;
dx_pipeline renderer_base::proceduralSkyPipeline;
dx_pipeline renderer_base::preethamSkyPipeline;
		   
dx_pipeline renderer_base::outlineMarkerPipeline;
dx_pipeline renderer_base::outlineDrawerPipeline;


dx_command_signature renderer_base::particleCommandSignature;

DXGI_FORMAT renderer_base::outputFormat;




void renderer_base::initializeCommon(DXGI_FORMAT outputFormat)
{
	renderer_base::outputFormat = outputFormat;


	// Sky.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(skyPassFormats, arraysize(skyPassFormats), hdrDepthStencilFormat)
			.depthSettings(true, false)
			.cullFrontFaces();

		textureSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_texture_ps" });
		proceduralSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_procedural_ps" });
		preethamSkyPipeline = createReloadablePipeline(desc, { "sky_vs", "sky_preetham_ps" });
	}

	// Depth prepass.
	{
		DXGI_FORMAT depthOnlyFormat[] = { screenVelocitiesFormat, objectIDsFormat };

		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(depthOnlyFormat, arraysize(depthOnlyFormat), hdrDepthStencilFormat)
			.inputLayout(inputLayout_position);

		depthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_vs", "depth_only_ps" }, rs_in_vertex_shader);
		animatedDepthOnlyPipeline = createReloadablePipeline(desc, { "depth_only_animated_vs", "depth_only_ps" }, rs_in_vertex_shader);
	}

	// Shadow.
	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(0, 0, render_resources::shadowDepthFormat)
			.inputLayout(inputLayout_position)
			//.cullFrontFaces()
			;

		shadowPipeline = createReloadablePipeline(desc, { "shadow_vs" }, rs_in_vertex_shader);
		pointLightShadowPipeline = createReloadablePipeline(desc, { "shadow_point_light_vs", "shadow_point_light_ps" }, rs_in_vertex_shader);
	}

	// Outline.
	{
		auto markerDesc = CREATE_GRAPHICS_PIPELINE
			.inputLayout(inputLayout_position)
			.renderTargets(0, 0, hdrDepthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_ALWAYS,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_REPLACE,
				D3D12_STENCIL_OP_KEEP,
				D3D12_DEFAULT_STENCIL_READ_MASK,
				stencil_flag_selected_object) // Mark selected object.
			.depthSettings(false, false);

		outlineMarkerPipeline = createReloadablePipeline(markerDesc, { "outline_vs" }, rs_in_vertex_shader);


		auto drawerDesc = CREATE_GRAPHICS_PIPELINE
			.renderTargets(ldrPostProcessFormat, hdrDepthStencilFormat)
			.stencilSettings(D3D12_COMPARISON_FUNC_EQUAL,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				D3D12_STENCIL_OP_KEEP,
				stencil_flag_selected_object, // Read only selected object bit.
				0)
			.depthSettings(false, false);

		outlineDrawerPipeline = createReloadablePipeline(drawerDesc, { "fullscreen_triangle_vs", "outline_ps" });
	}


	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	particleCommandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(particle_draw));
}
