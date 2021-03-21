#ifndef PARTICLES_RS_HLSLI
#define PARTICLES_RS_HLSLI


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



struct particle_sim_cb
{
	float emitRate;
	float dt;
	uint32 indexCount;
	uint32 startIndex;
	uint32 baseVertex;
};

#ifndef USER_PARTICLE_SIMULATION_RS
#define USER_APPEND_PARTICLE_SIMULATION_RS ""
#else
#define USER_APPEND_PARTICLE_SIMULATION_RS USER_PARTICLE_SIMULATION_RS ", "
#endif

#define PARTICLE_COMPUTE_RS \
	USER_APPEND_PARTICLE_SIMULATION_RS \
	"RootConstants(num32BitConstants=5, b0, space=1), " \
    "UAV(u0, space=1), " \
    "UAV(u1, space=1), " \
    "UAV(u2, space=1), " \
    "UAV(u3, space=1), " \
    "UAV(u4, space=1), " \
    "UAV(u5, space=1), " \
    "UAV(u6, space=1)"
	

#define PARTICLE_COMPUTE_RS_CB					0
#define PARTICLE_COMPUTE_RS_DISPATCH_INFO		1
#define PARTICLE_COMPUTE_RS_DRAW_INFO			2
#define PARTICLE_COMPUTE_RS_COUNTERS			3
#define PARTICLE_COMPUTE_RS_PARTICLES			4
#define PARTICLE_COMPUTE_RS_DEAD_LIST			5
#define PARTICLE_COMPUTE_RS_CURRENT_ALIVE		6
#define PARTICLE_COMPUTE_RS_NEW_ALIVE			7

#define PARTICLE_COMPUTE_RS_COUNT				(PARTICLE_COMPUTE_RS_NEW_ALIVE + 1)





#ifndef USER_PARTICLE_RENDERING_RS
#define USER_APPEND_PARTICLE_RENDERING_RS ""
#else
#define USER_APPEND_PARTICLE_RENDERING_RS USER_PARTICLE_RENDERING_RS ", "
#endif

#define PARTICLES_RENDERING_RS \
	"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS), " \
	USER_APPEND_PARTICLE_RENDERING_RS \
	"SRV(t0, space=1, visibility=SHADER_VISIBILITY_VERTEX), " \
	"SRV(t1, space=1, visibility=SHADER_VISIBILITY_VERTEX)"


#define PARTICLE_RENDERING_RS_PARTICLES			0
#define PARTICLE_RENDERING_RS_ALIVE_LIST		1

#define PARTICLE_RENDERING_RS_COUNT				(PARTICLE_RENDERING_RS_ALIVE_LIST + 1)


#endif

