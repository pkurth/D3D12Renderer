#pragma once

#include "core/math.h"
#include "core/random.h"
#include "dx/dx_texture.h"

#include "render_pass.h"
#include "pbr_raytracer.h"

#include "light_probe.hlsli"

struct light_probe_grid
{
	vec3 minCorner;
	float cellSize;

	uint32 numNodesX;
	uint32 numNodesY;
	uint32 numNodesZ;
	uint32 totalNumNodes = 0;

	ref<dx_texture> irradiance;
	ref<dx_texture> depth;

	ref<dx_texture> raytracedRadiance;
	ref<dx_texture> raytracedDirectionAndDistance;

	void initialize(vec3 minCorner, vec3 dimensions, float cellSize);
	void visualize(opaque_render_pass* renderPass, const pbr_environment& environment);

	void updateProbes(dx_command_list* cl, const raytracing_tlas& lightProbeTlas, const ref<dx_texture>& sky, dx_dynamic_constant_buffer sunCBV) const;

	light_probe_grid_cb getCB() const { return { minCorner, cellSize, numNodesX, numNodesY, numNodesZ }; }


private:
	bool visualizeProbes = false;
	bool visualizeRays = false;
	bool showTestSphere = false;
	bool autoRotateRays = true;

	bool rotateRays = false;

	float irradianceUIScale = 1.f;
	float depthUIScale = 1.f;
	float raytracedRadianceUIScale = 1.f;

	mutable quat rayRotation = quat::identity;
	mutable random_number_generator rng = { 61913 };

	friend struct light_probe_tracer;
};

struct light_probe_tracer : pbr_raytracer
{
	void initialize();

	void render(dx_command_list* cl, const raytracing_tlas& tlas,
		const light_probe_grid& grid,
		const ref<dx_texture>& sky,
		dx_dynamic_constant_buffer sunCBV);

private:

	const uint32 maxRecursionDepth = 2;
	const uint32 maxPayloadSize = 4 * sizeof(float); // Radiance-payload is 1 x float3, 1 x float.


	// Only descriptors in here!
	struct input_resources
	{
		dx_cpu_descriptor_handle tlas;
		dx_cpu_descriptor_handle sky;
		dx_cpu_descriptor_handle irradiance;
		dx_cpu_descriptor_handle depth;
	};

	struct output_resources
	{
		dx_cpu_descriptor_handle radiance;
		dx_cpu_descriptor_handle directionAndDistance;
	};
};


void initializeLightProbePipelines();


