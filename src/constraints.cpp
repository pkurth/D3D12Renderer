#include "pch.h"
#include "constraints.h"
#include "physics.h"

#define DISTANCE_CONSTRAINT_BETA 0.1f
#define BALL_JOINT_CONSTRAINT_BETA 0.1f
#define HINGE_ROTATION_CONSTRAINT_BETA 0.3f

#define DT_THRESHOLD 1e-5f

void initializeDistanceVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const distance_constraint* input, distance_constraint_update* output, uint32 count, float dt)
{
	auto rbView = appScene.view<rigid_body_component>();
	rigid_body_component* rbBase = rbView.raw();

	for (uint32 i = 0; i < count; ++i)
	{
		const distance_constraint& in = input[i];
		distance_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		trs& transformA = entityA.getComponent<trs>();
		trs& transformB = entityB.getComponent<trs>();

		out.rigidBodyIndexA = (uint16)(&rbA - rbBase);
		out.rigidBodyIndexB = (uint16)(&rbB - rbBase);

		rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to entity's origin.
		vec3 relGlobalAnchorA = transformA.rotation * in.localAnchorA;
		vec3 relGlobalAnchorB = transformA.rotation * in.localAnchorB;

		// Global.
		vec3 globalAnchorA = transformA.position + relGlobalAnchorA;
		vec3 globalAnchorB = transformB.position + relGlobalAnchorB;

		// Relative to COG.
		out.relGlobalAnchorA = transformA.rotation * (in.localAnchorA - rbA.localCOGPosition);
		out.relGlobalAnchorB = transformB.rotation * (in.localAnchorB - rbB.localCOGPosition);

		out.u = globalAnchorB - globalAnchorA;
		float l = length(out.u);
		if (l > 0.001f)
		{
			out.u *= 1.f / l;
		}
		else
		{
			out.u = vec3(0.f);
		}

		vec3 crAu = cross(out.relGlobalAnchorA, out.u);
		vec3 crBu = cross(out.relGlobalAnchorB, out.u);
		float invMass = globalA.invMass + dot(crAu, globalA.invInertia * crAu)
					  + globalB.invMass + dot(crBu, globalB.invInertia * crBu);
		out.effectiveMass = (invMass != 0.f) ? (1.f / invMass) : 0.f;

		out.bias = 0.f;
		if (dt > DT_THRESHOLD)
		{
			out.bias = (l - in.globalLength) * DISTANCE_CONSTRAINT_BETA / dt;
		}
	}
}

void solveDistanceVelocityConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		distance_constraint_update& con = constraints[i];

		rigid_body_global_state& rbA = rbs[con.rigidBodyIndexA];
		rigid_body_global_state& rbB = rbs[con.rigidBodyIndexB];

		vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, con.relGlobalAnchorA);
		vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, con.relGlobalAnchorB);
		float Cdot = dot(con.u, anchorVelocityB - anchorVelocityA) + con.bias;

		float impulse = -con.effectiveMass * Cdot;
		vec3 P = impulse * con.u;
		rbA.linearVelocity -= rbA.invMass * P;
		rbA.angularVelocity -= rbA.invInertia * cross(con.relGlobalAnchorA, P);
		rbB.linearVelocity += rbB.invMass * P;
		rbB.angularVelocity += rbB.invInertia * cross(con.relGlobalAnchorB, P);
	}
}

void initializeBallJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const ball_joint_constraint* input, ball_joint_constraint_update* output, uint32 count, float dt)
{
	auto rbView = appScene.view<rigid_body_component>();
	rigid_body_component* rbBase = rbView.raw();

	for (uint32 i = 0; i < count; ++i)
	{
		const ball_joint_constraint& in = input[i];
		ball_joint_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		trs& transformA = entityA.getComponent<trs>();
		trs& transformB = entityB.getComponent<trs>();

		out.rigidBodyIndexA = (uint16)(&rbA - rbBase);
		out.rigidBodyIndexB = (uint16)(&rbB - rbBase);

		rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to entity's origin.
		vec3 relGlobalAnchorA = transformA.rotation * in.localAnchorA;
		vec3 relGlobalAnchorB = transformB.rotation * in.localAnchorB;

		// Global.
		vec3 globalAnchorA = transformA.position + relGlobalAnchorA;
		vec3 globalAnchorB = transformB.position + relGlobalAnchorB;

		// Relative to COG.
		out.relGlobalAnchorA = transformA.rotation * (in.localAnchorA - rbA.localCOGPosition);
		out.relGlobalAnchorB = transformB.rotation * (in.localAnchorB - rbB.localCOGPosition);

		mat3 skewMatA = getSkewMatrix(out.relGlobalAnchorA);
		mat3 skewMatB = getSkewMatrix(out.relGlobalAnchorB);

		out.invEffectiveMass = mat3::identity * globalA.invMass + skewMatA * globalA.invInertia * transpose(skewMatA)
							 + mat3::identity * globalB.invMass + skewMatB * globalB.invInertia * transpose(skewMatB);

		out.bias = 0.f;
		if (dt > DT_THRESHOLD)
		{
			out.bias = (globalAnchorB - globalAnchorA) * BALL_JOINT_CONSTRAINT_BETA / dt;
		}
	}
}

void solveBallJointVelocityConstraints(ball_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		ball_joint_constraint_update& con = constraints[i];

		rigid_body_global_state& rbA = rbs[con.rigidBodyIndexA];
		rigid_body_global_state& rbB = rbs[con.rigidBodyIndexB];

		vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, con.relGlobalAnchorA);
		vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, con.relGlobalAnchorB);
		vec3 Cdot = anchorVelocityB - anchorVelocityA + con.bias;

		vec3 P = solveLinearSystem(con.invEffectiveMass, -Cdot);
		rbA.linearVelocity -= rbA.invMass * P;
		rbA.angularVelocity -= rbA.invInertia * cross(con.relGlobalAnchorA, P);
		rbB.linearVelocity += rbB.invMass * P;
		rbB.angularVelocity += rbB.invInertia * cross(con.relGlobalAnchorB, P);
	}
}

void initializeHingeJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const hinge_joint_constraint* input, hinge_joint_constraint_update* output, uint32 count, float dt)
{
	auto rbView = appScene.view<rigid_body_component>();
	rigid_body_component* rbBase = rbView.raw();

	for (uint32 i = 0; i < count; ++i)
	{
		const hinge_joint_constraint& in = input[i];
		hinge_joint_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		trs& transformA = entityA.getComponent<trs>();
		trs& transformB = entityB.getComponent<trs>();

		out.rigidBodyIndexA = (uint16)(&rbA - rbBase);
		out.rigidBodyIndexB = (uint16)(&rbB - rbBase);

		rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to entity's origin.
		vec3 relGlobalAnchorA = transformA.rotation * in.localAnchorA;
		vec3 relGlobalAnchorB = transformB.rotation * in.localAnchorB;

		// Global.
		vec3 globalAnchorA = transformA.position + relGlobalAnchorA;
		vec3 globalAnchorB = transformB.position + relGlobalAnchorB;

		// Relative to COG.
		out.relGlobalAnchorA = transformA.rotation * (in.localAnchorA - rbA.localCOGPosition);
		out.relGlobalAnchorB = transformB.rotation * (in.localAnchorB - rbB.localCOGPosition);




		// Position part.

		mat3 skewMatA = getSkewMatrix(out.relGlobalAnchorA);
		mat3 skewMatB = getSkewMatrix(out.relGlobalAnchorB);

		out.invEffectiveTranslationMass = mat3::identity * globalA.invMass + skewMatA * globalA.invInertia * transpose(skewMatA)
			+ mat3::identity * globalB.invMass + skewMatB * globalB.invInertia * transpose(skewMatB);

		out.translationBias = 0.f;
		if (dt > DT_THRESHOLD)
		{
			out.translationBias = (globalAnchorB - globalAnchorA) * BALL_JOINT_CONSTRAINT_BETA / dt;
		}



		// Rotation part.
		vec3 globalRotationAxisA = transformA.rotation * in.localAxisA;
		vec3 globalRotationAxisB = transformB.rotation * in.localAxisB;

		vec3 globalTangentB, globalBitangentB;
		getTangents(globalRotationAxisB, globalTangentB, globalBitangentB);

		vec3 bxa = cross(globalTangentB, globalRotationAxisA);
		vec3 cxa = cross(globalBitangentB, globalRotationAxisA);
		vec3 iAbxa = globalA.invInertia * bxa;
		vec3 iBbxa = globalB.invInertia * bxa;
		vec3 iAcxa = globalA.invInertia * cxa;
		vec3 iBcxa = globalB.invInertia * cxa;

		out.invEffectiveRotationMass.m00 = dot(bxa, iAbxa) + dot(bxa, iBbxa);
		out.invEffectiveRotationMass.m01 = dot(bxa, iAcxa) + dot(bxa, iBcxa);
		out.invEffectiveRotationMass.m10 = dot(cxa, iAbxa) + dot(cxa, iBbxa);
		out.invEffectiveRotationMass.m11 = dot(cxa, iAcxa) + dot(cxa, iBcxa);

		out.bxa = bxa;
		out.cxa = cxa;

		out.rotationBias = vec2(0.f, 0.f);
		if (dt > DT_THRESHOLD)
		{
			out.rotationBias = vec2(dot(globalRotationAxisA, globalTangentB), dot(globalRotationAxisA, globalBitangentB)) * HINGE_ROTATION_CONSTRAINT_BETA / dt;
		}
	}
}

void solveHingeJointVelocityConstraints(hinge_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		hinge_joint_constraint_update& con = constraints[i];

		rigid_body_global_state& rbA = rbs[con.rigidBodyIndexA];
		rigid_body_global_state& rbB = rbs[con.rigidBodyIndexB];


		// Position part.

		vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, con.relGlobalAnchorA);
		vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, con.relGlobalAnchorB);
		vec3 translationCdot = anchorVelocityB - anchorVelocityA + con.translationBias;

		vec3 translationP = solveLinearSystem(con.invEffectiveTranslationMass, -translationCdot);


		// Rotation part.

		vec3 deltaAngularVelocity = rbB.angularVelocity - rbA.angularVelocity;

		vec2 rotationCdot(dot(con.bxa, deltaAngularVelocity),
			dot(con.cxa, deltaAngularVelocity));
		vec2 rotLambda = solveLinearSystem(con.invEffectiveRotationMass, -(rotationCdot + con.rotationBias));

		vec3 rotationP = con.bxa * rotLambda.x + con.cxa * rotLambda.y;



		rbA.linearVelocity -= rbA.invMass * translationP;
		rbA.angularVelocity -= rbA.invInertia * (cross(con.relGlobalAnchorA, translationP) + rotationP);
		rbB.linearVelocity += rbB.invMass * translationP;
		rbB.angularVelocity += rbB.invInertia * (cross(con.relGlobalAnchorB, translationP) + rotationP);
	}
}
