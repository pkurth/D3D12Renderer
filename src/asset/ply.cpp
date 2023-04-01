#include "pch.h"
#include "geometry/mesh.h"

#include <fstream>



static void writeHeaderToFile(std::ofstream& outfile, int numPoints, bool writeUVs, bool writeNormals, bool writeColors, int numFaces)
{
	const char* format_header = "binary_little_endian 1.0";
	outfile << "ply" << std::endl
		<< "format " << format_header << std::endl
		<< "comment scan3d-capture generated" << std::endl
		<< "element vertex " << numPoints << std::endl
		<< "property float x" << std::endl
		<< "property float y" << std::endl
		<< "property float z" << std::endl;

	if (writeUVs)
	{
		outfile << "property float texture_u" << std::endl
			<< "property float texture_v" << std::endl;
	}

	if (writeNormals)
	{
		outfile << "property float nx" << std::endl
			<< "property float ny" << std::endl
			<< "property float nz" << std::endl;
	}

	if (writeColors)
	{
		outfile << "property uchar red" << std::endl
			<< "property uchar green" << std::endl
			<< "property uchar blue" << std::endl
			<< "property uchar alpha" << std::endl;
	}

	outfile << "element face " << numFaces << std::endl
		<< "property list uchar int vertex_indices" << std::endl;

	outfile << "end_header" << std::endl;
}

template <typename T>
static void write(std::ofstream& outfile, T value)
{
	outfile.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void testDumpToPLY(const std::string& filename,
	const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<indexed_triangle16>& triangles,
	uint8 r, uint8 g, uint8 b)
{
	std::ofstream outfile;
	std::ios::openmode mode = std::ios::out | std::ios::trunc | std::ios::binary;
	outfile.open(filename, mode);

	uint32 numPoints = (uint32)positions.size();
	uint32 numFaces = (uint32)triangles.size();
	bool writeUVs = uvs.size() > 0;
	bool writeNormals = normals.size() > 0;
	writeHeaderToFile(outfile, numPoints, writeUVs, writeNormals, true, numFaces);

	uint8 a = 255;
	for (uint32 i = 0; i < numPoints; ++i)
	{
		write(outfile, positions[i]);
		if (writeUVs) { write(outfile, uvs[i]); }
		if (writeNormals) { write(outfile, normals[i]); }
		write(outfile, r);
		write(outfile, g);
		write(outfile, b);
		write(outfile, a);
	}

	for (indexed_triangle16 tri : triangles)
	{
		uint8 count = 3;
		write(outfile, count);
		write(outfile, (int32)tri.a);
		write(outfile, (int32)tri.b);
		write(outfile, (int32)tri.c);
	}

	outfile.close();
}

