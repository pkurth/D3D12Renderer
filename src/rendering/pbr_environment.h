#pragma once

#include "dx/dx_texture.h"
#include "rendering/light_probe.h"
#include "asset/asset.h"

enum environment_gi_mode
{
	environment_gi_baked,
	environment_gi_raytraced,
};

static const char* environmentGIModeNames[] =
{
	"Baked",
	"Raytraced",
};

struct pbr_environment
{
	environment_gi_mode giMode = environment_gi_baked;

	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> prefilteredRadiance;

	bool isProcedural() const { return sky == 0; }

	void setFromTexture(const fs::path& filename);
	void setToProcedural(vec3 sunDirection);

	void update(vec3 sunDirection);
	void forceUpdate(vec3 sunDirection);

	float globalIlluminationIntensity = 1.f;
	float skyIntensity = 1.f;

	light_probe_grid lightProbeGrid;



	static const uint32 skyResolution = 2048;
	static const uint32 irradianceResolution = 32;
	static const uint32 prefilteredRadianceResolution = 128;

	vec3 lastSunDirection = { -1, -1, -1 };

	void allocate();
};

