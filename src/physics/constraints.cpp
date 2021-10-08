#include "pch.h"
#include "constraints.h"
#include "physics.h"
#include "collision_narrow.h"
#include "core/cpu_profiling.h"
#include "core/math_simd.h"


#define DISTANCE_CONSTRAINT_BETA 0.1f
#define BALL_JOINT_CONSTRAINT_BETA 0.1f
#define HINGE_ROTATION_CONSTRAINT_BETA 0.3f
#define HINGE_LIMIT_CONSTRAINT_BETA 0.1f
#define TWIST_LIMIT_CONSTRAINT_BETA 0.1f

#define DT_THRESHOLD 1e-5f


#if CONSTRAINT_SIMD_WIDTH == 4
typedef vec2x<floatx4> vec2w;
typedef vec3x<floatx4> vec3w;
typedef vec4x<floatx4> vec4w;
typedef quatx<floatx4> quatw;
typedef mat3x<floatx4> mat3w;
typedef floatx4 floatw;
typedef intx4 intw;
#else
typedef vec2x<floatx8> vec2w;
typedef vec3x<floatx8> vec3w;
typedef vec4x<floatx8> vec4w;
typedef quatx<floatx8> quatw;
typedef mat3x<floatx8> mat3w;
typedef floatx8 floatw;
typedef intx8 intw;
#endif


void initializeDistanceVelocityConstraints(const rigid_body_global_state* rbs, const distance_constraint* input, const constraint_body_pair* bodyPairs, distance_constraint_update* output, uint32 count, float dt)
{
	CPU_PROFILE_BLOCK("Initialize distance constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		const distance_constraint& in = input[i];
		distance_constraint_update& out = output[i];

		out.rigidBodyIndexA = bodyPairs[i].rbA;
		out.rigidBodyIndexB = bodyPairs[i].rbB;

		const rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		const rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to COG.
		out.relGlobalAnchorA = globalA.rotation * (in.localAnchorA - globalA.localCOGPosition);
		out.relGlobalAnchorB = globalB.rotation * (in.localAnchorB - globalB.localCOGPosition);

		// Global.
		vec3 globalAnchorA = globalA.position + out.relGlobalAnchorA;
		vec3 globalAnchorB = globalB.position + out.relGlobalAnchorB;

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

		out.impulseToAngularVelocityA = globalA.invInertia * cross(out.relGlobalAnchorA, crAu);
		out.impulseToAngularVelocityB = globalB.invInertia * cross(out.relGlobalAnchorB, crBu);
	}
}

void solveDistanceVelocityConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs)
{
	CPU_PROFILE_BLOCK("Solve distance constraints");

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
		rbA.angularVelocity -= con.impulseToAngularVelocityA * lambda;
		rbB.linearVelocity += rbB.invMass * P;
		rbB.angularVelocity += con.impulseToAngularVelocityB * lambda;
	}
}

void initializeBallJointVelocityConstraints(const rigid_body_global_state* rbs, const ball_joint_constraint* input, const constraint_body_pair* bodyPairs, ball_joint_constraint_update* output, uint32 count, float dt)
{
	CPU_PROFILE_BLOCK("Initialize ball joint constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		const ball_joint_constraint& in = input[i];
		ball_joint_constraint_update& out = output[i];

		out.rigidBodyIndexA = bodyPairs[i].rbA;
		out.rigidBodyIndexB = bodyPairs[i].rbB;

		const rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		const rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to COG.
		out.relGlobalAnchorA = globalA.rotation * (in.localAnchorA - globalA.localCOGPosition);
		out.relGlobalAnchorB = globalB.rotation * (in.localAnchorB - globalB.localCOGPosition);

		// Global.
		vec3 globalAnchorA = globalA.position + out.relGlobalAnchorA;
		vec3 globalAnchorB = globalB.position + out.relGlobalAnchorB;

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
	CPU_PROFILE_BLOCK("Solve ball joint constraints");

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

void initializeHingeJointVelocityConstraints(const rigid_body_global_state* rbs, const hinge_joint_constraint* input, const constraint_body_pair* bodyPairs, hinge_joint_constraint_update* output, uint32 count, float dt)
{
	CPU_PROFILE_BLOCK("Initialize hinge joint constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		const hinge_joint_constraint& in = input[i];
		hinge_joint_constraint_update& out = output[i];

		out.rigidBodyIndexA = bodyPairs[i].rbA;
		out.rigidBodyIndexB = bodyPairs[i].rbB;

		const rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		const rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to COG.
		out.relGlobalAnchorA = globalA.rotation * (in.localAnchorA - globalA.localCOGPosition);
		out.relGlobalAnchorB = globalB.rotation * (in.localAnchorB - globalB.localCOGPosition);

		// Global.
		vec3 globalAnchorA = globalA.position + out.relGlobalAnchorA;
		vec3 globalAnchorB = globalB.position + out.relGlobalAnchorB;




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
		vec3 globalHingeAxisA = globalA.rotation * in.localHingeAxisA;
		vec3 globalHingeAxisB = globalB.rotation * in.localHingeAxisB;

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
			vec3 localHingeCompareA = conjugate(globalA.rotation) * (globalB.rotation * in.localHingeTangentB);
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

				out.motorAndLimitImpulseToAngularVelocityA = globalA.invInertia * out.globalRotationAxis;
				out.motorAndLimitImpulseToAngularVelocityB = globalB.invInertia * out.globalRotationAxis;

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
	CPU_PROFILE_BLOCK("Solve hinge joint constraints");

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

			wA -= con.motorAndLimitImpulseToAngularVelocityA * motorLambda;
			wB += con.motorAndLimitImpulseToAngularVelocityB * motorLambda;
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

			wA -= con.motorAndLimitImpulseToAngularVelocityA * limitLambda;
			wB += con.motorAndLimitImpulseToAngularVelocityB * limitLambda;
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

void initializeConeTwistVelocityConstraints(const rigid_body_global_state* rbs, const cone_twist_constraint* input, const constraint_body_pair* bodyPairs, cone_twist_constraint_update* output, uint32 count, float dt)
{
	CPU_PROFILE_BLOCK("Initialize cone twist constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		const cone_twist_constraint& in = input[i];
		cone_twist_constraint_update& out = output[i];

		out.rigidBodyIndexA = bodyPairs[i].rbA;
		out.rigidBodyIndexB = bodyPairs[i].rbB;

		const rigid_body_global_state& globalA = rbs[out.rigidBodyIndexA];
		const rigid_body_global_state& globalB = rbs[out.rigidBodyIndexB];

		// Relative to COG.
		out.relGlobalAnchorA = globalA.rotation * (in.localAnchorA - globalA.localCOGPosition);
		out.relGlobalAnchorB = globalB.rotation * (in.localAnchorB - globalB.localCOGPosition);

		// Global.
		vec3 globalAnchorA = globalA.position + out.relGlobalAnchorA;
		vec3 globalAnchorB = globalB.position + out.relGlobalAnchorB;

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

		quat btoa = conjugate(globalA.rotation) * globalB.rotation;

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
			out.globalSwingAxis = globalA.rotation * swingAxis;
			float invEffectiveLimitMass = dot(out.globalSwingAxis, globalA.invInertia * out.globalSwingAxis)
										+ dot(out.globalSwingAxis, globalB.invInertia * out.globalSwingAxis);
			out.effectiveSwingLimitMass = (invEffectiveLimitMass != 0.f) ? (1.f / invEffectiveLimitMass) : 0.f;

			out.swingLimitBias = 0.f;
			if (dt > DT_THRESHOLD)
			{
				out.swingLimitBias = (in.swingLimit - swingAngle) * HINGE_LIMIT_CONSTRAINT_BETA / dt;
			}

			out.swingLimitImpulseToAngularVelocityA = globalA.invInertia * out.globalSwingAxis;
			out.swingLimitImpulseToAngularVelocityB = globalB.invInertia * out.globalSwingAxis;
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
				out.globalSwingMotorAxis = globalA.rotation * localSwingMotorAxis;
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
				out.globalSwingMotorAxis = globalA.rotation * localSwingMotorAxis;

				float cosAngle = dot(localTargetDirection, localLimitAxisCompareA);
				float deltaAngle = acos(clamp01(cosAngle));
				out.swingMotorVelocity = (dt > DT_THRESHOLD) ? (deltaAngle / dt * 0.2f) : 0.f;
			}

			out.swingMotorImpulseToAngularVelocityA = globalA.invInertia * out.globalSwingMotorAxis;
			out.swingMotorImpulseToAngularVelocityB = globalB.invInertia * out.globalSwingMotorAxis;

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
			out.globalTwistAxis = globalA.rotation * localLimitAxisA;
			float invEffectiveMass = dot(out.globalTwistAxis, globalA.invInertia * out.globalTwistAxis)
								   + dot(out.globalTwistAxis, globalB.invInertia * out.globalTwistAxis);
			out.effectiveTwistMass = (invEffectiveMass != 0.f) ? (1.f / invEffectiveMass) : 0.f;

			out.twistLimitSign = minTwistLimitViolated ? 1.f : -1.f;

			out.maxTwistMotorImpulse = in.maxTwistMotorTorque * dt;
			out.twistMotorImpulse = 0.f;

			out.twistMotorAndLimitImpulseToAngularVelocityA = globalA.invInertia * out.globalTwistAxis;
			out.twistMotorAndLimitImpulseToAngularVelocityB = globalB.invInertia * out.globalTwistAxis;

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
	CPU_PROFILE_BLOCK("Solve cone twist constraints");

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

			wA -= con.twistMotorAndLimitImpulseToAngularVelocityA * motorLambda;
			wB += con.twistMotorAndLimitImpulseToAngularVelocityB * motorLambda;
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

			wA -= con.swingMotorImpulseToAngularVelocityA * motorLambda;
			wB += con.swingMotorImpulseToAngularVelocityB * motorLambda;
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

			wA -= con.twistMotorAndLimitImpulseToAngularVelocityA * limitLambda;
			wB += con.twistMotorAndLimitImpulseToAngularVelocityB * limitLambda;
		}

		// Cone.
		if (con.solveSwingLimit)
		{
			float aDotWA = dot(con.globalSwingAxis, wA);
			float aDotWB = dot(con.globalSwingAxis, wB);
			float swingLimitCdot = aDotWA - aDotWB + con.swingLimitBias;
			float limitLambda = -con.effectiveSwingLimitMass * swingLimitCdot;

			float impulse = max(con.swingImpulse + limitLambda, 0.f);
			limitLambda = impulse - con.swingImpulse;
			con.swingImpulse = impulse;

			wA += con.swingLimitImpulseToAngularVelocityA * limitLambda;
			wB -= con.swingLimitImpulseToAngularVelocityB * limitLambda;
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






void initializeCollisionVelocityConstraints(rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, collision_constraint* collisionConstraints, uint32 numContacts, float dt)
{
	CPU_PROFILE_BLOCK("Initialize collision constraints");

	for (uint32 contactID = 0; contactID < numContacts; ++contactID)
	{
		collision_constraint& constraint = collisionConstraints[contactID];
		const collision_contact& contact = contacts[contactID];
		constraint_body_pair pair = bodyPairs[contactID];

		auto& rbA = rbs[pair.rbA];
		auto& rbB = rbs[pair.rbB];

		constraint.impulseInNormalDir = 0.f;
		constraint.impulseInTangentDir = 0.f;

		constraint.relGlobalAnchorA = contact.point - rbA.position;
		constraint.relGlobalAnchorB = contact.point - rbB.position;

		vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, constraint.relGlobalAnchorA);
		vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, constraint.relGlobalAnchorB);

		vec3 relVelocity = anchorVelocityB - anchorVelocityA;
		constraint.tangent = relVelocity - dot(contact.normal, relVelocity) * contact.normal;
		constraint.tangent = noz(constraint.tangent);

		{ // Tangent direction.
			vec3 crAt = cross(constraint.relGlobalAnchorA, constraint.tangent);
			vec3 crBt = cross(constraint.relGlobalAnchorB, constraint.tangent);
			float invMassInTangentDir = rbA.invMass + dot(crAt, rbA.invInertia * crAt)
				+ rbB.invMass + dot(crBt, rbB.invInertia * crBt);
			constraint.effectiveMassInTangentDir = (invMassInTangentDir != 0.f) ? (1.f / invMassInTangentDir) : 0.f;

			constraint.tangentImpulseToAngularVelocityA = rbA.invInertia * crAt;
			constraint.tangentImpulseToAngularVelocityB = rbB.invInertia * crBt;
		}

		{ // Normal direction.
			vec3 crAn = cross(constraint.relGlobalAnchorA, contact.normal);
			vec3 crBn = cross(constraint.relGlobalAnchorB, contact.normal);
			float invMassInNormalDir = rbA.invMass + dot(crAn, rbA.invInertia * crAn)
				+ rbB.invMass + dot(crBn, rbB.invInertia * crBn);
			constraint.effectiveMassInNormalDir = (invMassInNormalDir != 0.f) ? (1.f / invMassInNormalDir) : 0.f;

			constraint.bias = 0.f;

			if (dt > DT_THRESHOLD)
			{
				float vRel = dot(contact.normal, anchorVelocityB - anchorVelocityA);
				const float slop = -0.001f;
				if (-contact.penetrationDepth < slop && vRel < 0.f)
				{
					float restitution = (float)(contact.friction_restitution & 0xFFFF) / (float)0xFFFF;
					constraint.bias = -restitution * vRel - 0.1f * (-contact.penetrationDepth - slop) / dt;
				}
			}

			constraint.normalImpulseToAngularVelocityA = rbA.invInertia * crAn;
			constraint.normalImpulseToAngularVelocityB = rbB.invInertia * crBn;
		}
	}
}

void solveCollisionVelocityConstraints(const collision_contact* contacts, collision_constraint* constraints, const constraint_body_pair* bodyPairs, uint32 count, rigid_body_global_state* rbs)
{
	CPU_PROFILE_BLOCK("Solve collision constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		const collision_contact& contact = contacts[i];
		collision_constraint& constraint = constraints[i];
		constraint_body_pair pair = bodyPairs[i];

		auto& rbA = rbs[pair.rbA];
		auto& rbB = rbs[pair.rbB];

		if (rbA.invMass == 0.f && rbB.invMass == 0.f)
		{
			continue;
		}

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		{ // Tangent dir.
			vec3 anchorVelocityA = vA + cross(wA, constraint.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, constraint.relGlobalAnchorB);

			vec3 relVelocity = anchorVelocityB - anchorVelocityA;
			float vt = dot(relVelocity, constraint.tangent);
			float lambda = -constraint.effectiveMassInTangentDir * vt;

			float friction = (float)(contact.friction_restitution >> 16) / (float)0xFFFF;
			float maxFriction = friction * constraint.impulseInNormalDir;
			assert(maxFriction >= 0.f);
			float newImpulse = clamp(constraint.impulseInTangentDir + lambda, -maxFriction, maxFriction);
			lambda = newImpulse - constraint.impulseInTangentDir;
			constraint.impulseInTangentDir = newImpulse;

			vec3 P = lambda * constraint.tangent;
			vA -= rbA.invMass * P;
			wA -= constraint.tangentImpulseToAngularVelocityA * lambda;
			vB += rbB.invMass * P;
			wB += constraint.tangentImpulseToAngularVelocityB * lambda;
		}

		{ // Normal dir.
			vec3 anchorVelocityA = vA + cross(wA, constraint.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, constraint.relGlobalAnchorB);

			vec3 relVelocity = anchorVelocityB - anchorVelocityA;
			float vn = dot(relVelocity, contact.normal);
			float lambda = -constraint.effectiveMassInNormalDir * (vn - constraint.bias);
			float impulse = max(constraint.impulseInNormalDir + lambda, 0.f);
			lambda = impulse - constraint.impulseInNormalDir;
			constraint.impulseInNormalDir = impulse;

			vec3 P = lambda * contact.normal;
			vA -= rbA.invMass * P;
			wA -= constraint.normalImpulseToAngularVelocityA * lambda;
			vB += rbB.invMass * P;
			wB += constraint.normalImpulseToAngularVelocityB * lambda;
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}

struct alignas(32) simd_constraint_body_pair
{
	uint32 ab[CONSTRAINT_SIMD_WIDTH];
};

struct alignas(32) simd_constraint_slot
{
	uint32 indices[CONSTRAINT_SIMD_WIDTH];
};

static uint32 scheduleConstraintsSIMD(memory_arena& arena, const constraint_body_pair* bodyPairs, uint32 numBodyPairs, uint16 dummyRigidBodyIndex, simd_constraint_slot* outConstraintSlots)
{
	CPU_PROFILE_BLOCK("Schedule constraints SIMD");

	uint32 numConstraintSlots = 0;

	static const uint32 numBuckets = 4;
	simd_constraint_body_pair* pairBuckets[numBuckets];
	simd_constraint_slot* slotBuckets[numBuckets];
	uint32 numEntriesPerBucket[numBuckets] = {};

	intw invalid = ~0;

	uint32 numAllocationsPerBucket = bucketize(numBodyPairs, numBuckets);

	memory_marker marker = arena.getMarker();

	for (unsigned i = 0; i < numBuckets; ++i)
	{
		pairBuckets[i] = arena.allocate<simd_constraint_body_pair>(numAllocationsPerBucket + 1);
		slotBuckets[i] = arena.allocate<simd_constraint_slot>(numAllocationsPerBucket);

		// Add padding with invalid data so we don't have to range check.
		invalid.store((int*)pairBuckets[i]->ab);
	}

	for (uint32 i = 0; i < numBodyPairs; ++i)
	{
		constraint_body_pair bodyPair = bodyPairs[i];

		// If one of the bodies is the dummy, just set it to the other for the comparison below.
		uint16 rbA = (bodyPair.rbA == dummyRigidBodyIndex) ? bodyPair.rbB : bodyPair.rbA;
		uint16 rbB = (bodyPair.rbB == dummyRigidBodyIndex) ? bodyPair.rbA : bodyPair.rbB;

		uint32 bucket = i % numBuckets;
		simd_constraint_body_pair* pairs = pairBuckets[bucket];
		simd_constraint_slot* slots = slotBuckets[bucket];


#if CONSTRAINT_SIMD_WIDTH == 4
		intw a = _mm_set1_epi16(rbA);
		intw b = _mm_set1_epi16(rbB);
		intw scheduled;

		uint32 j = 0;
		for (;; ++j)
		{
			scheduled = _mm_load_si128((const __m128i*)pairs[j].ab);

			__m128i conflictsWithThisSlot = _mm_packs_epi16(_mm_cmpeq_epi16(a, scheduled), _mm_cmpeq_epi16(b, scheduled));
			if (!_mm_movemask_epi8(conflictsWithThisSlot))
			{
				break;
			}
		}
#else
		intw a = _mm256_set1_epi16(rbA);
		intw b = _mm256_set1_epi16(rbB);
		intw scheduled;

		uint32 j = 0;
		for (;; ++j)
		{
			scheduled = _mm256_load_si256((const __m256i*)pairs[j].ab);

			__m256i conflictsWithThisSlot = _mm256_packs_epi16(_mm256_cmpeq_epi16(a, scheduled), _mm256_cmpeq_epi16(b, scheduled));
			if (!_mm256_movemask_epi8(conflictsWithThisSlot))
			{
				break;
			}
		}

#endif


		uint32 lane = indexOfLeastSignificantSetBit(toBitMask(reinterpret(scheduled == invalid)));

		simd_constraint_body_pair* pair = pairs + j;
		simd_constraint_slot* slot = slots + j;

		slot->indices[lane] = i;
		pair->ab[lane] = ((uint32)bodyPair.rbA << 16) | bodyPair.rbB; // Use the original indices here.

		uint32& count = numEntriesPerBucket[bucket];
		if (j == count)
		{
			// We used a new entry.
			++count;

			// Set entry at end to invalid.
			invalid.store((int*)pairs[count].ab);
		}
		else if (lane == CONSTRAINT_SIMD_WIDTH - 1)
		{
			// This entry is full -> commit it.
			intw indices = (int32*)slot->indices;

			// Swap and pop.
			--count;
			*pair = pairs[count];
			*slot = slots[count];

			indices.store((int32*)outConstraintSlots[numConstraintSlots++].indices);

			// Set entry at end to invalid.
			invalid.store((int*)pairs[count].ab);
		}
	}

	// There are still entries left, where not all lanes are filled. We replace these indices with the first in this entry.

	for (uint32 bucket = 0; bucket < numBuckets; ++bucket)
	{
		simd_constraint_body_pair* pairs = pairBuckets[bucket];
		simd_constraint_slot* slots = slotBuckets[bucket];
		uint32 count = numEntriesPerBucket[bucket];

		for (uint32 i = 0; i < count; ++i)
		{
			intw ab = (int32*)pairs[i].ab;
			intw indices = (int32_t*)slots[i].indices;

			intw firstIndex = fillWithFirstLane(indices);

			auto mask = ab == invalid;
			indices = ifThen(mask, firstIndex, indices);
			indices.store((int32*)outConstraintSlots[numConstraintSlots++].indices);
		}
	}

	arena.resetToMarker(marker);

	return numConstraintSlots;
}

void initializeCollisionVelocityConstraintsSIMD(memory_arena& arena, rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, uint32 numContacts,
	uint16 dummyRigidBodyIndex, simd_collision_constraint& outConstraints, float dt)
{
	CPU_PROFILE_BLOCK("Initialize collision constraints SIMD");

	simd_constraint_slot* contactSlots = arena.allocate<simd_constraint_slot>(numContacts);
	uint32 numContactSlots = scheduleConstraintsSIMD(arena, bodyPairs, numContacts, dummyRigidBodyIndex, contactSlots);

	outConstraints.numBatches = numContactSlots;
	outConstraints.batches = arena.allocate<simd_collision_constraint_batch>(outConstraints.numBatches);

	const floatw zero = floatw::zero();
	const floatw slop = -0.001f;
	const floatw scale = 0.1f;
	const floatw invDt = 1.f / dt;

	for (uint32 i = 0; i < numContactSlots; ++i)
	{
		simd_constraint_slot& slot = contactSlots[i];
		simd_collision_constraint_batch& batch = outConstraints.batches[i];

		uint16 contactIndices[CONSTRAINT_SIMD_WIDTH];
		for (uint32 j = 0; j < CONSTRAINT_SIMD_WIDTH; ++j)
		{
			contactIndices[j] = (uint16)slot.indices[j];
			batch.rbAIndices[j] = bodyPairs[slot.indices[j]].rbA;
			batch.rbBIndices[j] = bodyPairs[slot.indices[j]].rbB;
		}

		vec3w point, normal;
		floatw penetrationDepth, friction_restitutionF;
		load8((float*)contacts, contactIndices, (uint32)sizeof(collision_contact),
			point.x, point.y, point.z, penetrationDepth, normal.x, normal.y, normal.z, friction_restitutionF);
		intw friction_restitution = reinterpret(friction_restitutionF);

		floatw friction = convert(friction_restitution >> 16) / floatw(0xFFFF);
		floatw restitution = convert(friction_restitution & 0xFFFF) / floatw(0xFFFF);


		// Load body A.
		vec3w vA, wA;
		mat3w invInertiaA;
		floatw invMassA;
		vec3w positionA;
		floatw unused;

		load8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, invInertiaA.m00);

		load8((float*)&rbs->invInertia.m10, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			invInertiaA.m10, invInertiaA.m20, invInertiaA.m01, invInertiaA.m11,
			invInertiaA.m21, invInertiaA.m02, invInertiaA.m12, invInertiaA.m22);

		load4((float*)&rbs->position, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			positionA.x, positionA.y, positionA.z, unused);


		// Load body B.
		vec3w vB, wB;
		mat3w invInertiaB;
		floatw invMassB;
		vec3w positionB;

		load8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, invInertiaB.m00);

		load8((float*)&rbs->invInertia.m10, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			invInertiaB.m10, invInertiaB.m20, invInertiaB.m01, invInertiaB.m11,
			invInertiaB.m21, invInertiaB.m02, invInertiaB.m12, invInertiaB.m22);

		load4((float*)&rbs->position, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			positionB.x, positionB.y, positionB.z, unused);





		vec3w relGlobalAnchorA = point - positionA;
		vec3w relGlobalAnchorB = point - positionB;

		vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
		vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

		vec3w relVelocity = anchorVelocityB - anchorVelocityA;
		vec3w tangent = relVelocity - dot(normal, relVelocity) * normal;
		tangent = noz(tangent);


		relGlobalAnchorA.x.store(batch.relGlobalAnchorA[0]);
		relGlobalAnchorA.y.store(batch.relGlobalAnchorA[1]);
		relGlobalAnchorA.z.store(batch.relGlobalAnchorA[2]);

		relGlobalAnchorB.x.store(batch.relGlobalAnchorB[0]);
		relGlobalAnchorB.y.store(batch.relGlobalAnchorB[1]);
		relGlobalAnchorB.z.store(batch.relGlobalAnchorB[2]);

		normal.x.store(batch.normal[0]);
		normal.y.store(batch.normal[1]);
		normal.z.store(batch.normal[2]);

		tangent.x.store(batch.tangent[0]);
		tangent.y.store(batch.tangent[1]);
		tangent.z.store(batch.tangent[2]);

		zero.store(batch.impulseInNormalDir);
		zero.store(batch.impulseInTangentDir);
		friction.store(batch.friction);



		{ // Tangent direction.
			vec3w crAt = cross(relGlobalAnchorA, tangent);
			vec3w crBt = cross(relGlobalAnchorB, tangent);
			floatw invMassInTangentDir = invMassA + dot(crAt, invInertiaA * crAt)
				+ invMassB + dot(crBt, invInertiaB * crBt);
			floatw effectiveMassInTangentDir = ifThen(invMassInTangentDir != zero, 1.f / invMassInTangentDir, zero);
			effectiveMassInTangentDir.store(batch.effectiveMassInTangentDir);

			vec3w tangentImpulseToAngularVelocityA = invInertiaA * crAt;
			tangentImpulseToAngularVelocityA.x.store(batch.tangentImpulseToAngularVelocityA[0]);
			tangentImpulseToAngularVelocityA.y.store(batch.tangentImpulseToAngularVelocityA[1]);
			tangentImpulseToAngularVelocityA.z.store(batch.tangentImpulseToAngularVelocityA[2]);

			vec3w tangentImpulseToAngularVelocityB = invInertiaB * crBt;
			tangentImpulseToAngularVelocityB.x.store(batch.tangentImpulseToAngularVelocityB[0]);
			tangentImpulseToAngularVelocityB.y.store(batch.tangentImpulseToAngularVelocityB[1]);
			tangentImpulseToAngularVelocityB.z.store(batch.tangentImpulseToAngularVelocityB[2]);
		}


		{ // Normal direction.
			vec3w crAn = cross(relGlobalAnchorA, normal);
			vec3w crBn = cross(relGlobalAnchorB, normal);
			floatw invMassInNormalDir = invMassA + dot(crAn, invInertiaA * crAn)
				+ invMassB + dot(crBn, invInertiaB * crBn);
			floatw effectiveMassInNormalDir = ifThen(invMassInNormalDir != zero, 1.f / invMassInNormalDir, zero);

			floatw bias = zero;

			if (dt > DT_THRESHOLD)
			{
				floatw vRel = dot(normal, anchorVelocityB - anchorVelocityA);

				floatw bounceBias = -restitution * vRel - scale * (-penetrationDepth - slop) * invDt;
				bias = ifThen((-penetrationDepth < slop) & (vRel < zero), bounceBias, bias);
			}

			effectiveMassInNormalDir.store(batch.effectiveMassInNormalDir);
			bias.store(batch.bias);

			vec3w normalImpulseToAngularVelocityA = invInertiaA * crAn;
			normalImpulseToAngularVelocityA.x.store(batch.normalImpulseToAngularVelocityA[0]);
			normalImpulseToAngularVelocityA.y.store(batch.normalImpulseToAngularVelocityA[1]);
			normalImpulseToAngularVelocityA.z.store(batch.normalImpulseToAngularVelocityA[2]);

			vec3w normalImpulseToAngularVelocityB = invInertiaB * crBn;
			normalImpulseToAngularVelocityB.x.store(batch.normalImpulseToAngularVelocityB[0]);
			normalImpulseToAngularVelocityB.y.store(batch.normalImpulseToAngularVelocityB[1]);
			normalImpulseToAngularVelocityB.z.store(batch.normalImpulseToAngularVelocityB[2]);
		}
	}
}

void solveCollisionVelocityConstraintsSIMD(simd_collision_constraint& constraints, rigid_body_global_state* rbs)
{
	CPU_PROFILE_BLOCK("Solve collision constraints SIMD");

	for (uint32 i = 0; i < constraints.numBatches; ++i)
	{
		simd_collision_constraint_batch& batch = constraints.batches[i];

		// Load body A.
		vec3w vA, wA;
		floatw invMassA;
		floatw dummyA;

		load8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, dummyA);


		// Load body B.
		vec3w vB, wB;
		floatw invMassB;
		floatw dummyB;

		load8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, dummyB);


		// Load constraint.
		vec3w relGlobalAnchorA(batch.relGlobalAnchorA[0], batch.relGlobalAnchorA[1], batch.relGlobalAnchorA[2]);
		vec3w relGlobalAnchorB(batch.relGlobalAnchorB[0], batch.relGlobalAnchorB[1], batch.relGlobalAnchorB[2]);
		vec3w normal(batch.normal[0], batch.normal[1], batch.normal[2]);
		vec3w tangent(batch.tangent[0], batch.tangent[1], batch.tangent[2]);
		floatw effectiveMassInNormalDir(batch.effectiveMassInNormalDir);
		floatw effectiveMassInTangentDir(batch.effectiveMassInTangentDir);
		floatw friction(batch.friction);
		floatw impulseInNormalDir(batch.impulseInNormalDir);
		floatw impulseInTangentDir(batch.impulseInTangentDir);
		floatw bias(batch.bias);

		vec3w tangentImpulseToAngularVelocityA(batch.tangentImpulseToAngularVelocityA[0], batch.tangentImpulseToAngularVelocityA[1], batch.tangentImpulseToAngularVelocityA[2]);
		vec3w tangentImpulseToAngularVelocityB(batch.tangentImpulseToAngularVelocityB[0], batch.tangentImpulseToAngularVelocityB[1], batch.tangentImpulseToAngularVelocityB[2]);

		{ // Tangent dir.
			vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
			vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

			vec3w relVelocity = anchorVelocityB - anchorVelocityA;
			floatw vt = dot(relVelocity, tangent);
			floatw lambda = -effectiveMassInTangentDir * vt;

			floatw maxFriction = friction * impulseInNormalDir;
			floatw newImpulse = clamp(impulseInTangentDir + lambda, -maxFriction, maxFriction);
			lambda = newImpulse - impulseInTangentDir;
			impulseInTangentDir = newImpulse;

			vec3w P = lambda * tangent;
			vA -= invMassA * P;
			wA -= tangentImpulseToAngularVelocityA * lambda;
			vB += invMassB * P;
			wB += tangentImpulseToAngularVelocityB * lambda;
		}

		vec3w normalImpulseToAngularVelocityA(batch.normalImpulseToAngularVelocityA[0], batch.normalImpulseToAngularVelocityA[1], batch.normalImpulseToAngularVelocityA[2]);
		vec3w normalImpulseToAngularVelocityB(batch.normalImpulseToAngularVelocityB[0], batch.normalImpulseToAngularVelocityB[1], batch.normalImpulseToAngularVelocityB[2]);

		{ // Normal dir.
			vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
			vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

			vec3w relVelocity = anchorVelocityB - anchorVelocityA;
			floatw vn = dot(relVelocity, normal);
			floatw lambda = -effectiveMassInNormalDir * (vn - bias);
			floatw impulse = maximum(impulseInNormalDir + lambda, floatw::zero());
			lambda = impulse - impulseInNormalDir;
			impulseInNormalDir = impulse;

			vec3w P = lambda * normal;
			vA -= invMassA * P;
			wA -= normalImpulseToAngularVelocityA * lambda;
			vB += invMassB * P;
			wB += normalImpulseToAngularVelocityB * lambda;
		}

		impulseInNormalDir.store(batch.impulseInNormalDir);
		impulseInTangentDir.store(batch.impulseInTangentDir);

		store8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, dummyA);

		store8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, dummyB);
	}
}


