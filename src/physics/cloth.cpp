#include "pch.h"
#include "cloth.h"
#include "physics.h"
#include "core/random.h"

cloth_component::cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, float totalMass, float thickness, float gravityFactor, float damping)
{
	this->thickness = thickness;
	this->gravityFactor = gravityFactor;
	this->damping = damping;
	this->gridSizeX = gridSizeX;
	this->gridSizeY = gridSizeY;

	float invMassPerParticle = (gridSizeX * gridSizeY) / totalMass;

	particles.reserve(gridSizeX * gridSizeY);

	random_number_generator rng = { 1578123 };

	for (uint32 y = 0; y < gridSizeY; ++y)
	{
		for (uint32 x = 0; x < gridSizeX; ++x)
		{
			cloth_particle& p = particles.emplace_back();
			float relX = x / (float)(gridSizeX - 1);
			float relY = y / (float)(gridSizeY - 1);
			p.position = vec3(relX * width, -relY * height, rng.randomFloatBetween(-0.01f, 0.01f));
			std::swap(p.position.y, p.position.z);
			p.prevPosition = p.position;
			p.locked = false;
		}
	}

	// Stretch constraints.
	for (uint32 y = 0; y < gridSizeY; ++y)
	{
		for (uint32 x = 0; x < gridSizeX; ++x)
		{
			uint32 index = y * gridSizeX + x;

			if (x < gridSizeX - 1)
			{
				addConstraint(index, index + 1);
			}
			if (y < gridSizeY - 1)
			{
				addConstraint(index, index + gridSizeX);
			}
		}
	}

	numStretchConstraints = (uint32)constraints.size();

#if 0
	// Shear/bend constraints.
	for (uint32 y = 0; y < gridSizeY; ++y)
	{
		for (uint32 x = 0; x < gridSizeX; ++x)
		{
			uint32 index = y * gridSizeX + x;

			if (x < gridSizeX - 1 && y < gridSizeY - 1)
			{
				addConstraint(index, index + gridSizeX + 1);
				addConstraint(index + gridSizeX, index + 1);
			}

			if (x < gridSizeX - 2)
			{
				addConstraint(index, index + 2);
			}
			if (y < gridSizeY - 2)
			{
				addConstraint(index, index + gridSizeX * 2);
			}
		}
	}
#endif
}

void cloth_component::simulate(uint32 iterations, float dt)
{
	//static float time = 0.f;
	//time += dt;
	//
	//for (uint32 i = 0; i < gridSizeX; ++i)
	//{
	//	float s = sin(time + (float)i * 0.1f);
	//	particles[i].position.z = s;
	//}

	float velocityScale = (1.f - damping);
	float gravity = GRAVITY * dt * dt * gravityFactor;
	float stiffness = 0.001f;

	for (cloth_particle& p : particles)
	{
		if (!p.locked)
		{
			vec3 positionBeforeUpdate = p.position;
			p.position += (p.position - p.prevPosition) * velocityScale;
			p.position.y += gravity;
			p.prevPosition = positionBeforeUpdate;
		}
	}

	for (uint32 iteration = 0; iteration < iterations; ++iteration)
	{
		for (uint32 constraintIndex = 0; constraintIndex < (uint32)constraints.size(); ++constraintIndex)
		{
			cloth_constraint& c = constraints[constraintIndex];

			cloth_particle& a = particles[c.a];
			cloth_particle& b = particles[c.b];

			vec3 aToB = b.position - a.position;
			float distance = length(aToB);
			aToB /= distance;
			vec3 offset = aToB * (distance - c.restDistance);

			if (constraintIndex >= numStretchConstraints)
			{
				offset *= stiffness;
			}

			if (!a.locked) { a.position += offset * 0.5f; }
			if (!b.locked) { b.position -= offset * 0.5f; }
		}
	}
}

void cloth_component::collide(cloth_collider c)
{
	for (auto& p : particles)
	{
		vec3 v = p.position - c.sphere.center;
		float l = length(v);
		if (l < c.sphere.radius)
		{
			p.position += normalize(v) * (c.sphere.radius - l);
		}
	}
}

void cloth_component::addConstraint(uint32 a, uint32 b)
{
	constraints.push_back(cloth_constraint{ a, b, length(particles[a].position - particles[b].position) });
}

