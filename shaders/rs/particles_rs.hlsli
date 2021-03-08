#ifndef PARTICLES_RS_HLSLI
#define PARTICLES_RS_HLSLI

#include "transform.hlsli"


#ifdef HLSL
struct D3D12_DISPATCH_ARGUMENTS
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
};

struct D3D12_DRAW_INDEXED_ARGUMENTS
{
	uint32 IndexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartIndexLocation;
	int BaseVertexLocation;
	uint32 StartInstanceLocation;
};
#endif

struct particle_dispatch
{
	D3D12_DISPATCH_ARGUMENTS emit;
	D3D12_DISPATCH_ARGUMENTS simulate;
};

struct particle_draw
{
	// If you add stuff here, update the command signature creation in the renderer.
	D3D12_DRAW_INDEXED_ARGUMENTS arguments;
};

struct particle_counters
{
	uint32 numDeadParticles;
	uint32 numAliveParticlesThisFrame;
	float emitRateAccum;
	uint32 newParticles;
};

#define PARTICLES_EMIT_BLOCK_SIZE		256
#define PARTICLES_SIMULATE_BLOCK_SIZE	256


#define PARTICLES_RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS), " \
    "RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
	"SRV(t0, visibility=SHADER_VISIBILITY_VERTEX), " \
	"SRV(t1, visibility=SHADER_VISIBILITY_VERTEX)"

#define PARTICLES_RS_MVP            0
#define PARTICLES_RS_PARTICLES      1
#define PARTICLES_RS_ALIVE_LIST     2


#define PARTICLES_COMPUTE_RS \
    "UAV(u0), " \
    "UAV(u1), " \
    "UAV(u2), " \
    "UAV(u3), " \
    "UAV(u4), " \
    "UAV(u5), " \
    "UAV(u6)"



#endif

