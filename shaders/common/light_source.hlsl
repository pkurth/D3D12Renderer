#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_IRRADIANCE_SCALE 1000.f

static float getAttenuation(float distance, float maxDistance)
{
	// https://imdoingitwrong.wordpress.com/2011/02/10/improved-light-attenuation/
	float relDist = min(distance / maxDistance, 1.f);
	float d = distance / (1.f - relDist * relDist);
	
	float att =  1.f / (d * d + 1.f);
	return att;
}

struct directional_light_cb
{
	vec3 direction;
	uint32 padding;
	vec3 radiance;
	uint32 padding2;
};

struct point_light_cb
{
	vec3 position;
	float radius; // Maximum distance.
	vec3 radiance;
	uint32 padding;
};

struct spot_light_cb
{
	vec3 position;
	float innerCutoff; // cos(innerAngle).
	vec3 direction;
	float outerCutoff; // cos(outerAngle).
	vec3 radiance;
	float maxDistance;
};

#endif
