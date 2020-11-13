#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

#define LIGHT_IRRADIANCE_SCALE 100.f

static float getAttenuation(float distance, float maxDistance)
{
	const float q = 0.03f;
	const float l = 0.f;
	
	float denom = min(distance / maxDistance, 1.f);
	float d = distance / (1.f - denom);
	
	float att =  1.f / (q * d * d + l * d + 1.f);
	return att;
}


struct point_light_cb
{
	vec3 position;
	float radius;
	vec3 radiance;
	uint32 flags;
};

struct spot_light_cb
{
	vec3 position;
	float innerCutoff; // cos(innerAngle).
	vec3 direction;
	float outerCutoff; // cos(outerAngle).
	vec3 radiance;
	uint32 flags;
};

struct spot_light_bounding_volume
{
	vec3 tip;
	float height;
	vec3 direction;
	float radius;
};

static spot_light_bounding_volume getSpotLightBoundingVolume(spot_light_cb sl)
{
	spot_light_bounding_volume result;
	result.tip = sl.position;
	result.direction = sl.direction;
	result.height = 32.f;
	result.radius = result.height * sqrt(1.f - sl.outerCutoff * sl.outerCutoff) / sl.outerCutoff; // Same as height * tan(acos(sl.outerCutoff)).
	return result;
}

#endif
