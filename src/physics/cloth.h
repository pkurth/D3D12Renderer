#pragma once

#include "core/math.h"
#include "bounding_volumes.h"

struct cloth_particle
{
	vec3 position;
	vec3 prevPosition;
	bool locked;
};

struct cloth_constraint
{
	uint32 a, b;
	float restDistance;
};

struct cloth_collider
{
	bounding_sphere sphere;
};

struct cloth_component
{
	cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, float totalMass, float thickness = 0.1f, float gravityFactor = 1.f, float damping = 0.05f);
	void simulate(uint32 iterations, float dt);
	void collide(cloth_collider c);

	std::vector<cloth_particle> particles;
	std::vector<cloth_constraint> constraints;

	float gravityFactor;
	float damping;
	float thickness;

private:
	uint32 gridSizeX, gridSizeY;
	uint32 numStretchConstraints;

	void addConstraint(uint32 a, uint32 b);
};
