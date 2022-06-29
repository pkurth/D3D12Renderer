
#define MESH_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
	"DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
    "CBV(b1), " \
	"SRV(t0), " \
	"StaticSampler(s0, space=1), " \
	"DescriptorTable(SRV(t0, space=1, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"


#include "koch_common.hlsli"
#include "../marching_cubes/marching_cubes_ms.hlsli"
