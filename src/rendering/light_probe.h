#pragma once

#include "core/math.h"
#include "dx/dx_texture.h"

#include "render_pass.h"
#include "pbr_raytracer.h"

struct light_probe_grid
{
	vec3 minCorner;
	float cellSize;

	uint32 numNodesX;
	uint32 numNodesY;
	uint32 numNodesZ;
	uint32 totalNumNodes;

	ref<dx_texture> irradiance;
	ref<dx_texture> depth;

	void initialize(vec3 minCorner, vec3 dimensions, float cellSize);
	void visualize(ldr_render_pass* ldrRenderPass);
};

struct light_probe_tracer : pbr_raytracer
{
	void initialize();

	void render(dx_command_list* cl, const raytracing_tlas& tlas,
		const light_probe_grid& grid,
		const common_material_info& materialInfo);

private:

	const uint32 maxRecursionDepth = 2;
	const uint32 maxPayloadSize = 3 * sizeof(float); // Radiance-payload is 1 x float3.


	// Only descriptors in here!
	struct input_resources
	{
		dx_cpu_descriptor_handle tlas;
	};

	struct output_resources
	{
		dx_cpu_descriptor_handle output;
	};
};


void initializeLightProbePipelines();


