#include "pch.h"
#include "camera_controller.h"


void updateCamera(render_camera& camera, const user_input& input, uint32 viewportWidth, uint32 viewportHeight, float dt)
{
	const float CAMERA_MOVEMENT_SPEED = 8.f;
	const float CAMERA_SENSITIVITY = 4.f;
	const float CAMERA_ORBIT_RADIUS = 50.f;

	camera.setViewport(viewportWidth, viewportHeight);

	if (input.mouse.right.down)
	{
		// Fly camera.

		vec3 cameraInputDir = vec3(
			(input.keyboard['D'].down ? 1.f : 0.f) + (input.keyboard['A'].down ? -1.f : 0.f),
			(input.keyboard['E'].down ? 1.f : 0.f) + (input.keyboard['Q'].down ? -1.f : 0.f),
			(input.keyboard['W'].down ? -1.f : 0.f) + (input.keyboard['S'].down ? 1.f : 0.f)
		) * (input.keyboard[key_shift].down ? 3.f : 1.f) * (input.keyboard[key_ctrl].down ? 0.1f : 1.f) * CAMERA_MOVEMENT_SPEED;

		vec2 turnAngle(0.f, 0.f);
		turnAngle = vec2(-input.mouse.reldx, -input.mouse.reldy) * CAMERA_SENSITIVITY;

		quat& cameraRotation = camera.rotation;
		cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
		cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

		camera.position += cameraRotation * cameraInputDir * dt;
	}
	else if (input.keyboard[key_alt].down)
	{
		if (input.mouse.left.down)
		{
			// Orbit camera.

			vec2 turnAngle(0.f, 0.f);
			turnAngle = vec2(-input.mouse.reldx, -input.mouse.reldy) * CAMERA_SENSITIVITY;

			quat& cameraRotation = camera.rotation;

			vec3 center = camera.position + cameraRotation * vec3(0.f, 0.f, -CAMERA_ORBIT_RADIUS);

			cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
			cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

			camera.position = center - cameraRotation * vec3(0.f, 0.f, -CAMERA_ORBIT_RADIUS);
		}
		else if (input.mouse.middle.down)
		{
			// Pan camera.

			vec3 cameraInputDir = vec3(
				-input.mouse.reldx * camera.aspect,
				input.mouse.reldy,
				0.f
			) * (input.keyboard[key_shift].down ? 3.f : 1.f) * (input.keyboard[key_ctrl].down ? 0.1f : 1.f) * 1000.f * CAMERA_MOVEMENT_SPEED;

			camera.position += camera.rotation * cameraInputDir * dt;
		}
	}

	camera.updateMatrices();
}
