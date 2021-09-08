#pragma once


#include <functional>

// Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
template <typename T>
inline void hash_combine(size_t& seed, const T& v)
{
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
	template<>
	struct hash<vec2>
	{
		size_t operator()(const vec2& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);

			return seed;
		}
	};

	template<>
	struct hash<vec3>
	{
		size_t operator()(const vec3& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);
			hash_combine(seed, x.z);

			return seed;
		}
	};

	template<>
	struct hash<vec4>
	{
		size_t operator()(const vec4& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);
			hash_combine(seed, x.z);
			hash_combine(seed, x.w);

			return seed;
		}
	};

	template<>
	struct hash<quat>
	{
		size_t operator()(const quat& x) const
		{
			size_t seed = 0;

			hash_combine(seed, x.x);
			hash_combine(seed, x.y);
			hash_combine(seed, x.z);
			hash_combine(seed, x.w);

			return seed;
		}
	};

	template<>
	struct hash<mat4>
	{
		size_t operator()(const mat4& x) const
		{
			size_t seed = 0;

			for (uint32 i = 0; i < 16; ++i)
			{
				hash_combine(seed, x.m[i]);
			}

			return seed;
		}
	};

	template <>
	struct hash<fs::path>
	{
		size_t operator()(const fs::path& p) const
		{
			return std::hash<fs::path::string_type>{}(p.native());
		}
	};
}

