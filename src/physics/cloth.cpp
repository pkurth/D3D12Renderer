#include "pch.h"
#include "cloth.h"
#include "physics.h"
#include "core/cpu_profiling.h"
#include "dx/dx_context.h"

cloth_component::cloth_component(float width, float height, uint32 gridSizeX, uint32 gridSizeY, float totalMass, float stiffness, float damping, float gravityFactor)
	: gridSizeX(gridSizeX), gridSizeY(gridSizeY), width(width), height(height)
{
	this->gravityFactor = gravityFactor;
	this->damping = damping;
	this->totalMass = totalMass;
	this->stiffness = stiffness;

	uint32 numParticles = gridSizeX * gridSizeY;

	float invMassPerParticle = numParticles / totalMass;

	positions.reserve(numParticles);
	prevPositions.reserve(numParticles);
	invMasses.reserve(numParticles);
	velocities.resize(numParticles, vec3(0.f));
	forceAccumulators.resize(numParticles, vec3(0.f));

	random_number_generator rng = { 1578123 };

	for (uint32 y = 0; y < gridSizeY; ++y)
	{
		float invMass = (y == 0) ? 0.f : invMassPerParticle; // Lock upper row.

		for (uint32 x = 0; x < gridSizeX; ++x)
		{
			float relX = x / (float)(gridSizeX - 1);
			float relY = y / (float)(gridSizeY - 1);
			
			vec3 position = getParticlePosition(relX, relY);
			positions.push_back(position);
			prevPositions.push_back(position);
			invMasses.push_back(invMass);
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

void cloth_component::setWorldPositionOfFixedVertices(const trs& transform, bool moveRigid)
{
	if (moveRigid)
	{
		vec3 pivot;
		if (gridSizeX % 2 == 1)
		{
			pivot = positions[gridSizeX / 2];
		}
		else
		{
			pivot = (positions[gridSizeX / 2] + positions[gridSizeX / 2 - 1]) * 0.5f;
		}

		vec3 currentAxis = normalize(positions[gridSizeX - 1] - positions[0]);
		vec3 newAxis = normalize(transformPosition(transform, getParticlePosition(1.f, 0.f)) - transformPosition(transform, getParticlePosition(0.f, 0.f)));

		vec3 newPivot = transformPosition(transform, getParticlePosition(0.5f, 0.f));

		quat deltaRotation = rotateFromTo(currentAxis, newAxis);
		vec3 deltaTranslation = newPivot - pivot;

		for (uint32 y = 1; y < gridSizeY; ++y)
		{
			for (uint32 x = 0; x < gridSizeX; ++x)
			{
				vec3& position = positions[y * gridSizeX + x];
				position = deltaRotation * (position - pivot) + newPivot;
			}
		}
	}

	// Currently the top row is fixed, so transform this.
	for (uint32 x = 0; x < gridSizeX; ++x)
	{
		float relX = x / (float)(gridSizeX - 1);
		float relY = 0.f;
		vec3 localPosition = getParticlePosition(relX, relY);
		positions[x] = transformPosition(transform, localPosition);
	}
}

vec3 cloth_component::getParticlePosition(float relX, float relY)
{
	vec3 position = vec3(relX * width, -relY * height, 0.f);
	position.x -= width * 0.5f;
	std::swap(position.y, position.z);
	return position;
}

static vec3 calculateNormal(vec3 a, vec3 b, vec3 c)
{
	return cross(b - a, c - a);
}

void cloth_component::applyWindForce(vec3 force)
{
	for (uint32 y = 0; y < gridSizeY - 1; ++y)
	{
		for (uint32 x = 0; x < gridSizeX - 1; ++x)
		{
			uint32 tlIndex = y * gridSizeX + x;
			uint32 trIndex = tlIndex + 1;
			uint32 blIndex = tlIndex + gridSizeX;
			uint32 brIndex = blIndex + 1;

			vec3& tlForce = forceAccumulators[tlIndex];
			vec3& trForce = forceAccumulators[trIndex];
			vec3& blForce = forceAccumulators[blIndex];
			vec3& brForce = forceAccumulators[brIndex];

			{
				vec3 normal = calculateNormal(positions[tlIndex], positions[blIndex], positions[trIndex]);
				vec3 forceInNormalDir = normal * dot(normalize(normal), force);
				forceInNormalDir *= 1.f / 3.f;
				tlForce += forceInNormalDir;
				trForce += forceInNormalDir;
				blForce += forceInNormalDir;
			}

			{
				vec3 normal = calculateNormal(positions[brIndex], positions[trIndex], positions[blIndex]);
				vec3 forceInNormalDir = normal * dot(normalize(normal), force);
				forceInNormalDir *= 1.f / 3.f;
				brForce += forceInNormalDir;
				trForce += forceInNormalDir;
				blForce += forceInNormalDir;
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
	CPU_PROFILE_BLOCK("Simulate cloth");

	float gravityVelocity = GRAVITY * dt * gravityFactor;
	uint32 numParticles = gridSizeX * gridSizeY;

	for (uint32 i = 0; i < numParticles; ++i)
	{
		vec3& position = positions[i];
		vec3& prevPosition = prevPositions[i];
		float invMass = invMasses[i];
		vec3& velocity = velocities[i];
		vec3& force = forceAccumulators[i];

		if (invMass > 0.f)
		{
			velocity.y += gravityVelocity;
		}

		velocity += force * (invMass * dt);

		prevPosition = position;
		position += velocity * dt;
		force = vec3(0.f);
	}

	float invDt = (dt > 1e-5f) ? (1.f / dt) : 1.f;
	
	// Solve velocities.
	if (velocityIterations > 0)
	{
		std::vector<cloth_constraint_temp> constraintsTemp;
		constraintsTemp.reserve(constraints.size());

		for (cloth_constraint& c : constraints)
		{
			vec3 prevPositionA = prevPositions[c.a];
			vec3 prevPositionB = prevPositions[c.b];

			cloth_constraint_temp temp;
			temp.gradient = prevPositionB - prevPositionA;
			temp.inverseScaledGradientSquared = (c.inverseMassSum == 0.f) ? 0.f : (1.f / (squaredLength(temp.gradient) * c.inverseMassSum));
			constraintsTemp.push_back(temp);
		}


		for (uint32 it = 0; it < velocityIterations; ++it)
		{
			solveVelocities(constraintsTemp);
		}

		for (uint32 i = 0; i < numParticles; ++i)
		{
			positions[i] = prevPositions[i] + velocities[i] * dt;
		}
	}

	// Solve positions.
	if (positionIterations > 0)
	{
		for (uint32 it = 0; it < positionIterations; ++it)
		{
			solvePositions();
		}

		for (uint32 i = 0; i < numParticles; ++i)
		{
			velocities[i] = (positions[i] - prevPositions[i]) * invDt;
		}
	}

	// Solve drift.
	if (driftIterations > 0)
	{
		for (uint32 i = 0; i < numParticles; ++i)
		{
			prevPositions[i] = positions[i];
		}

		for (uint32 it = 0; it < driftIterations; ++it)
		{
			solvePositions();
		}

		for (uint32 i = 0; i < numParticles; ++i)
		{
			velocities[i] += (positions[i] - prevPositions[i]) * invDt;
		}
	}

	// Damping.
	float dampingFactor = 1.f / (1.f + dt * damping);
	for (uint32 i = 0; i < numParticles; ++i)
	{
		velocities[i] *= dampingFactor;
	}
}

void cloth_component::solveVelocities(const std::vector<cloth_constraint_temp>& constraintsTemp)
{
	for (uint32 i = 0; i < (uint32)constraints.size(); ++i)
	{
		cloth_constraint& c = constraints[i];
		const cloth_constraint_temp& temp = constraintsTemp[i];
		float j = -dot(temp.gradient, velocities[c.a] - velocities[c.b]) * temp.inverseScaledGradientSquared;
		velocities[c.a] += temp.gradient * (j * invMasses[c.a]);
		velocities[c.b] -= temp.gradient * (j * invMasses[c.b]);
	}
}

void cloth_component::solvePositions()
{
	for (cloth_constraint& c : constraints)
	{
		if (c.inverseMassSum > 0.f)
		{
			vec3 delta = positions[c.b] - positions[c.a];
			float len = squaredLength(delta);

			float sqRestDistance = c.restDistance * c.restDistance;
			if (sqRestDistance + len > 1e-5f)
			{
				float k = ((sqRestDistance - len) / (c.inverseMassSum * (sqRestDistance + len)));
				positions[c.a] -= delta * (k * invMasses[c.a]);
				positions[c.b] += delta * (k * invMasses[c.b]);
			}
		}
	}
}

void cloth_component::addConstraint(uint32 indexA, uint32 indexB)
{
	constraints.push_back(cloth_constraint
		{ 
			indexA, 
			indexB, 
			length(positions[indexA] - positions[indexB]),
			(invMasses[indexA] + invMasses[indexB]) / stiffness,
		});
}

void cloth_component::recalculateProperties()
{
	uint32 numParticles = gridSizeX * gridSizeY;
	float invMassPerParticle = numParticles / totalMass;
	for (float& invMass : invMasses)
	{
		invMass = (invMass != 0.f) ? invMassPerParticle : 0.f;
	}

	stiffness = clamp(stiffness, 0.01f, 1.f);
	float invStiffness = 1.f / stiffness;
	for (cloth_constraint& c : constraints)
	{
		c.inverseMassSum = (invMasses[c.a] + invMasses[c.b]) * invStiffness;
	}
}





#ifndef PHYSICS_ONLY
#include "animation/skinning.h"

std::tuple<dx_vertex_buffer_group_view, dx_vertex_buffer_group_view, dx_index_buffer_view, submesh_info> cloth_render_component::getRenderData(const cloth_component& cloth)
{
	CPU_PROFILE_BLOCK("Get cloth render data");

	uint32 numVertices = cloth.gridSizeX * cloth.gridSizeY;
	uint32 numTriangles = (cloth.gridSizeX - 1) * (cloth.gridSizeY - 1) * 2;

	auto [positionVertexBuffer, positionPtr] = dxContext.createDynamicVertexBuffer(sizeof(vec3), numVertices);
	memcpy(positionPtr, cloth.positions.data(), numVertices * sizeof(vec3));

	dx_vertex_buffer_group_view vb = skinCloth(positionVertexBuffer, cloth.gridSizeX, cloth.gridSizeY);
	submesh_info sm;
	sm.baseVertex = 0;
	sm.firstIndex = 0;
	sm.numIndices = numTriangles * 3;
	sm.numVertices = numVertices;

	dx_vertex_buffer_group_view prev = prevFrameVB;
	prevFrameVB = vb;



	if (!indexBuffer)
	{
		std::vector<indexed_triangle16> triangles;
		triangles.reserve(numTriangles);
		for (uint32 y = 0; y < cloth.gridSizeY - 1; ++y)
		{
			for (uint32 x = 0; x < cloth.gridSizeX - 1; ++x)
			{
				uint16 tlIndex = y * cloth.gridSizeX + x;
				uint16 trIndex = tlIndex + 1;
				uint16 blIndex = tlIndex + cloth.gridSizeX;
				uint16 brIndex = blIndex + 1;

				triangles.push_back({ tlIndex, blIndex, brIndex });
				triangles.push_back({ tlIndex, brIndex, trIndex });
			}
		}

		indexBuffer = createIndexBuffer(sizeof(uint16), (uint32)triangles.size() * 3, triangles.data());
	}


	return { vb, prev, indexBuffer, sm };
}

#endif

