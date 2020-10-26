#pragma once

#include "dx_render_primitives.h"

enum texture_load_flags
{
	texture_load_flags_none						= 0,
	texture_load_flags_noncolor					= (1 << 0),
	texture_load_flags_compress_bc3				= (1 << 1),
	texture_load_flags_gen_mips_on_cpu			= (1 << 2),
	texture_load_flags_allocate_full_mipchain	= (1 << 3), // Use if you want to create the mip chain on the GPU.
	texture_load_flags_premultiply_alpha		= (1 << 4),
	texture_load_flags_cache_to_dds				= (1 << 5),
	texture_load_flags_always_load_from_source	= (1 << 6), // By default the system will always try to load a cached version of the texture. You can prevent this with this flag.

	texture_load_flags_default = texture_load_flags_compress_bc3 | texture_load_flags_gen_mips_on_cpu | texture_load_flags_cache_to_dds,
};

dx_texture loadTextureFromFile(struct dx_context* context, const char* filename, uint32 flags = texture_load_flags_default);
