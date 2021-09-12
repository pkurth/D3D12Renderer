#include "pch.h"
#include "constraints.h"
#include "physics.h"

#define DISTANCE_CONSTRAINT_BETA 0.1f
#define BALL_JOINT_CONSTRAINT_BETA 0.1f
#define HINGE_ROTATION_CONSTRAINT_BETA 0.3f
#define HINGE_LIMIT_CONSTRAINT_BETA 0.1f
#define TWIST_LIMIT_CONSTRAINT_BETA 0.1f

#define DT_THRESHOLD 1e-5f

void initializeDistanceVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const distance_constraint* input, distance_constraint_update* output, uint32 count, float dt)
{
	rigid_body_component* rbBase = appScene.raw<rigid_body_component>();

	for (uint32 i = 0; i < count; ++i)
	{
		const distance_constraint& in = input[i];
		distance_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		transform_component& transformA = entityA.getComponent<transform_component>();
		transform_component& transformB = entityB.getComponent<transform_component>();

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

		float lambda = -con.effectiveMass * Cdot;
		vec3 P = lambda * con.u;
		rbA.linearVelocity -= rbA.invMass * P;
		rbA.angularVelocity -= rbA.invInertia * cross(con.relGlobalAnchorA, P);
		rbB.linearVelocity += rbB.invMass * P;
		rbB.angularVelocity += rbB.invInertia * cross(con.relGlobalAnchorB, P);
	}
}

void initializeBallJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const ball_joint_constraint* input, ball_joint_constraint_update* output, uint32 count, float dt)
{
	rigid_body_component* rbBase = appScene.raw<rigid_body_component>();

	for (uint32 i = 0; i < count; ++i)
	{
		const ball_joint_constraint& in = input[i];
		ball_joint_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		transform_component& transformA = entityA.getComponent<transform_component>();
		transform_component& transformB = entityB.getComponent<transform_component>();

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
	rigid_body_component* rbBase = appScene.raw<rigid_body_component>();

	for (uint32 i = 0; i < count; ++i)
	{
		const hinge_joint_constraint& in = input[i];
		hinge_joint_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		transform_component& transformA = entityA.getComponent<transform_component>();
		transform_component& transformB = entityB.getComponent<transform_component>();

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




		// Position part. Identical to ball joint.

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
		vec3 globalHingeAxisA = transformA.rotation * in.localHingeAxisA;
		vec3 globalHingeAxisB = transformB.rotation * in.localHingeAxisB;

		vec3 globalTangentB, globalBitangentB;
		getTangents(globalHingeAxisB, globalTangentB, globalBitangentB);

		vec3 bxa = cross(globalTangentB, globalHingeAxisA);
		vec3 cxa = cross(globalBitangentB, globalHingeAxisA);
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
			out.rotationBias = vec2(dot(globalHingeAxisA, globalTangentB), dot(globalHingeAxisA, globalBitangentB)) * HINGE_ROTATION_CONSTRAINT_BETA / dt;
		}


		// Limits and motor.
		out.solveLimit = false;
		out.solveMotor = false;

		if (in.minRotationLimit <= 0.f || in.maxRotationLimit >= 0.f || in.maxMotorTorque > 0.f)
		{
			vec3 localHingeCompareA = conjugate(transformA.rotation) * (transformB.rotation * in.localHingeTangentB);
			float angle = atan2(dot(localHingeCompareA, in.localHingeBitangentA), dot(localHingeCompareA, in.localHingeTangentA));

			bool minLimitViolated = in.minRotationLimit <= 0.f && angle <= in.minRotationLimit;
			bool maxLimitViolated = in.maxRotationLimit >= 0.f && angle >= in.maxRotationLimit;

			assert(!(minLimitViolated && maxLimitViolated));

			out.solveLimit = minLimitViolated || maxLimitViolated;
			out.solveMotor = in.maxMotorTorque > 0.f;
			if (out.solveLimit || out.solveMotor)
			{
				out.globalRotationAxis = globalHingeAxisA;
				out.limitImpulse = 0.f;

				float invEffectiveAxialMass = dot(globalHingeAxisA, globalA.invInertia * globalHingeAxisA)
											+ dot(globalHingeAxisA, globalB.invInertia * globalHingeAxisA);

				out.effectiveAxialMass = (invEffectiveAxialMass != 0.f) ? (1.f / invEffectiveAxialMass) : 0.f;
				out.limitSign = minLimitViolated ? 1.f : -1.f;

				out.maxMotorImpulse = in.maxMotorTorque * dt;
				out.motorImpulse = 0.f;

				out.motorVelocity = in.motorVelocity;
				if (in.motorType == constraint_position_motor)
				{
					// Inspired by Bullet Engine. We set the velocity such that the target angle is reached within one frame.
					// This will later get clamped to the maximum motor impulse.
					float minLimit = (in.minRotationLimit <= 0.f) ? in.minRotationLimit : -M_PI;
					float maxLimit = (in.maxRotationLimit >= 0.f) ? in.maxRotationLimit : M_PI;
					float targetAngle = clamp(in.motorTargetAngle, minLimit, maxLimit);
					out.motorVelocity = (dt > DT_THRESHOLD) ? ((targetAngle - angle) / dt) : 0.f;
				}

				out.limitBias = 0.f;
				if (dt > DT_THRESHOLD)
				{
					float d = minLimitViolated ? (angle - in.minRotationLimit) : (in.maxRotationLimit - angle);
					out.limitBias = d * HINGE_LIMIT_CONSTRAINT_BETA / dt;
				}
			}
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

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		// Solve in order of importance (most important last): Motor -> Limits -> Rotation -> Position.

		vec3 globalRotationAxis = con.globalRotationAxis;

		// Motor.
		if (con.solveMotor)
		{
			float aDotWA = dot(globalRotationAxis, wA); // How fast are we turning about the axis.
			float aDotWB = dot(globalRotationAxis, wB);

			float relAngularVelocity = (aDotWB - aDotWA);
			float motorCdot = relAngularVelocity - con.motorVelocity;

			float motorLambda = -con.effectiveAxialMass * motorCdot;
			float oldImpulse = con.motorImpulse;
			con.motorImpulse = clamp(con.motorImpulse + motorLambda, -con.maxMotorImpulse, con.maxMotorImpulse);
			motorLambda = con.motorImpulse - oldImpulse;

			vec3 P = globalRotationAxis * motorLambda;
			wA -= rbA.invInertia * P;
			wB += rbB.invInertia * P;
		}

		// Limits.
		if (con.solveLimit)
		{
			float limitSign = con.limitSign;

			float aDotWA = dot(globalRotationAxis, wA); // How fast are we turning about the axis.
			float aDotWB = dot(globalRotationAxis, wB);
			float relAngularVelocity = limitSign * (aDotWB - aDotWA);

			float limitCdot = relAngularVelocity + con.limitBias;
			float limitLambda = -con.effectiveAxialMass * limitCdot;

			float impulse = max(con.limitImpulse + limitLambda, 0.f);
			limitLambda = impulse - con.limitImpulse;
			con.limitImpulse = impulse;

			limitLambda *= limitSign;

			vec3 P = globalRotationAxis * limitLambda;
			wA -= rbA.invInertia * P;
			wB += rbB.invInertia * P;
		}

		// Rotation part.
		{
			vec3 deltaAngularVelocity = wB - wA;

			vec2 rotationCdot(dot(con.bxa, deltaAngularVelocity), dot(con.cxa, deltaAngularVelocity));
			vec2 rotLambda = solveLinearSystem(con.invEffectiveRotationMass, -(rotationCdot + con.rotationBias));

			vec3 rotationP = con.bxa * rotLambda.x + con.cxa * rotLambda.y;

			wA -= rbA.invInertia * rotationP;
			wB += rbB.invInertia * rotationP;
		}

		// Position part.
		{
			vec3 anchorVelocityA = vA + cross(wA, con.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, con.relGlobalAnchorB);
			vec3 translationCdot = anchorVelocityB - anchorVelocityA + con.translationBias;

			vec3 translationP = solveLinearSystem(con.invEffectiveTranslationMass, -translationCdot);

			vA -= rbA.invMass * translationP;
			wA -= rbA.invInertia * cross(con.relGlobalAnchorA, translationP);
			vB += rbB.invMass * translationP;
			wB += rbB.invInertia * cross(con.relGlobalAnchorB, translationP);
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}

void initializeConeTwistVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const cone_twist_constraint* input, cone_twist_constraint_update* output, uint32 count, float dt)
{
	rigid_body_component* rbBase = appScene.raw<rigid_body_component>();

	for (uint32 i = 0; i < count; ++i)
	{
		const cone_twist_constraint& in = input[i];
		cone_twist_constraint_update& out = output[i];

		scene_entity entityA = { in.entityA, appScene };
		scene_entity entityB = { in.entityB, appScene };

		rigid_body_component& rbA = entityA.getComponent<rigid_body_component>();
		rigid_body_component& rbB = entityB.getComponent<rigid_body_component>();

		transform_component& transformA = entityA.getComponent<transform_component>();
		transform_component& transformB = entityB.getComponent<transform_component>();

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


		// Limits and motors.

		quat btoa = conjugate(transformA.rotation) * transformB.rotation;

		vec3 localLimitAxisA = in.localLimitAxisA;
		vec3 localLimitAxisCompareA = btoa * in.localLimitAxisB;

		quat swingRotation = rotateFromTo(localLimitAxisA, localLimitAxisCompareA);

		vec3 twistTangentA = swingRotation * in.localLimitTangentA;
		vec3 twistBitangentA = swingRotation * in.localLimitBitangentA;
		vec3 localLimitTangentCompareA = btoa * in.localLimitTangentB;
		float twistAngle = atan2(dot(localLimitTangentCompareA, twistBitangentA), dot(localLimitTangentCompareA, twistTangentA));



		// Swing limit.
		vec3 swingAxis; float swingAngle;
		getAxisRotation(swingRotation, swingAxis, swingAngle);
		if (swingAngle < 0.f)
		{
			swingAngle *= -1.f;
			swingAxis *= -1.f;
		}

		out.solveSwingLimit = in.swingLimit >= 0.f && swingAngle >= in.swingLimit;
		if (out.solveSwingLimit)
		{
			out.swingImpulse = 0.f;
			out.globalSwingAxis = transformA.rotation * swingAxis;
			float invEffectiveLimitMass = dot(out.globalSwingAxis, globalA.invInertia * out.globalSwingAxis)
										+ dot(out.globalSwingAxis, globalB.invInertia * out.globalSwingAxis);
			out.effectiveSwingLimitMass = (invEffectiveLimitMass != 0.f) ? (1.f / invEffectiveLimitMass) : 0.f;

			out.swingLimitBias = 0.f;
			if (dt > DT_THRESHOLD)
			{
				out.swingLimitBias = (in.swingLimit - swingAngle) * HINGE_LIMIT_CONSTRAINT_BETA / dt;
			}
		}

		// Swing motor.
		out.solveSwingMotor = in.maxSwingMotorTorque > 0.f;
		if (out.solveSwingMotor)
		{
			out.maxSwingMotorImpulse = in.maxSwingMotorTorque * dt;
			out.swingMotorImpulse = 0.f;

			float axisX = cos(in.swingMotorAxis), axisY = sin(in.swingMotorAxis);
			vec3 localSwingMotorAxis = axisX * in.localLimitTangentA + axisY * in.localLimitBitangentA;

			if (in.swingMotorType == constraint_velocity_motor)
			{
				out.globalSwingMotorAxis = transformA.rotation * localSwingMotorAxis;
				out.swingMotorVelocity = in.swingMotorVelocity;
			}
			else
			{
				float targetAngle = in.swingMotorTargetAngle;
				if (in.swingLimit >= 0.f)
				{
					targetAngle = clamp(targetAngle, -in.swingLimit, in.swingLimit);
				}

				vec3 localTargetDirection = quat(localSwingMotorAxis, targetAngle) * localLimitAxisA;
				vec3 localSwingMotorAxis = noz(cross(localLimitAxisCompareA, localTargetDirection));
				out.globalSwingMotorAxis = transformA.rotation * localSwingMotorAxis;

				float cosAngle = dot(localTargetDirection, localLimitAxisCompareA);
				float deltaAngle = acos(clamp01(cosAngle));
				out.swingMotorVelocity = (dt > DT_THRESHOLD) ? (deltaAngle / dt * 0.2f) : 0.f;
			}

			float invEffectiveMotorMass = dot(out.globalSwingMotorAxis, globalA.invInertia * out.globalSwingMotorAxis)
										+ dot(out.globalSwingMotorAxis, globalB.invInertia * out.globalSwingMotorAxis);
			out.effectiveSwingMotorMass = (invEffectiveMotorMass != 0.f) ? (1.f / invEffectiveMotorMass) : 0.f;
		}

		// Twist limit and motor.
		bool minTwistLimitViolated = in.twistLimit >= 0.f && twistAngle <= -in.twistLimit;
		bool maxTwistLimitViolated = in.twistLimit >= 0.f && twistAngle >= in.twistLimit; 
		assert(!(minTwistLimitViolated && maxTwistLimitViolated));

		out.solveTwistLimit = minTwistLimitViolated || maxTwistLimitViolated;
		out.solveTwistMotor = in.maxTwistMotorTorque > 0.f;
		if (out.solveTwistLimit || out.solveTwistMotor)
		{
			out.twistImpulse = 0.f;
			out.globalTwistAxis = transformA.rotation * localLimitAxisA;
			float invEffectiveMass = dot(out.globalTwistAxis, globalA.invInertia * out.globalTwistAxis)
								   + dot(out.globalTwistAxis, globalB.invInertia * out.globalTwistAxis);
			out.effectiveTwistMass = (invEffectiveMass != 0.f) ? (1.f / invEffectiveMass) : 0.f;

			out.twistLimitSign = minTwistLimitViolated ? 1.f : -1.f;

			out.maxTwistMotorImpulse = in.maxTwistMotorTorque * dt;
			out.twistMotorImpulse = 0.f;

			out.twistMotorVelocity = in.twistMotorVelocity;
			if (in.twistMotorType == constraint_position_motor)
			{
				// Inspired by Bullet Engine. We set the velocity such that the target angle is reached within one frame.
				// This will later get clamped to the maximum motor impulse.
				float limit = (in.twistLimit >= 0.f) ? in.twistLimit : M_PI;
				float targetAngle = clamp(in.twistMotorTargetAngle, -limit, limit);
				out.twistMotorVelocity = (dt > DT_THRESHOLD) ? ((targetAngle - twistAngle) / dt) : 0.f;
			}

			out.twistLimitBias = 0.f;
			if (dt > DT_THRESHOLD)
			{
				float d = minTwistLimitViolated ? (in.twistLimit + twistAngle) : (in.twistLimit - twistAngle);
				out.twistLimitBias = d * TWIST_LIMIT_CONSTRAINT_BETA / dt;
			}
		}
	}
}

void solveConeTwistVelocityConstraints(cone_twist_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		cone_twist_constraint_update& con = constraints[i];

		rigid_body_global_state& rbA = rbs[con.rigidBodyIndexA];
		rigid_body_global_state& rbB = rbs[con.rigidBodyIndexB];

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		// Solve in order of importance (most important last): Motors -> Limits -> Position.

		vec3 globalTwistAxis = con.globalTwistAxis;

		// Motor.
		if (con.solveTwistMotor)
		{
			float aDotWA = dot(globalTwistAxis, wA);
			float aDotWB = dot(globalTwistAxis, wB);
			float relAngularVelocity = (aDotWB - aDotWA);

			float motorCdot = relAngularVelocity - con.twistMotorVelocity;

			float motorLambda = -con.effectiveTwistMass * motorCdot;
			float oldImpulse = con.twistMotorImpulse;
			con.twistMotorImpulse = clamp(con.twistMotorImpulse + motorLambda, -con.maxTwistMotorImpulse, con.maxTwistMotorImpulse);
			motorLambda = con.twistMotorImpulse - oldImpulse;

			vec3 P = globalTwistAxis * motorLambda;
			wA -= rbA.invInertia * P;
			wB += rbB.invInertia * P;
		}

		if (con.solveSwingMotor)
		{
			vec3 globalSwingMotorAxis = con.globalSwingMotorAxis;

			float aDotWA = dot(globalSwingMotorAxis, wA);
			float aDotWB = dot(globalSwingMotorAxis, wB);
			float relAngularVelocity = (aDotWB - aDotWA);

			float motorCdot = relAngularVelocity - con.swingMotorVelocity;

			float motorLambda = -con.effectiveSwingMotorMass * motorCdot;
			float oldImpulse = con.swingMotorImpulse;
			con.swingMotorImpulse = clamp(con.swingMotorImpulse + motorLambda, -con.maxSwingMotorImpulse, con.maxSwingMotorImpulse);
			motorLambda = con.swingMotorImpulse - oldImpulse;

			vec3 P = globalSwingMotorAxis * motorLambda;
			wA -= rbA.invInertia * P;
			wB += rbB.invInertia * P;
		}

		// Twist.
		if (con.solveTwistLimit)
		{
			float limitSign = con.twistLimitSign;

			float aDotWA = dot(globalTwistAxis, wA);
			float aDotWB = dot(globalTwistAxis, wB);
			float relAngularVelocity = limitSign * (aDotWB - aDotWA);

			float limitCdot = relAngularVelocity + con.twistLimitBias;
			float limitLambda = -con.effectiveTwistMass * limitCdot;

			float impulse = max(con.twistImpulse + limitLambda, 0.f);
			limitLambda = impulse - con.twistImpulse;
			con.twistImpulse = impulse;

			limitLambda *= limitSign;

			vec3 P = globalTwistAxis * limitLambda;
			wA -= rbA.invInertia * P;
			wB += rbB.invInertia * P;
		}

		// Cone.
		if (con.solveSwingLimit)
		{
			float aDotWA = dot(con.globalSwingAxis, wA);
			float aDotWB = dot(con.globalSwingAxis, wB);
			float swingLimitCdot = aDotWA - aDotWB + con.swingLimitBias;
			float swingLimitLambda = -con.effectiveSwingLimitMass * swingLimitCdot;

			float impulse = max(con.swingImpulse + swingLimitLambda, 0.f);
			swingLimitLambda = impulse - con.swingImpulse;
			con.swingImpulse = impulse;

			vec3 P = con.globalSwingAxis * swingLimitLambda;
			wA += rbA.invInertia * P;
			wB -= rbB.invInertia * P;
		}


		// Position part.
		{
			vec3 anchorVelocityA = vA + cross(wA, con.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, con.relGlobalAnchorB);
			vec3 translationCdot = anchorVelocityB - anchorVelocityA + con.bias;

			vec3 translationP = solveLinearSystem(con.invEffectiveMass, -translationCdot);

			vA -= rbA.invMass * translationP;
			wA -= rbA.invInertia * cross(con.relGlobalAnchorA, translationP);
			vB += rbB.invMass * translationP;
			wB += rbB.invInertia * cross(con.relGlobalAnchorB, translationP);
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}
