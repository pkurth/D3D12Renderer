#pragma once

#include "math.h"
#include "random.h"
#include "dx_texture.h"
#include "material.h"
#include "render_pass.h"
#include "dx_pipeline.h"

struct dx_command_list;


enum particle_render_mode
{
	particle_render_mode_billboard,
	particle_render_mode_mesh,
};

struct particle_system
{
	void initializeAsBillboard(const std::string& shaderName, uint32 particleStructSize, uint32 maxNumParticles, float emitRate);
	void initializeAsMesh(const std::string& shaderName, uint32 particleStructSize, dx_mesh mesh, submesh_info submesh, uint32 maxNumParticles, float emitRate, uint32 flags);

	void update(float dt, const std::function<void(dx_command_list* cl)>& setUserResourcesFunction);

	template <typename material_t>
	void render(transparent_render_pass* renderPass, const ref<material_t>& material);

private:
	static void initializePipeline();

	void initialize(const std::string& shaderName, uint32 particleStructSize, uint32 maxNumParticles, float emitRate, submesh_info submesh);

	void setResources(dx_command_list* cl, const struct particle_sim_cb& cb, uint32 offset, const std::function<void(dx_command_list* cl)>& setUserResourcesFunction);
	uint32 getAliveListOffset(uint32 alive);
	uint32 getDeadListOffset();

	uint32 maxNumParticles;
	float emitRate;
	uint32 currentAlive = 0;

	ref<dx_buffer> particlesBuffer;
	ref<dx_buffer> listBuffer; // Counters, dead, alive 0, alive 1.
	ref<dx_buffer> dispatchBuffer;

	dx_mesh mesh;

	particle_render_mode renderMode;

	dx_pipeline emitPipeline;
	dx_pipeline simulatePipeline;

	uint32 index;



	static dx_command_signature commandSignature;
	static ref<dx_buffer> particleDrawCommandBuffer;
	static dx_mesh billboardMesh;

	friend struct dx_renderer;
};

template<typename material_t>
inline void particle_system::render(transparent_render_pass* renderPass, const ref<material_t>& material)
{
	auto& m = (renderMode == particle_render_mode_billboard) ? billboardMesh : mesh;

	renderPass->renderParticles(m.vertexBuffer, m.indexBuffer, particlesBuffer,
		listBuffer, getAliveListOffset(currentAlive),
		particleDrawCommandBuffer, index * particleDrawCommandBuffer->elementSize,
		material);
}



struct particle_billboard_material : material_base
{
	static dx_pipeline pipeline; // Static for now.

	dx_texture_atlas atlas;

	particle_billboard_material(const std::string& textureFilename, uint32 cols, uint32 rows);

	static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& materialInfo);
	void prepareForRendering(dx_command_list* cl);
};

