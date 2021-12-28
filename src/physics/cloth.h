#pragma once

#include "bounding_volumes.h"
#include "geometry/mesh_builder.h"
#include "dx/dx_buffer.h"

struct cloth_component
{
	cloth_component() {}
	cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, 
		float totalMass, float stiffness = 0.5f, float damping = 0.3f, float gravityFactor = 1.f);
	cloth_component(const cloth_component&) = default;
	cloth_component(cloth_component&&) = default;

	cloth_component& operator=(const cloth_component&) = default;
	cloth_component& operator=(cloth_component&&) = default;

	void setWorldPositionOfFixedVertices(const trs& transform, bool moveRigid = false);
	void applyWindForce(vec3 force);
	void simulate(uint32 velocityIterations, uint32 positionIterations, uint32 driftIterations, float dt);

	void recalculateProperties(); // Call if stiffness or total mass changed. No need for changes in damping or gravity factor.

	float totalMass;
	float gravityFactor;
	float damping;
	float stiffness;

	uint32 gridSizeX, gridSizeY;
	float width, height;

private:
	struct cloth_constraint
	{
		uint32 a, b;
		float restDistance;
		float inverseMassSum;
	};

	std::vector<vec3> positions;
	std::vector<vec3> prevPositions;
	std::vector<vec3> velocities;
	std::vector<vec3> forceAccumulators;
	std::vector<float> invMasses;
	std::vector<cloth_constraint> constraints;

	void solveVelocities(const std::vector<struct cloth_constraint_temp>& constraintsTemp);
	void solvePositions();

	vec3 getParticlePosition(float relX, float relY);

	void addConstraint(uint32 a, uint32 b);

	friend struct cloth_render_component;
};


#ifndef PHYSICS_ONLY
struct cloth_render_component
{
	std::tuple<dx_vertex_buffer_group_view, dx_vertex_buffer_group_view, dx_index_buffer_view, submesh_info> getRenderData(const cloth_component& cloth);

	ref<dx_index_buffer> indexBuffer;
	dx_vertex_buffer_group_view prevFrameVB;
};
#endif
