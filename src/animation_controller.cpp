#include "pch.h"
#include "animation_controller.h"
#include "skinning.h"
#include "imgui.h"


mat4* animation_controller::allocateSkin(ref<composite_mesh> mesh)
{
	const dx_mesh& dxMesh = mesh->mesh;
	animation_skeleton& skeleton = mesh->skeleton;

	auto [vb, vertexOffset, skinningMatrices] = skinObject(dxMesh.vertexBuffer, (uint32)skeleton.joints.size());

	prevFrameVertexBuffer = this->currentVertexBuffer;
	this->currentVertexBuffer = vb;

	uint32 numSubmeshes = (uint32)mesh->submeshes.size();
	for (uint32 i = 0; i < numSubmeshes; ++i)
	{
		prevFrameSubmeshes[i] = currentSubmeshes[i];

		currentSubmeshes[i] = mesh->submeshes[i].info;
		currentSubmeshes[i].baseVertex += vertexOffset;
	}

	return skinningMatrices;
}

void animation_controller::defaultVertexBuffer(ref<composite_mesh> mesh)
{
	const dx_mesh& dxMesh = mesh->mesh;

	prevFrameVertexBuffer = dxMesh.vertexBuffer;
	this->currentVertexBuffer = dxMesh.vertexBuffer;

	uint32 numSubmeshes = (uint32)mesh->submeshes.size();
	for (uint32 i = 0; i < numSubmeshes; ++i)
	{
		prevFrameSubmeshes[i] = mesh->submeshes[i].info;
		currentSubmeshes[i] = mesh->submeshes[i].info;
	}
}

void simple_animation_controller::update(scene_entity entity, float dt)
{
	ref<composite_mesh> mesh = entity.getComponent<raster_component>().mesh;
	animation_skeleton& skeleton = mesh->skeleton;
	if (animationIndex < (uint32)skeleton.clips.size())
	{
		mat4* skinningMatrices = allocateSkin(mesh);
		
		animation_clip& clip = skeleton.clips[animationIndex];
		time += dt * timeScale;
		if (time >= clip.lengthInSeconds)
		{
			time = fmod(time, clip.lengthInSeconds);

			lastRootMotion = clip.getFirstRootTransform();
		}
		
		trs localTransforms[128];
		trs rootMotion;

		skeleton.sampleAnimation(animationIndex, time, localTransforms, &rootMotion);
		skeleton.getSkinningMatricesFromLocalTransforms(localTransforms, skinningMatrices);

		if (entity.hasComponent<trs>())
		{
			trs& transform = entity.getComponent<trs>();
			transform = transform * invert(lastRootMotion) * rootMotion;
			transform.rotation = normalize(transform.rotation);
		}
		lastRootMotion = rootMotion;
	}
	else
	{
		defaultVertexBuffer(mesh);
	}
}

void simple_animation_controller::edit(scene_entity entity)
{
	ref<composite_mesh> mesh = entity.getComponent<raster_component>().mesh;
	animation_skeleton& skeleton = mesh->skeleton;

	bool animationChanged = ImGui::Dropdown("Currently playing", [](uint32 index, void* data)
	{
		animation_skeleton& skeleton = *(animation_skeleton*)data;
		const char* result = 0;
		if (index < (uint32)skeleton.clips.size())
		{
			result = skeleton.clips[index].name.c_str();
		}
		return result;
	}, animationIndex, &skeleton);

	if (animationChanged)
	{
		time = 0.f;
		lastRootMotion = skeleton.clips[animationIndex].getFirstRootTransform();
	}

	ImGui::SliderFloat("Time scale", &timeScale, 0.f, 1.f);
	if (animationIndex < (uint32)skeleton.clips.size())
	{
		ImGui::SliderFloat("Time", &time, 0.f, skeleton.clips[animationIndex].lengthInSeconds);

		skeleton.clips[animationIndex].edit();
	}
}

void ragdoll_animation_controller::update(scene_entity entity, float dt)
{
	ref<composite_mesh> mesh = entity.getComponent<raster_component>().mesh;
	animation_skeleton& skeleton = mesh->skeleton;
	mat4* skinningMatrices = allocateSkin(mesh);


	assert(entity.hasComponent<ragdoll_component>());
	auto& ragdoll = entity.getComponent<ragdoll_component>();

	trs* rbTransforms = (trs*)alloca(ragdoll.rigidbodies.size() * sizeof(trs));

	for (uint32 i = 0; i < (uint32)ragdoll.rigidbodies.size(); ++i)
	{
		scene_entity& rbEntity = ragdoll.rigidbodies[i];
		rbTransforms[i] = rbEntity.getComponent<trs>();
	}

	skeleton.getSkinningMatricesFromGlobalTransforms(rbTransforms, skinningMatrices);
}

ref<animation_controller> createAnimationController(animation_controller_type type)
{
	switch (type)
	{
		case animation_controller_type_simple: return make_ref<simple_animation_controller>();
		case animation_controller_type_ragdoll: return make_ref<ragdoll_animation_controller>();
		default: assert(false);
	}
	return 0;
}
