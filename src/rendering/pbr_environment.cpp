#include "pch.h"
#include "pbr_environment.h"
#include "texture_preprocessing.h"
#include "render_resources.h"
#include "dx/dx_context.h"
#include "asset/file_registry.h"

void pbr_environment::setFromTexture(const fs::path& filename)
{
	ref<dx_texture> equiSky = loadTextureFromFile(filename,
		image_load_flags_noncolor | image_load_flags_cache_to_dds | image_load_flags_gen_mips_on_cpu | image_load_flags_synchronous);

	if (equiSky)
	{
		dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
		dx_command_list* cl = dxContext.getFreeRenderCommandList();

		//generateMipMapsOnGPU(cl, equiSky);

		allocate();

		sky = equirectangularToCubemap(cl, equiSky, skyResolution, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		sky->handle = equiSky->handle;
		texturedSkyToIrradiance(cl, sky, irradiance);
		texturedSkyToPrefilteredRadiance(cl, sky, prefilteredRadiance);

		SET_NAME(sky->resource, "Sky");

		handle = getAssetHandleFromPath(filename.lexically_normal());

		dxContext.executeCommandList(cl);
	}
}

void pbr_environment::setToProcedural(vec3 sunDirection)
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	allocate();

	sky = 0;
	handle = {};
	proceduralSkyToIrradiance(cl, sunDirection, irradiance);
	// TODO: Prefiltered radiance.

	dxContext.executeCommandList(cl);

	lastSunDirection = sunDirection;
}

void pbr_environment::update(vec3 sunDirection)
{
	if (isProcedural() && lastSunDirection != sunDirection)
	{
		setToProcedural(sunDirection);
	}
}

void pbr_environment::forceUpdate(vec3 sunDirection)
{
	if (isProcedural())
	{
		setToProcedural(sunDirection);
	}
}

void pbr_environment::allocate()
{
	if (!irradiance)
	{
		irradiance = createCubeTexture(0, irradianceResolution, irradianceResolution, DXGI_FORMAT_R16G16B16A16_FLOAT, false, false, true);
		SET_NAME(irradiance->resource, "Irradiance");
	}

	if (!prefilteredRadiance)
	{
		prefilteredRadiance = createCubeTexture(0, prefilteredRadianceResolution, prefilteredRadianceResolution, DXGI_FORMAT_R16G16B16A16_FLOAT,
			true, false, true, D3D12_RESOURCE_STATE_COMMON, true);
		SET_NAME(prefilteredRadiance->resource, "Prefiltered Radiance");
	}
}
