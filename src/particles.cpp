#include "pch.h"
#include "particles.h"
#include "dx_renderer.h"
#include "camera.h"
#include "geometry.h"
#include "dx_context.h"
#include "dx_command_list.h"
#include "dx_barrier_batcher.h"
#include "dx_profiling.h"
#include "particles_rs.hlsli"

static dx_pipeline renderPipeline;

static dx_pipeline startPipeline;

static dx_command_signature commandSignature;

static ref<dx_buffer> particleDrawCommandBuffer;

static volatile uint32 particleSystemCounter = 0;


static std::unordered_map<std::string, dx_pipeline> pipelineCache;
static std::mutex mutex;


static dx_pipeline getPipeline(const std::string& shaderName, const std::string& type)
{
	mutex.lock();

	std::string s = "particle_" + type + "_" + shaderName + "_cs";

	auto it = pipelineCache.find(s);
	if (it == pipelineCache.end())
	{
		std::string* copy = new std::string(s); // Heap-allocated, because the pipeline reloader caches the const char*.
		it = pipelineCache.insert({ s, createReloadablePipeline(copy->c_str()) }).first;

		createAllPendingReloadablePipelines();
	}

	mutex.unlock();
	return it->second;
}

void particle_system::initialize(const std::string& shaderName, uint32 maxNumParticles, float emitRate)
{
	this->emitPipeline = getPipeline(shaderName, "emit");
	this->simulatePipeline = getPipeline(shaderName, "sim");
	this->maxNumParticles = maxNumParticles;
	this->emitRate = emitRate;
	this->index = atomicIncrement(particleSystemCounter);

	std::vector<uint32> dead(maxNumParticles);

	for (uint32 i = 0; i < maxNumParticles; ++i)
	{
		dead[i] = i;
	}

	particle_counters counters = {};
	counters.numDeadParticles = maxNumParticles;


	particlesBuffer = createBuffer(32, maxNumParticles, 0, true);


	// Lists.
	listBuffer = createBuffer(1, 
		sizeof(particle_counters)
		+ maxNumParticles * 3 * sizeof(uint32),
		0, true);
	updateBufferDataRange(listBuffer, &counters, 0, sizeof(particle_counters));
	updateBufferDataRange(listBuffer, dead.data(), getDeadListOffset(), maxNumParticles * sizeof(uint32));



	// Dispatch.
	dispatchBuffer = createBuffer((uint32)sizeof(particle_dispatch), 1, 0, true);



	// Mesh.
	cpu_mesh cpuMesh(mesh_creation_flags_with_positions);
	submesh_info cubeSubmesh = cpuMesh.pushCube(1.f);
	mesh = cpuMesh.createDXMesh();

	particle_draw draw = {};
	draw.arguments.IndexCountPerInstance = cubeSubmesh.numTriangles * 3;
	updateBufferDataRange(particleDrawCommandBuffer, &draw, (uint32)sizeof(particle_draw) * index, (uint32)sizeof(particle_draw));



	material = make_ref<particle_material>();
}

void particle_system::update(float dt)
{
	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	{
		DX_PROFILE_BLOCK(cl, "Particle system update");

		// Buffers get promoted to D3D12_RESOURCE_STATE_UNORDERED_ACCESS implicitly, so we can omit this.
		//cl->transitionBarrier(dispatchBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		particles_sim_cb cb = { emitRate, dt, (uint32)dxContext.frameID };

		// ----------------------------------------
		// START
		// ----------------------------------------

		{
			DX_PROFILE_BLOCK(cl, "Start");

			cl->setPipelineState(*startPipeline.pipeline);
			cl->setComputeRootSignature(*startPipeline.rootSignature);

			cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_DISPATCH_INFO, dispatchBuffer);
			cl->setCompute32BitConstants(PARTICLES_COMPUTE_RS_CB, cb);
			setResources(cl);

			cl->dispatch(1);
			barrier_batcher(cl)
				//.uav(dispatchBuffer)
				.uav(particleDrawCommandBuffer)
				.transition(dispatchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		}

		// ----------------------------------------
		// EMIT
		// ----------------------------------------

		{
			DX_PROFILE_BLOCK(cl, "Emit");

			cl->setPipelineState(*emitPipeline.pipeline);
			cl->setComputeRootSignature(*emitPipeline.rootSignature);

			//setResources(cl); // Already set.

			cl->dispatchIndirect(commandSignature, 1, dispatchBuffer, 0);
			barrier_batcher(cl)
				.uav(particleDrawCommandBuffer)
				.uav(particlesBuffer)
				.uav(listBuffer);
		}

		// ----------------------------------------
		// SIMULATE
		// ----------------------------------------

		{
			DX_PROFILE_BLOCK(cl, "Simulate");

			cl->setPipelineState(*simulatePipeline.pipeline);
			cl->setComputeRootSignature(*simulatePipeline.rootSignature);

			cl->setCompute32BitConstants(PARTICLES_COMPUTE_RS_CB, cb);
			//setResources(cl); // Already set.

			cl->dispatchIndirect(commandSignature, 1, dispatchBuffer, sizeof(D3D12_DISPATCH_ARGUMENTS));
			barrier_batcher(cl)
				.uav(particleDrawCommandBuffer)
				.uav(particlesBuffer)
				.uav(listBuffer);
		}

		currentAlive = 1 - currentAlive;


		// Buffers decay to D3D12_RESOURCE_STATE_COMMON implicitly, so we can omit this.
		//cl->transitionBarrier(dispatchBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COMMON);
	}

	dxContext.executeCommandList(cl);
}

void particle_system::setResources(dx_command_list* cl)
{
	uint32 nextAlive = 1 - currentAlive;

	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_DRAW_INFO, particleDrawCommandBuffer->gpuVirtualAddress + sizeof(particle_draw) * index);
	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_COUNTERS, listBuffer->gpuVirtualAddress);
	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_PARTICLES, particlesBuffer);
	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_DEAD_LIST, listBuffer->gpuVirtualAddress + getDeadListOffset());
	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_CURRENT_ALIVE, listBuffer->gpuVirtualAddress + getAliveListOffset(currentAlive));
	cl->setRootComputeUAV(PARTICLES_COMPUTE_RS_NEW_ALIVE, listBuffer->gpuVirtualAddress + getAliveListOffset(nextAlive));
}

uint32 particle_system::getAliveListOffset(uint32 alive)
{
	assert(alive == 0 || alive == 1);

	return getDeadListOffset() +
		(1 + alive) * maxNumParticles * (uint32)sizeof(uint32);
}

uint32 particle_system::getDeadListOffset()
{
	return (uint32)sizeof(particle_counters);
}

void particle_system::render(transparent_render_pass* renderPass, const trs& transform)
{
	renderPass->renderParticles(mesh.vertexBuffer, mesh.indexBuffer, particlesBuffer,
		listBuffer, getAliveListOffset(currentAlive),
		particleDrawCommandBuffer, index * particleDrawCommandBuffer->elementSize,
		trsToMat4(transform),
		material);
}

void initializeParticlePipeline()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.inputLayout(inputLayout_position)
		.renderTargets(dx_renderer::transparentLightPassFormats, arraysize(dx_renderer::transparentLightPassFormats), dx_renderer::hdrDepthStencilFormat)
		//.additiveBlending(0)
		.depthSettings(true, false);

	renderPipeline = createReloadablePipeline(desc, { "test_particle_system_vs", "test_particle_system_ps" });


	startPipeline = createReloadablePipeline("particle_start_cs");

	D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
	argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	commandSignature = createCommandSignature({}, &argumentDesc, 1, sizeof(D3D12_DISPATCH_ARGUMENTS));

	particleDrawCommandBuffer = createBuffer(sizeof(particle_draw), 1024, 0, true);
}







struct particle_material : material_base
{
	void prepareForRendering(struct dx_command_list* cl);
	static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& info);
};

void particle_material::prepareForRendering(dx_command_list* cl)
{
}

void particle_material::setupTransparentPipeline(dx_command_list* cl, const common_material_info& info)
{
	cl->setPipelineState(*renderPipeline.pipeline);
	cl->setGraphicsRootSignature(*renderPipeline.rootSignature);
}
