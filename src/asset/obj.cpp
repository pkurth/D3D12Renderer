#include "pch.h"
#include "io.h"
#include "model_asset.h"
#include "mesh_postprocessing.h"
#include "core/math.h"
#include "core/cpu_profiling.h"
#include "geometry/mesh.h"


void testDumpToPLY(const std::string& filename,
	const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<indexed_triangle16>& triangles,
	uint8 r = 255, uint8 g = 255, uint8 b = 255);


static bool isEndOfLine(char c)
{
	return c == '\r' ||
		c == '\n';
}

static bool isWhitespace(char c)
{
	return c == ' ' ||
		c == '\t' ||
		isEndOfLine(c);
}

static bool isWhiteSpaceNoEndOfLine(char c)
{
	return c == ' ' ||
		c == '\t';
}

static void discardLine(entire_file& file)
{
	while (file.readOffset < file.size && file.content[file.readOffset] != '\n')
	{
		++file.readOffset;
	}
	if (file.readOffset < file.size)
	{
		++file.readOffset;
	}
}

static void discardWhitespace(entire_file& file, bool allowLineSkip = true)
{
	if (allowLineSkip)
	{
		while (file.readOffset < file.size)
		{
			while (file.readOffset < file.size && isWhitespace((char)file.content[file.readOffset]))
			{
				++file.readOffset;
			}
			if (file.readOffset < file.size && file.content[file.readOffset] == '#')
			{
				discardLine(file);
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		while (file.readOffset < file.size && isWhiteSpaceNoEndOfLine((char)file.content[file.readOffset]))
		{
			++file.readOffset;
		}
	}
}

static sized_string readString(entire_file& file, bool allowLineSkip = true)
{
	discardWhitespace(file, allowLineSkip);

	sized_string result;
	result.str = (const char*)file.content + file.readOffset;
	while (file.readOffset < file.size && !isWhitespace((char)file.content[file.readOffset]))
	{
		++result.length;
		++file.readOffset;
	}

	return result;
}

static int32 readInt32(entire_file& file)
{
	sized_string str = readString(file);
	return atoi(str.str);
}

static vec3 readVec3(entire_file& file)
{
	sized_string xStr = readString(file);
	sized_string yStr = readString(file);
	sized_string zStr = readString(file);

	float x = (float)atof(xStr.str);
	float y = (float)atof(yStr.str);
	float z = (float)atof(zStr.str);

	return vec3(x, y, z);
}

static vec2 readVec2(entire_file& file)
{
	sized_string xStr = readString(file);
	sized_string yStr = readString(file);

	float x = (float)atof(xStr.str);
	float y = (float)atof(yStr.str);

	return vec2(x, y);
}

static float readFloat(entire_file& file)
{
	sized_string str = readString(file);
	float v = (float)atof(str.str);
	return v;
}

struct obj_vertex_indices
{
	int32 positionIndex;
	int32 normalIndex;
	int32 uvIndex;
};

static obj_vertex_indices readVertexIndices(sized_string vertexStr)
{
	sized_string indexStrs[3] = {};
	indexStrs[0].str = vertexStr.str;
	uint32 curr = 0;
	for (uint32 i = 0; i < vertexStr.length; ++i)
	{
		if (vertexStr.str[i] == '/')
		{
			++curr;
			indexStrs[curr].str = vertexStr.str + (i + 1);
		}
		else
		{
			++indexStrs[curr].length;
		}
	}


	int32 positionIndex = indexStrs[0].length ? atoi(indexStrs[0].str) : 0;
	int32 uvIndex = indexStrs[1].length ? atoi(indexStrs[1].str) : 0;
	int32 normalIndex = indexStrs[2].length ? atoi(indexStrs[2].str) : 0;

	if (positionIndex > 0)
	{
		--positionIndex;
	}
	if (normalIndex > 0)
	{
		--normalIndex;
	}
	if (uvIndex > 0)
	{
		--uvIndex;
	}

	return { positionIndex, normalIndex, uvIndex };
}

static std::vector<std::pair<std::string, pbr_material_desc>> loadMaterialLibrary(const fs::path& path)
{
	entire_file file = loadFile(path);

	std::vector<std::pair<std::string, pbr_material_desc>> result;
	pbr_material_desc currentMaterial = {};
	std::string currentName;

	{
		CPU_PRINT_PROFILE_BLOCK("Parse OBJ material library");

		while (file.readOffset < file.size)
		{
			sized_string token = readString(file);
			std::transform(token.str, token.str + token.length, (char*)token.str, [](char c) { return std::tolower(c); });

			if (token == "newmtl")
			{
				if (!currentName.empty())
				{
					result.push_back({ std::move(currentName), std::move(currentMaterial) });
				}

				sized_string name = readString(file);
				currentName = nameToString(name);
			}
			else if (token == "ns")
			{
				float value = readFloat(file);
				currentMaterial.roughnessOverride = 1.f - (sqrt(value) * 0.1f);
			}
			else if (token == "pr")
			{
				float value = readFloat(file);
				currentMaterial.roughnessOverride = value;
			}
			else if (token == "pm")
			{
				float value = readFloat(file);
				currentMaterial.metallicOverride = value;
			}
			else if (token == "ni")
			{

			}
			else if (token == "d")
			{

			}
			else if (token == "tr")
			{
				float value = readFloat(file);
				float alpha = 1.f - value;
				currentMaterial.albedoTint.w = alpha;
				if (alpha < 1.f)
				{
					currentMaterial.shader = pbr_material_shader_transparent;
				}
			}
			else if (token == "tf")
			{

			}
			else if (token == "illum")
			{

			}
			else if (token == "ka")
			{

			}
			else if (token == "kd")
			{
				vec3 color = readVec3(file);
				currentMaterial.albedoTint.xyz = color;
			}
			else if (token == "ks")
			{

			}
			else if (token == "ke")
			{
				vec3 color = readVec3(file);
				currentMaterial.emission = vec4(color, 1.f);
			}
			else if (token == "map_ka" || token == "map_pm")
			{
				sized_string str = readString(file);
				currentMaterial.metallic = relativeFilepath(str, path);
			}
			else if (token == "map_kd")
			{
				sized_string str = readString(file);
				currentMaterial.albedo = relativeFilepath(str, path);
			}
			else if (token == "map_d")
			{

			}
			else if (token == "map_bump")
			{
				sized_string str = readString(file);
				currentMaterial.normal = relativeFilepath(str, path);
			}
			else if (token == "map_ns" || token == "map_pr")
			{
				sized_string str = readString(file);
				currentMaterial.roughness = relativeFilepath(str, path);
			}
			else if (token == "bump")
			{

			}
			else if (token.length == 0)
			{
				// Nothing.
			}
			else
			{
				printf("Unrecognized material start token '%.*s'\n", token.length, token.str);
			}

			discardLine(file);
		}
	}
	if (!currentName.empty())
	{
		result.push_back({ std::move(currentName), std::move(currentMaterial) });
	}

	freeFile(file);

	return result;
}

model_asset loadOBJ(const fs::path& path, uint32 flags)
{
	CPU_PRINT_PROFILE_BLOCK("Loading OBJ");

	entire_file file = loadFile(path);

	std::vector<vec3> positions; positions.reserve(1 << 16);
	std::vector<vec2> uvs; uvs.reserve(1 << 16);
	std::vector<vec3> normals; normals.reserve(1 << 16);

	std::vector<pbr_material_desc> materials;
	std::unordered_map<std::string, int32> nameToMaterialIndex;
	int32 currentMaterialIndex = 0;

	std::vector<submesh_asset> submeshes;


	std::unordered_map<int32, per_material> materialToMesh;


	std::vector<vec3> positionCache; positionCache.reserve(16);
	std::vector<vec2> uvCache; uvCache.reserve(16);
	std::vector<vec3> normalCache; normalCache.reserve(16);

	{
		CPU_PRINT_PROFILE_BLOCK("Parse OBJ");

		while (file.readOffset < file.size)
		{
			sized_string token = readString(file);

			if (token == "mtllib")
			{
				sized_string lib = readString(file);
				auto libMaterials = loadMaterialLibrary(relativeFilepath(lib, path));
				for (auto [name, mat] : libMaterials)
				{
					nameToMaterialIndex[std::move(name)] = (int32)materials.size();
					materials.push_back(std::move(mat));
				}
			}
			else if (token == "v")
			{
				positions.push_back(readVec3(file));
			}
			else if (token == "vn")
			{
				normals.push_back(readVec3(file));
			}
			else if (token == "vt")
			{
				vec2 uv = readVec2(file);
				if (flags & mesh_flag_flip_uvs_vertically)
				{
					uv.y = 1.f - uv.y;
				}
				uvs.push_back(uv);
			}
			else if (token == "g")
			{
				sized_string name = readString(file);
			}
			else if (token == "o")
			{
				sized_string name = readString(file);
			}
			else if (token == "s")
			{
				int32 smoothing = readInt32(file);
			}
			else if (token == "usemtl")
			{
				sized_string mtl = readString(file);
				std::string name = nameToString(mtl);

				auto it = nameToMaterialIndex.find(name);
				if (it != nameToMaterialIndex.end())
				{
					currentMaterialIndex = it->second;
				}
				else
				{
					printf("Unrecognized material '%.*s'\n", mtl.length, mtl.str);
				}
			}
			else if (token == "f")
			{
				int32 faceSize = 0;
				while (file.readOffset < file.size && !isEndOfLine((char)file.content[file.readOffset]))
				{
					sized_string vertexStr = readString(file, false);
					if (vertexStr.length == 0)
					{
						break;
					}

					obj_vertex_indices vertexIndices = readVertexIndices(vertexStr);

					int32 currNumPositions = (int32)positions.size();
					vertexIndices.positionIndex += (vertexIndices.positionIndex >= 0) ? 0 : currNumPositions;
					ASSERT(vertexIndices.positionIndex < currNumPositions);
					positionCache.push_back(positions[vertexIndices.positionIndex]);

					if (flags & mesh_flag_load_uvs)
					{
						int32 currNumUVs = (int32)uvs.size();
						vertexIndices.uvIndex += (vertexIndices.uvIndex >= 0) ? 0 : currNumUVs;
						ASSERT(vertexIndices.uvIndex < currNumUVs);
						uvCache.push_back(uvs[vertexIndices.uvIndex]);
					}

					if (flags & mesh_flag_load_normals)
					{
						int32 currNumNormals = (int32)normals.size();
						vertexIndices.normalIndex += (vertexIndices.normalIndex >= 0) ? 0 : currNumNormals;
						ASSERT(vertexIndices.normalIndex < currNumNormals);
						normalCache.push_back(normals[vertexIndices.normalIndex]);
					}

					++faceSize;
				}

				per_material& perMat = materialToMesh[currentMaterialIndex];
				perMat.sub.materialIndex = currentMaterialIndex;

				perMat.addTriangles(positionCache, uvCache, normalCache, {}, {}, {}, 0, faceSize, submeshes);

				positionCache.clear();
				uvCache.clear();
				normalCache.clear();
			}
			else if (token.length == 0)
			{
				// Nothing.
			}
			else
			{
				printf("Unrecognized start token '%.*s'\n", token.length, token.str);
			}


			discardLine(file);
		}
	}

	for (auto [i, perMat] : materialToMesh)
	{
		perMat.flush(submeshes);
	}


	freeFile(file);


	generateNormalsAndTangents(submeshes, flags);



	model_asset result;
	result.meshes.push_back({ path.filename().string(), std::move(submeshes), -1 });
	result.materials = std::move(materials);

#if 0
	for (uint32 i = 0; i < (uint32)submeshes.size(); ++i)
	{
		const submesh_asset& sub = submeshes[i];

		vec3 diffuseColor = vec3(1.f, 1.f, 1.f);

		std::string indexedName2 = "Mesh_" + std::to_string(i) + ".ply";

		testDumpToPLY(indexedName2, sub.positions, sub.uvs, sub.normals, sub.triangles,
			(uint8)(diffuseColor.x * 255), (uint8)(diffuseColor.y * 255), (uint8)(diffuseColor.z * 255));
	}
#endif

	return result;
}
