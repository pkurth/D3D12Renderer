#include "pch.h"
#include "cloth.h"
#include "physics.h"
#include "core/random.h"

cloth_component::cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, float totalMass, float thickness, float damping, float gravityFactor)
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
			
			p.position = vec3(relX * width, -relY * height, 0.f);
			std::swap(p.position.y, p.position.z);
			p.prevPosition = p.position;

			p.invMass = invMassPerParticle;
			p.velocity = vec3(0.f);
			p.forceAccumulator = vec3(0.f);

			if (y == 0)
			{
				p.invMass = 0.f; // Lock.
			}
		}
	}

	for (uint32 y = 0; y < gridSizeY; ++y)
	{
		for (uint32 x = 0; x < gridSizeX; ++x)
		{
			uint32 index = y * gridSizeX + x;

			// Stretch constraints: direct right and bottom neighbor.
			if (x < gridSizeX - 1)
			{
				addConstraint(index, index + 1);
			}
			if (y < gridSizeY - 1)
			{
				addConstraint(index, index + gridSizeX);
			}

			// Shear constraints: direct diagonal neighbor.
			if (x < gridSizeX - 1 && y < gridSizeY - 1)
			{
				addConstraint(index, index + gridSizeX + 1);
				addConstraint(index + gridSizeX, index + 1);
			}

			// Bend constraints: neighbor right and bottom two places away.
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
}

struct cloth_constraint_temp
{
	vec3 gradient;
	float inverseScaledGradientSquared;
};

void cloth_component::simulate(uint32 velocityIterations, uint32 positionIterations, uint32 driftIterations, float dt)
{
	float gravityVelocity = GRAVITY * dt * gravityFactor;
	for (cloth_particle& p : particles)
	{
		if (p.invMass > 0.f)
		{
			p.velocity.y += gravityVelocity;
		}

		// TODO: Apply external forces.

		p.prevPosition = p.position;
		p.position = p.prevPosition + p.velocity * dt;
		p.forceAccumulator = vec3(0.f);
	}

	float invDt = (dt > 1e-5f) ? (1.f / dt) : 1.f;
	
	// Solve velocities.
	if (velocityIterations > 0)
	{
		std::vector<cloth_constraint_temp> constraintsTemp;
		constraintsTemp.reserve(constraints.size());

		for (cloth_constraint& c : constraints)
		{
			cloth_particle& a = particles[c.a];
			cloth_particle& b = particles[c.b];

			cloth_constraint_temp temp;
			temp.gradient = b.prevPosition - a.prevPosition;
			temp.inverseScaledGradientSquared = (c.inverseMassSum == 0.f) ? 0.f : (1.f / (squaredLength(temp.gradient) * c.inverseMassSum));
			constraintsTemp.push_back(temp);
		}


		for (uint32 it = 0; it < velocityIterations; ++it)
		{
			solveVelocities(constraintsTemp);
		}

		for (cloth_particle& p : particles)
		{
			p.position = p.prevPosition + p.velocity * dt;
		}
	}

	// Solve positions.
	if (positionIterations > 0)
	{
		for (uint32 it = 0; it < positionIterations; ++it)
		{
			solvePositions();
		}

		for (cloth_particle& p : particles)
		{
			p.velocity = (p.position - p.prevPosition) * invDt;
		}
	}

	// Solve drift.
	if (driftIterations > 0)
	{
		for (cloth_particle& p : particles)
		{
			p.prevPosition = p.position;
		}

		for (uint32 it = 0; it < driftIterations; ++it)
		{
			solvePositions();
		}

		for (cloth_particle& p : particles)
		{
			p.velocity += (p.position - p.prevPosition) * invDt;
		}
	}

	// Damping.
	float dampingFactor = 1.f / (1.f + dt * damping);
	for (cloth_particle& p : particles)
	{
		p.velocity *= dampingFactor;
	}
}

void cloth_component::solveVelocities(const std::vector<cloth_constraint_temp>& constraintsTemp)
{
	for (uint32 i = 0; i < (uint32)constraints.size(); ++i)
	{
		cloth_constraint& c = constraints[i];
		const cloth_constraint_temp& temp = constraintsTemp[i];
		cloth_particle& a = particles[c.a];
		cloth_particle& b = particles[c.b];

		float j = -dot(temp.gradient, a.velocity - b.velocity) * temp.inverseScaledGradientSquared;
		a.velocity += temp.gradient * (j * a.invMass);
		b.velocity -= temp.gradient * (j * b.invMass);
	}
}

void cloth_component::solvePositions()
{
	for (cloth_constraint& c : constraints)
	{
		if (c.inverseMassSum > 0.f)
		{
			cloth_particle& a = particles[c.a];
			cloth_particle& b = particles[c.b];

			vec3 delta = b.position - a.position;
			float len = squaredLength(delta);

			float sqRestDistance = c.restDistance * c.restDistance;
			if (sqRestDistance + len > 1e-5f)
			{
				float k = ((sqRestDistance - len) / (c.inverseMassSum * (sqRestDistance + len)));
				a.position -= delta * (k * a.invMass);
				b.position += delta * (k * b.invMass);
			}
		}
	}
}

void cloth_component::addConstraint(uint32 indexA, uint32 indexB)
{
	const cloth_particle& a = particles[indexA];
	const cloth_particle& b = particles[indexB];
	constraints.push_back(cloth_constraint
		{ 
			indexA, 
			indexB, 
			length(a.position - b.position),
			(a.invMass + b.invMass) / stiffness,
		});
}

