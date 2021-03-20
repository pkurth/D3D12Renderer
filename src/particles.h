#pragma once

#include "math.h"
#include "render_pass.h"

struct dx_command_list;

struct particle_system
{
	float emitRate;

protected:
	void initializeAsBillboard(uint32 particleStructSize, uint32 maxNumParticles, float emitRate);
	void initializeAsMesh(uint32 particleStructSize, dx_mesh mesh, submesh_info submesh, uint32 maxNumParticles, float emitRate);

	void update(float dt, const struct dx_pipeline& emitPipeline, const struct dx_pipeline& simulatePipeline);

	particle_draw_info getDrawInfo(const struct dx_pipeline& renderPipeline);

	virtual void setSimulationParameters(dx_command_list* cl) = 0;

	static dx_mesh billboardMesh;

	submesh_info submesh;

private:
	static void initializePipeline();

	void initializeInternal(uint32 particleStructSize, uint32 maxNumParticles, float emitRate, submesh_info submesh);

	void setResources(dx_command_list* cl, const struct particle_sim_cb& cb, uint32 offset, bool setUserResources);
	uint32 getAliveListOffset(uint32 alive);
	uint32 getDeadListOffset();

	uint32 maxNumParticles;
	uint32 currentAlive = 0;

	ref<dx_buffer> particlesBuffer;
	ref<dx_buffer> listBuffer; // Counters, dead, alive 0, alive 1.
	ref<dx_buffer> dispatchBuffer;


	uint32 index;



	static dx_command_signature commandSignature;
	static ref<dx_buffer> particleDrawCommandBuffer;

	friend struct dx_renderer;
};

