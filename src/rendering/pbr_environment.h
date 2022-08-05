#pragma once

#include "dx/dx_texture.h"

enum environment_gi_update_mode
{
	environment_gi_update_baked,
	environment_gi_update_raytraced,
};

struct pbr_environment
{
	environment_gi_update_mode giMode = environment_gi_update_baked;
	fs::path name;

	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> prefilteredRadiance;

	void setFromTexture(const fs::path& filename);
	void setToProcedural(vec3 sunDirection);

	void update(vec3 sunDirection);
	void forceUpdate(vec3 sunDirection);

private:
	static const uint32 skyResolution = 2048;
	static const uint32 irradianceResolution = 32;
	static const uint32 prefilteredRadianceResolution = 128;

	vec3 lastSunDirection = { -1, -1, -1 };

	void allocate();
};

