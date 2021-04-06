#include "pch.h"
#include "collision_narrow.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_gjk.h"
#include "collision_epa.h"

#include "imgui.h"


static void getTangents(vec3 normal, vec3& outTangent, vec3& outBitangent)
{
	if (abs(normal.x) >= 0.57735f)
	{
		outTangent = vec3(normal.y, -normal.x, 0.f);
	}
	else
	{
		outTangent = vec3(0.f, normal.z, -normal.y);
	}

	outTangent = normalize(outTangent);
	outBitangent = cross(normal, outTangent);
}


// Sphere tests.
static bool intersection(const bounding_sphere& s1, const bounding_sphere& s2, contact_manifold& outContact)
{
	vec3 n = s2.center - s1.center;
	float radiusSum = s2.radius + s1.radius;
	float sqDistance = squaredLength(n);
	if (sqDistance <= radiusSum * radiusSum)
	{
		float distance;
		if (sqDistance == 0.f) // Degenerate case.
		{
			distance = 0.f;
			outContact.collisionNormal = vec3(0.f, 1.f, 0.f); // Up.
		}
		else
		{
			distance = sqrt(sqDistance);
			outContact.collisionNormal = n / distance;
		}

		getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);
		outContact.numContacts = 1;
		outContact.contacts[0].penetrationDepth = radiusSum - distance; // Flipped to change sign.
		assert(outContact.contacts[0].penetrationDepth >= 0.f);
		outContact.contacts[0].point = 0.5f * (s1.center + s1.radius * outContact.collisionNormal + s2.center - s2.radius * outContact.collisionNormal);
		return true;
	}
	return false;
}

static bool intersection(const bounding_sphere& s, const bounding_capsule& c, contact_manifold& outContact)
{
	vec3 closestPoint = closestPoint_PointSegment(s.center, line_segment{ c.positionA, c.positionB });
	return intersection(s, bounding_sphere{ closestPoint, c.radius }, outContact);
}

static bool intersection(const bounding_sphere& s, const bounding_box& a, contact_manifold& outContact)
{
	vec3 p = closestPoint_PointAABB(s.center, a);
	vec3 n = p - s.center;
	float sqDistance = squaredLength(n);
	if (sqDistance <= s.radius * s.radius)
	{
		float dist = 0.f;
		if (sqDistance > 0.f)
		{
			dist = sqrt(sqDistance);
			n /= dist;
		}
		else
		{
			n = vec3(0.f, 1.f, 0.f);
		}

		outContact.numContacts = 1;

		outContact.collisionNormal = n;
		outContact.contacts[0].penetrationDepth = s.radius - dist; // Flipped to change sign.
		outContact.contacts[0].point = 0.5f * (p + s.center + n * s.radius);
		getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);

		return true;
	}
	return false;
}

static bool intersection(const bounding_sphere& s, const bounding_oriented_box& o, contact_manifold& outContact)
{
	bounding_box aabb = bounding_box::fromCenterRadius(o.center, o.radius);
	bounding_sphere s_ = { 
		conjugate(o.rotation)* (s.center - o.center) + o.center,
		s.radius };
	
	if (intersection(s_, aabb, outContact))
	{
		outContact.collisionNormal = o.rotation * outContact.collisionNormal;
		outContact.collisionTangent = o.rotation * outContact.collisionTangent;
		outContact.collisionBitangent = o.rotation * outContact.collisionBitangent;
		outContact.contacts[0].point = o.rotation * (outContact.contacts[0].point - o.center) + o.center;
		return true;
	}
	return false;
}

// Capsule tests.
static bool intersection(const bounding_capsule& c1, const bounding_capsule& c2, contact_manifold& outContact)
{
	vec3 closestPoint1, closestPoint2;
	closestPoint_SegmentSegment(line_segment{ c1.positionA, c1.positionB }, line_segment{ c2.positionA, c2.positionB }, closestPoint1, closestPoint2);
	return intersection(bounding_sphere{ closestPoint1, c1.radius }, bounding_sphere{ closestPoint2, c2.radius }, outContact);
}

static bool intersection(const bounding_capsule& c, const bounding_box& a, contact_manifold& outContact)
{
	capsule_support_fn capsuleSupport{ c };
	aabb_support_fn boxSupport{ a };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(capsuleSupport, boxSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, capsuleSupport, boxSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
}

static bool intersection(const bounding_capsule& c, const bounding_oriented_box& o, contact_manifold& outContact)
{
	bounding_box aabb = bounding_box::fromCenterRadius(o.center, o.radius);
	bounding_capsule c_ = { 
		conjugate(o.rotation) * (c.positionA - o.center) + o.center, 
		conjugate(o.rotation) * (c.positionB - o.center) + o.center,
		c.radius };

	if (intersection(c_, aabb, outContact))
	{
		outContact.collisionNormal = o.rotation * outContact.collisionNormal;
		outContact.collisionTangent = o.rotation * outContact.collisionTangent;
		outContact.collisionBitangent = o.rotation * outContact.collisionBitangent;

		for (uint32 i = 0; i < outContact.numContacts; ++i)
		{
			outContact.contacts[i].point = o.rotation * (outContact.contacts[i].point - o.center) + o.center;
		}
		return true;
	}
	return false;
}

// AABB tests.

uint32 narrowphase(collider_union* worldSpaceColliders, rigid_body_global_state* rbs, broadphase_collision* possibleCollisions, uint32 numPossibleCollisions, float dt,
	collision_constraint* outCollisionConstraints)
{
	uint32 numContacts = 0;

	for (uint32 i = 0; i < numPossibleCollisions; ++i)
	{
		broadphase_collision overlap = possibleCollisions[i];
		collider_union* colliderAInitial = worldSpaceColliders + overlap.colliderA;
		collider_union* colliderBInitial = worldSpaceColliders + overlap.colliderB;

		if (colliderAInitial->rigidBodyIndex != colliderBInitial->rigidBodyIndex)
		{
			contact_manifold& contact = outCollisionConstraints[numContacts].contact;
			bool collides = false;

			collider_union* colliderA = (colliderAInitial->type < colliderBInitial->type) ? colliderAInitial : colliderBInitial;
			collider_union* colliderB = (colliderAInitial->type < colliderBInitial->type) ? colliderBInitial : colliderAInitial;


			// Sphere tests.
			if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_sphere)
			{
				collides = intersection(colliderA->sphere, colliderB->sphere, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_capsule)
			{
				collides = intersection(colliderA->sphere, colliderB->capsule, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_aabb)
			{
				collides = intersection(colliderA->sphere, colliderB->aabb, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->sphere, colliderB->obb, contact);
			}
			
			// Capsule tests.
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_capsule)
			{
				collides = intersection(colliderA->capsule, colliderB->capsule, contact);
			}
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_aabb)
			{
				collides = intersection(colliderA->capsule, colliderB->aabb, contact);
			}
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->capsule, colliderB->obb, contact);
			}

			// AABB tests.



			if (collides)
			{
				collider_properties propsA = colliderA->properties;
				collider_properties propsB = colliderB->properties;

				float friction = sqrt(propsA.friction * propsB.friction);

				contact.colliderA = overlap.colliderA;
				contact.colliderB = overlap.colliderB;

				collision_constraint& c = outCollisionConstraints[numContacts];
				c.friction = friction;
				c.rbA = colliderA->rigidBodyIndex;
				c.rbB = colliderB->rigidBodyIndex;


				for (uint32 contactID = 0; contactID < contact.numContacts; ++contactID)
				{
					collision_point& point = c.points[contactID];
					contact_info& contact = c.contact.contacts[contactID];

					point.impulseInNormalDir = 0.f;
					point.impulseInTangentDir = 0.f;

					auto& rbA = rbs[c.rbA];
					auto& rbB = rbs[c.rbB];

					point.relGlobalAnchorA = contact.point - rbA.position;
					point.relGlobalAnchorB = contact.point - rbB.position;

					vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, point.relGlobalAnchorA);
					vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, point.relGlobalAnchorB);

					vec3 relVelocity = anchorVelocityB - anchorVelocityA;
					point.tangent = relVelocity - dot(c.contact.collisionNormal, relVelocity) * c.contact.collisionNormal;
					if (squaredLength(point.tangent) > 0.f)
					{
						point.tangent = normalize(point.tangent);
					}
					else
					{
						point.tangent = vec3(1.f, 0.f, 0.f);
					}

					{ // Tangent direction.
						vec3 crAt = cross(point.relGlobalAnchorA, point.tangent);
						vec3 crBt = cross(point.relGlobalAnchorB, point.tangent);
						float invMassInTangentDir = rbA.invMass + dot(crAt, rbA.invInertia * crAt)
							+ rbB.invMass + dot(crBt, rbB.invMass * crBt);
						point.effectiveMassInTangentDir = (invMassInTangentDir != 0.f) ? (1.f / invMassInTangentDir) : 0.f;
					}

					{ // Normal direction.
						vec3 crAn = cross(point.relGlobalAnchorA, c.contact.collisionNormal);
						vec3 crBn = cross(point.relGlobalAnchorB, c.contact.collisionNormal);
						float invMassInNormalDir = rbA.invMass + dot(crAn, rbA.invInertia * crAn)
							+ rbB.invMass + dot(crBn, rbB.invMass * crBn);
						point.effectiveMassInNormalDir = (invMassInNormalDir != 0.f) ? (1.f / invMassInNormalDir) : 0.f;

						point.bias = 0.f;

						if (dt > 0.f)
						{
							float vRel = dot(c.contact.collisionNormal, anchorVelocityB - anchorVelocityA);
							const float slop = -0.001f;
							if (-contact.penetrationDepth < slop && vRel < 0.f)
							{
								float restitution = max(propsA.restitution, propsB.restitution);
								point.bias = -restitution * vRel - 0.1f * (-contact.penetrationDepth - slop) / dt;
							}
						}
					}
				}


				++numContacts;
			}
		}
	}

	return numContacts;
}

void solveCollisionVelocityConstraints(collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		collision_constraint& c = constraints[i];

		auto& rbA = rbs[c.rbA];
		auto& rbB = rbs[c.rbB];

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		for (uint32 contactID = 0; contactID < c.contact.numContacts; ++contactID)
		{
			collision_point& point = c.points[contactID];
			contact_info& contact = c.contact.contacts[contactID];

			{ // Tangent dir
				vec3 anchorVelocityA = vA + cross(wA, point.relGlobalAnchorA);
				vec3 anchorVelocityB = vB + cross(wB, point.relGlobalAnchorB);

				vec3 relVelocity = anchorVelocityB - anchorVelocityA;
				float vt = dot(relVelocity, point.tangent);
				float lambda = -point.effectiveMassInTangentDir * vt;

				float maxFriction = c.friction * point.impulseInNormalDir;
				assert(maxFriction >= 0.f);
				float newImpulse = clamp(point.impulseInTangentDir + lambda, -maxFriction, maxFriction);
				lambda = newImpulse - point.impulseInTangentDir;
				point.impulseInTangentDir = newImpulse;

				vec3 P = lambda * point.tangent;
				vA -= rbA.invMass * P;
				wA -= rbA.invInertia * cross(point.relGlobalAnchorA, P);
				vB += rbB.invMass * P;
				wB += rbB.invInertia * cross(point.relGlobalAnchorB, P);
			}

			{ // Normal dir
				vec3 anchorVelocityA = vA + cross(wA, point.relGlobalAnchorA);
				vec3 anchorVelocityB = vB + cross(wB, point.relGlobalAnchorB);

				vec3 relVelocity = anchorVelocityB - anchorVelocityA;
				float vn = dot(relVelocity, c.contact.collisionNormal);
				float lambda = -point.effectiveMassInNormalDir * (vn - point.bias);
				float impulse = max(point.impulseInNormalDir + lambda, 0.f);
				lambda = impulse - point.impulseInNormalDir;
				point.impulseInNormalDir = impulse;

				vec3 P = lambda * c.contact.collisionNormal;
				vA -= rbA.invMass * P;
				wA -= rbA.invInertia * cross(point.relGlobalAnchorA, P);
				vB += rbB.invMass * P;
				wB += rbB.invInertia * cross(point.relGlobalAnchorB, P);
			}
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}
