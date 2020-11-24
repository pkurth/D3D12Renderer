#pragma once

#include "dx_render_primitives.h"

// If the texture_load_flags_cache_to_dds flags is set, the system will cache the texture as DDS to disk for faster loading next time.
// This is not done if the original file has a newer write time.
// It is also not done if the cache was created with different flags.
// Therefore: If you change these flags, delete the texture cache!

// If you want the mip chain to be computed on the GPU, you must call this yourself. This system only supports CPU mip levels for now.

enum texture_load_flags
{
	texture_load_flags_none						= 0,
	texture_load_flags_noncolor					= (1 << 0),
	texture_load_flags_compress					= (1 << 1),
	texture_load_flags_gen_mips_on_cpu			= (1 << 2),
	texture_load_flags_gen_mips_on_gpu			= (1 << 3),
	texture_load_flags_allocate_full_mipchain	= (1 << 4), // Use if you want to create the mip chain on the GPU.
	texture_load_flags_premultiply_alpha		= (1 << 5),
	texture_load_flags_cache_to_dds				= (1 << 6),
	texture_load_flags_always_load_from_source	= (1 << 7), // By default the system will always try to load a cached version of the texture. You can prevent this with this flag.

	texture_load_flags_default = texture_load_flags_compress | texture_load_flags_gen_mips_on_cpu | texture_load_flags_cache_to_dds,
};




// This system caches textures. It does not keep the resource alive (we store weak ptrs).
// So if no one else has a reference, the texture gets deleted.
// This means you should keep a reference to your textures yourself and not call this every frame.
// TODO: Maybe we want to keep the texture around for a couple more frames?

ref<dx_texture> loadTextureFromFile(const char* filename, uint32 flags = texture_load_flags_default);
