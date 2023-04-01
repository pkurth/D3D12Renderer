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

struct obj_mesh
{
	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;

	std::vector<int32> indices;
	std::vector<int32> materialIndexPerFace;
};

struct obj_material
{
	std::string name;
};

static std::vector<obj_material> loadMaterialLibrary(const fs::path& path)
{
	entire_file file = loadFile(path);

	std::vector<obj_material> result;
	obj_material currentMaterial = {};

	{
		CPU_PRINT_PROFILE_BLOCK("Parse OBJ material library");

		while (file.readOffset < file.size)
		{
			sized_string token = readString(file);

			if (token == "newmtl")
			{
				if (!currentMaterial.name.empty())
				{
					result.push_back(std::move(currentMaterial));
				}

				sized_string name = readString(file);
				currentMaterial.name = nameToString(name);
			}
			else if (token == "Ns")
			{
				float value = readFloat(file);
				float roughness = 1.f - (sqrt(value) * 0.1f);
			}
			else if (token == "Ni")
			{

			}
			else if (token == "d")
			{

			}
			else if (token == "Tr")
			{

			}
			else if (token == "Tf")
			{

			}
			else if (token == "illum")
			{

			}
			else if (token == "Ka")
			{

			}
			else if (token == "Kd")
			{

			}
			else if (token == "Ks")
			{

			}
			else if (token == "Ke")
			{

			}
			else if (token == "map_Ka")
			{

			}
			else if (token == "map_Kd")
			{

			}
			else if (token == "map_d")
			{

			}
			else if (token == "map_bump")
			{

			}
			else if (token == "map_Ns" || token == "map_NS")
			{

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
	if (!currentMaterial.name.empty())
	{
		result.push_back(std::move(currentMaterial));
	}

	freeFile(file);

	return result;
}

model_asset loadOBJ(const fs::path& path, uint32 flags)
{
	entire_file file = loadFile(path);

	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;

	std::vector<obj_material> materials;
	std::unordered_map<std::string, int32> nameToMaterialIndex;
	int32 currentMaterialIndex = 0;

	obj_mesh mesh;

	{
		CPU_PRINT_PROFILE_BLOCK("Parse OBJ");

		while (file.readOffset < file.size)
		{
			sized_string token = readString(file);

			if (token == "mtllib")
			{
				sized_string lib = readString(file);
				std::vector<obj_material> libMaterials = loadMaterialLibrary(relativeFilepath(lib, path));
				for (obj_material& mat : libMaterials)
				{
					nameToMaterialIndex[mat.name] = (int32)materials.size();
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
				uvs.push_back(readVec2(file));
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
				uint32 faceSize = 0;
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
					mesh.positions.push_back(positions[vertexIndices.positionIndex]);

					int32 currNumNormals = (int32)normals.size();
					vertexIndices.normalIndex += (vertexIndices.normalIndex >= 0) ? 0 : currNumNormals;
					ASSERT(vertexIndices.normalIndex < currNumNormals);
					mesh.normals.push_back(normals[vertexIndices.normalIndex]);

					int32 currNumUVs = (int32)uvs.size();
					vertexIndices.uvIndex += (vertexIndices.uvIndex >= 0) ? 0 : currNumUVs;
					ASSERT(vertexIndices.uvIndex < currNumUVs);
					mesh.uvs.push_back(uvs[vertexIndices.uvIndex]);


					mesh.indices.push_back((int32)mesh.indices.size());

					++faceSize;
				}

				if (faceSize > 0)
				{
					mesh.indices.back() = ~mesh.indices.back();
					mesh.materialIndexPerFace.push_back(currentMaterialIndex);
				}
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


	freeFile(file);


	std::vector<submesh_asset> submeshes;

	triangulateAndRemoveDuplicateVertices(mesh.positions, mesh.uvs, mesh.normals, {}, {}, mesh.indices,
		mesh.materialIndexPerFace, submeshes);

	generateNormalsAndTangents(submeshes, flags);

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

	return {};
}
