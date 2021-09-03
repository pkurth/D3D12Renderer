#pragma once

#include "bounding_volumes.h"
#include "geometry/geometry.h"

struct cloth_particle
{
	vec3 position;
	vec3 prevPosition;
	vec3 velocity;
	vec3 forceAccumulator;
	float invMass;
};

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
	submesh_info getRenderData(vec3* positions, vertex_uv_normal_tangent* others, indexed_triangle16* triangles) const;

	std::vector<cloth_particle> particles;
	std::vector<cloth_constraint> constraints;

	float gravityFactor;
	float damping;
	float thickness;
	float stiffness = 0.5f;

private:
	uint32 gridSizeX, gridSizeY;
	float width, height;

	void solveVelocities(const std::vector<struct cloth_constraint_temp>& constraintsTemp);
	void solvePositions();

	vec3 getParticlePosition(float relX, float relY);

	void addConstraint(uint32 a, uint32 b);
};
