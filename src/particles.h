#pragma once

#include "math.h"
#include "random.h"
#include "dx_texture.h"
#include "material.h"

struct particle_data
{
	vec3 position;
	float timeAlive;
	vec3 velocity;
	float maxLifetime;
	vec4 color;
	vec2 uv0;
	vec2 uv1;
};

enum particle_property_type
{
	particle_property_type_constant,
	particle_property_type_linear,
	particle_property_type_random,
};

template <typename T>
struct particle_property_constant
{
	T value;

	inline T interpolate(float t) const
	{
		return value;
	}
};

template <typename T>
struct particle_property_linear
{
	T from;
	T to;

	inline T interpolate(float t) const
	{
		return lerp(from, to, t);
	}
};

template <typename T>
struct particle_property_random
{
	T min;
	T max;

	inline T start(random_number_generator& rng) const
	{
		T result;
		for (uint32 i = 0; i < arraysize(result.data); ++i)
		{
			result.data[i] = rng.randomFloatBetween(min.data[i], max.data[i]);
		}
		return result;
	}
};

template <>
struct particle_property_random<float>
{
	float min;
	float max;

	inline float start(random_number_generator& rng) const
	{
		return rng.randomFloatBetween(min, max);
	}
};

template <typename T>
struct particle_property
{
	particle_property() {}

	particle_property_type type;

	union
	{
		particle_property_constant<T> constant;
		particle_property_linear<T> linear;
		particle_property_random<T> random;
	};

	inline T start(random_number_generator& rng) const
	{
		switch (type)
		{
			case particle_property_type_constant: return constant.value;
			case particle_property_type_linear: return linear.from;
			case particle_property_type_random: return random.start(rng);
		}
		return T();
	}

	inline T interpolate(float relativeLifetime, T current) const
	{
		switch (type)
		{
			case particle_property_type_constant: return constant.interpolate(relativeLifetime);
			case particle_property_type_linear: return linear.interpolate(relativeLifetime);
			case particle_property_type_random: return current;
		}
		return T();
	}

	void initializeAsConstant(T c)
	{
		type = particle_property_type_constant;
		constant.value = c;
	}

	void initializeAsLinear(T from, T to)
	{
		type = particle_property_type_linear;
		linear.from = from;
		linear.to = to;
	}

	void initializeAsRandom(T min, T max)
	{
		type = particle_property_type_random;
		random.min = min;
		random.max = max;
	}
};

struct particle_material : material_base
{
	static void initializePipeline();

	void prepareForRendering(struct dx_command_list* cl);
	static void setupTransparentPipeline(dx_command_list* cl, const common_material_info& info);
};

struct particle_system
{
	void initialize(uint32 numParticles);
	void update(float dt);
	void render(const struct render_camera& camera, struct transparent_render_pass* renderPass);

	vec3 spawnPosition;
	float spawnRate;
	float gravityFactor;

	particle_property<vec4> color;
	particle_property<float> maxLifetime;
	particle_property<vec3> startVelocity;

private:
	ref<particle_material> material;
	random_number_generator rng;

	float particleSpawnAccumulator;
	std::vector<particle_data> particles;
};
