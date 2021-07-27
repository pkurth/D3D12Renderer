#pragma once

#include "renderer_base.h"
#include "render_pass.h"
#include "light_source.h"
#include "pbr.h"
#include "render_algorithms.h"

#include "camera.hlsli"

struct asset_editor_renderer : renderer_base
{
	void initialize(uint32 renderWidth, uint32 renderHeight);

	void beginFrame(uint32 renderWidth, uint32 renderHeight);
	void endFrame();

	void setCamera(const render_camera& camera);
	void setEnvironment(const ref<pbr_environment>& environment);
	void setSun(const directional_light& light);

	void submitRenderPass(opaque_render_pass* renderPass) { assert(!opaqueRenderPass); opaqueRenderPass = renderPass; }


	// Settings.
	tonemap_settings tonemapSettings;
	float environmentIntensity = 1.f;
	float skyIntensity = 1.f;



	uint32 renderWidth;
	uint32 renderHeight;
	ref<dx_texture> frameResult;

private:
	opaque_render_pass* opaqueRenderPass;
	ref<pbr_environment> environment;

	camera_cb camera;
	directional_light_cb sun;

	ref<dx_texture> hdrColorTexture;
	ref<dx_texture> worldNormalsTexture;
	ref<dx_texture> reflectanceTexture;
	ref<dx_texture> depthStencilBuffer;
	ref<dx_texture> hdrPostProcessingTexture;
	ref<dx_texture> ldrPostProcessingTexture;
};

