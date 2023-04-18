#include "pch.h"
#include "pbr_environment.h"
#include "texture_preprocessing.h"
#include "render_resources.h"
#include "dx/dx_context.h"
#include "asset/file_registry.h"
#include "core/job_system.h"

void pbr_environment::setFromTexture(const fs::path& filename)
{
	allocate();

	ref<dx_texture> equiSky = loadTextureFromFileAsync(filename,
		image_load_flags_noncolor | image_load_flags_cache_to_dds | image_load_flags_gen_mips_on_cpu);

	if (equiSky)
	{
		struct post_process_sky_data
		{
			ref<dx_texture> equiSky;
			ref<dx_texture>& sky;
			ref<dx_texture>& irradiance;
			ref<dx_texture>& prefilteredRadiance;
		};

		post_process_sky_data data =
		{
			equiSky,
			sky,
			irradiance,
			prefilteredRadiance,
		};

		lowPriorityJobQueue.createJob<post_process_sky_data>([](post_process_sky_data& data, job_handle)
		{
			dxContext.renderQueue.waitForOtherQueue(dxContext.copyQueue);
			dx_command_list* cl = dxContext.getFreeRenderCommandList();

			//generateMipMapsOnGPU(cl, data.equiSky);

			data.sky = equirectangularToCubemap(cl, data.equiSky, skyResolution, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
			data.sky->handle = data.equiSky->handle;
			texturedSkyToIrradiance(cl, data.sky, data.irradiance);
			texturedSkyToPrefilteredRadiance(cl, data.sky, data.prefilteredRadiance);

			SET_NAME(data.sky->resource, "Sky");

			dxContext.executeCommandList(cl);
		}, data).submitAfter(equiSky->loadJob);
	}
}

void pbr_environment::setToProcedural(vec3 sunDirection)
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	allocate();

	sky = 0;
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
