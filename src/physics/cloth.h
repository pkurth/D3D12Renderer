#pragma once

#include "bounding_volumes.h"
#include "geometry/geometry.h"
#include "rendering/material.h"

struct cloth_constraint
{
	uint32 a, b;
	float restDistance;
	float inverseMassSum;
};

struct cloth_component
{
	cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, float totalMass, float thickness = 0.1f, float damping = 0.3f, float gravityFactor = 1.f);

	void setWorldPositionOfFixedVertices(const trs& transform);
	void applyWindForce(vec3 force);
	void simulate(uint32 velocityIterations, uint32 positionIterations, uint32 driftIterations, float dt);

	uint32 getRenderableVertexCount() const;
	uint32 getRenderableTriangleCount() const;
	//submesh_info getRenderData(vec3* positions, vertex_uv_normal_tangent* others, indexed_triangle16* triangles) const;
	std::tuple<material_vertex_buffer_group_view, material_vertex_buffer_group_view, material_index_buffer_view, submesh_info> getRenderData();

	float gravityFactor;
	float damping;
	float thickness;
	float stiffness = 0.5f;

private:
	std::vector<vec3> positions;
	std::vector<vec3> prevPositions;
	std::vector<vec3> velocities;
	std::vector<vec3> forceAccumulators;
	std::vector<float> invMasses;
	std::vector<cloth_constraint> constraints;

	uint32 gridSizeX, gridSizeY;
	float width, height;

	void solveVelocities(const std::vector<struct cloth_constraint_temp>& constraintsTemp);
	void solvePositions();

	vec3 getParticlePosition(float relX, float relY);

	void addConstraint(uint32 a, uint32 b);

	ref<dx_index_buffer> indexBuffer;
	material_vertex_buffer_group_view prevFrameVB;
};
