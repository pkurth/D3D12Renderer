#pragma once

#include "core/math.h"
#include "render_pass.h"

#include "particles_rs.hlsli"

struct dx_command_list;

enum sort_mode
{
	sort_mode_none,
	sort_mode_front_to_back,
	sort_mode_back_to_front,
};

struct particle_system
{
	float emitRate;

protected:
	void initializeAsBillboard(uint32 particleStructSize, uint32 maxNumParticles, float emitRate, sort_mode sortMode = sort_mode_none);
	void initializeAsMesh(uint32 particleStructSize, dx_mesh mesh, submesh_info submesh, uint32 maxNumParticles, float emitRate, sort_mode sortMode = sort_mode_none);

	void update(float dt, const struct dx_pipeline& emitPipeline, const struct dx_pipeline& simulatePipeline);

	particle_draw_info getDrawInfo(const struct dx_pipeline& renderPipeline);

	virtual void setSimulationParameters(dx_command_list* cl) = 0;

	static dx_mesh billboardMesh;

	submesh_info submesh;

private:
	static void initializePipeline();

	void initializeInternal(uint32 particleStructSize, uint32 maxNumParticles, float emitRate, submesh_info submesh, sort_mode sortMode);

	void setResources(dx_command_list* cl, const struct particle_sim_cb& cb, uint32 offset, bool setUserResources);
	uint32 getAliveListOffset(uint32 alive);
	uint32 getDeadListOffset();
	uint32 getNumUserRootParameters(const struct dx_pipeline& pipeline);

	uint32 maxNumParticles;
	uint32 currentAlive = 0;

	ref<dx_buffer> particlesBuffer;
	ref<dx_buffer> listBuffer; // Counters, dead, alive 0, alive 1.
	ref<dx_buffer> dispatchBuffer;
	ref<dx_buffer> sortBuffer;

	sort_mode sortMode;

	uint32 index;



	static ref<dx_buffer> particleDrawCommandBuffer;
	static dx_command_signature particleCommandSignature;

	friend void initializeRenderUtils();
	template <typename T> friend struct particle_render_pipeline;
};

template <typename derived_pipeline_t>
struct particle_render_pipeline
{
	static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<derived_pipeline_t>& rc);
};

template<typename derived_pipeline_t>
inline void particle_render_pipeline<derived_pipeline_t>::render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<derived_pipeline_t>& rc)
{
	const particle_draw_info& info = rc.drawInfo;

	cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_PARTICLES, info.particleBuffer->gpuVirtualAddress);
	cl->setRootGraphicsSRV(info.rootParameterOffset + PARTICLE_RENDERING_RS_ALIVE_LIST, info.aliveList->gpuVirtualAddress + info.aliveListOffset);

	cl->setVertexBuffer(0, rc.vertexBuffer.positions);
	if (rc.vertexBuffer.others)
	{
		cl->setVertexBuffer(1, rc.vertexBuffer.others);
	}
	cl->setIndexBuffer(rc.indexBuffer);

	cl->drawIndirect(particle_system::particleCommandSignature, 1, info.commandBuffer, info.commandBufferOffset);
}
