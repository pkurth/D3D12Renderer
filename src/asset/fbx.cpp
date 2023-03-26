#include "pch.h"
#include "deflate.h"
#include "core/math.h"
#include "geometry/mesh.h"

#include "mesh_postprocessing.h"


static void testDumpToPLY(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<int32>& indices);

struct entire_file
{
	uint8* content;
	uint64 size;
	uint64 readOffset;

	template <typename T>
	T* consume(uint32 count = 1)
	{
		T* result = (T*)(content + readOffset);
		readOffset += sizeof(T) * count;
		return result;
	}
};

static entire_file loadFile(const fs::path& path)
{
	FILE* f = fopen(path.string().c_str(), "rb");
	if (!f)
	{
		return {};
	}
	fseek(f, 0, SEEK_END);
	uint32 fileSize = ftell(f);
	if (fileSize == 0)
	{
		fclose(f);
		return {};
	}
	fseek(f, 0, SEEK_SET);

	uint8* buffer = (uint8*)malloc(fileSize);
	fread(buffer, fileSize, 1, f);
	fclose(f);

	return { buffer, fileSize };
}

static void freeFile(entire_file file)
{
	free(file.content);
}

#pragma pack(push, 1)
struct fbx_header
{
	char magic[21];
	uint8 unknown[2];
	uint32 version;
};

struct fbx_node_record_header_32
{
	uint32 endOffset;
	uint32 numProperties;
	uint32 propertyListLength;
	uint8 nameLength;
};

struct fbx_node_record_header_64
{
	uint64 endOffset;
	uint64 numProperties;
	uint64 propertyListLength;
	uint8 nameLength;
};

struct fbx_data_array_header
{
	uint32 arrayLength;
	uint32 encoding;
	uint32 compressedLength;
};

#pragma pack(pop)


struct sized_string
{
	const char* str;
	uint32 length;

	sized_string() : str(0), length(0) {}
	sized_string(const char* str, uint32 length) : str(str), length(length) {}
	template<uint32 len> sized_string(const char (&str)[len]) : str(str), length(len - 1) {}
};

static bool operator==(sized_string a, sized_string b)
{
	return a.length == b.length && strncmp(a.str, b.str, a.length) == 0;
}

struct fbx_property;

struct fbx_node
{
	sized_string name;

	uint32 parent;
	uint32 next;
	uint32 firstChild;
	uint32 lastChild;
	uint32 level;

	uint32 firstProperty;
	uint32 numProperties;


	const fbx_node* findChild(const std::vector<fbx_node>& nodes, sized_string name) const
	{
		uint32 currentNode = firstChild;
		while (currentNode != -1)
		{
			if (nodes[currentNode].name == name)
			{
				return &nodes[currentNode];
			}
			currentNode = nodes[currentNode].next;
		}
		return 0;
	}

	const fbx_property* getFirstProperty(const std::vector<fbx_property>& properties) const
	{
		return numProperties ? &properties[firstProperty] : 0;
	}
};

enum fbx_property_type
{
	fbx_property_type_bool,
	fbx_property_type_float,
	fbx_property_type_double,
	fbx_property_type_int16,
	fbx_property_type_int32,
	fbx_property_type_int64,
	
	fbx_property_type_string,
	fbx_property_type_raw,
};

struct fbx_property
{
	fbx_property_type type;
	uint32 encoding;
	uint32 encodedLength;
	uint32 numElements;
	uint8* data;
};

const char* indentStr = "                              ";


static uint32 parseProperties(entire_file& file, std::vector<fbx_property>& outProperties, uint32 numProperties)
{
	uint32 before = (uint32)outProperties.size();

	for (uint32 propID = 0; propID < numProperties; ++propID)
	{
		char propType = *file.consume<char>();
		switch (propType)
		{
			case 'C': // bool
			{
				bool* value = file.consume<bool>();
				outProperties.push_back({ fbx_property_type_bool, 0, sizeof(bool), 1, (uint8*)value });
			} break;
			case 'F': // float
			{
				float* value = file.consume<float>();
				outProperties.push_back({ fbx_property_type_float, 0, sizeof(float), 1, (uint8*)value });
			} break;
			case 'D': // double
			{
				double* value = file.consume<double>();
				outProperties.push_back({ fbx_property_type_double, 0, sizeof(double), 1, (uint8*)value });
			} break;
			case 'Y': // int16
			{
				int16* value = file.consume<int16>();
				outProperties.push_back({ fbx_property_type_int16, 0, sizeof(int16), 1, (uint8*)value });
			} break;
			case 'I': // int32
			{
				int32* value = file.consume<int32>();
				outProperties.push_back({ fbx_property_type_int32, 0, sizeof(int32), 1, (uint8*)value });
			} break;
			case 'L': // int64
			{
				int64* value = file.consume<int64>();
				outProperties.push_back({ fbx_property_type_int64, 0, sizeof(int64), 1, (uint8*)value });
			} break;

			case 'b': // bool[]
			{
				fbx_data_array_header* header = file.consume<fbx_data_array_header>();
				uint8* data = file.consume<uint8>(header->compressedLength);
				outProperties.push_back({ fbx_property_type_bool, header->encoding, header->compressedLength, header->arrayLength, data });
			} break;
			case 'f': // float[]
			{
				fbx_data_array_header* header = file.consume<fbx_data_array_header>();
				uint8* data = file.consume<uint8>(header->compressedLength);
				outProperties.push_back({ fbx_property_type_float, header->encoding, header->compressedLength, header->arrayLength, data });
			} break;
			case 'd': // double[]
			{
				fbx_data_array_header* header = file.consume<fbx_data_array_header>();
				uint8* data = file.consume<uint8>(header->compressedLength);
				outProperties.push_back({ fbx_property_type_double, header->encoding, header->compressedLength, header->arrayLength, data });
			} break;
			case 'i': // int32[]
			{
				fbx_data_array_header* header = file.consume<fbx_data_array_header>();
				uint8* data = file.consume<uint8>(header->compressedLength);
				outProperties.push_back({ fbx_property_type_int32, header->encoding, header->compressedLength, header->arrayLength, data });
			} break;
			case 'l': // int64[]
			{
				fbx_data_array_header* header = file.consume<fbx_data_array_header>();
				uint8* data = file.consume<uint8>(header->compressedLength);
				outProperties.push_back({ fbx_property_type_int64, header->encoding, header->compressedLength, header->arrayLength, data });
			} break;

			case 'S': // String
			{
				uint32 length = *file.consume<uint32>();
				char* str = file.consume<char>(length);
				if (length)
				{
					outProperties.push_back({ fbx_property_type_string, 0, length, length, (uint8*)str });
				}
			} break;

			case 'R': // Raw
			{
				uint32 length = *file.consume<uint32>();
				uint8* data = file.consume<uint8>(length);
				if (length)
				{
					outProperties.push_back({ fbx_property_type_raw, 0, length, length, data });
				}
			} break;

			default:
			{
				ASSERT(false);
			} break;
		}
	}

	return (uint32)outProperties.size() - before;
}

static fbx_node_record_header_64 readNodeRecordHeader(uint32 version, entire_file& file)
{
	fbx_node_record_header_64 node;
	if (version >= 7500)
	{
		node = *file.consume<fbx_node_record_header_64>();
	}
	else
	{
		fbx_node_record_header_32 node32 = *file.consume<fbx_node_record_header_32>();
		node.endOffset = node32.endOffset;
		node.numProperties = node32.numProperties;
		node.propertyListLength = node32.propertyListLength;
		node.nameLength = node32.nameLength;
	}
	return node;
}

static void parseNodes(uint32 version, entire_file& file, std::vector<fbx_node>& outNodes, std::vector<fbx_property>& outProperties, uint32 level, uint32 parent)
{
	fbx_node_record_header_64 currentNode = readNodeRecordHeader(version, file);
	while (currentNode.endOffset != 0)
	{
		uint32 nodeNameLength = currentNode.nameLength;
		char* nodeName = file.consume<char>(nodeNameLength);
		
		fbx_node node;
		node.name = { nodeName, nodeNameLength };
		node.parent = parent;
		node.level = outNodes[parent].level + 1;
		node.next = -1;
		node.firstChild = -1;
		node.lastChild = -1;
		node.firstProperty = (uint32)outProperties.size();
		node.numProperties = (uint32)currentNode.numProperties;

		uint32 nodeIndex = (uint32)outNodes.size();
		if (parent != -1)
		{
			if (outNodes[parent].firstChild == -1)
			{
				outNodes[parent].firstChild = nodeIndex;
			}
			else
			{
				outNodes[outNodes[parent].lastChild].next = nodeIndex;
			}
			outNodes[parent].lastChild = nodeIndex;
		}

		node.numProperties = parseProperties(file, outProperties, (uint32)currentNode.numProperties);

		outNodes.push_back(node);


		uint64 sizeLeft = currentNode.endOffset - file.readOffset;
		if (sizeLeft > 0)
		{
			parseNodes(version, file, outNodes, outProperties, level + 1, nodeIndex);
			sizeLeft = currentNode.endOffset - file.readOffset;
		}

		ASSERT(sizeLeft == 0);
		currentNode = readNodeRecordHeader(version, file);
	}
}



static const fbx_node* findNode(const std::vector<fbx_node>& nodes, std::initializer_list<sized_string> names)
{
	uint32 currentNode = nodes[0].firstChild;
	const sized_string* currentName = names.begin();

	while (currentNode != -1)
	{
		if (nodes[currentNode].name == *currentName)
		{
			++currentName;
			if (currentName == names.end())
			{
				return &nodes[currentNode];
			}
			else
			{
				currentNode = nodes[currentNode].firstChild;
			}
		}
		else
		{
			currentNode = nodes[currentNode].next;
		}
	}
	return 0;
}

static uint64 readArray(const fbx_property& prop, uint8* out)
{
	if (prop.encoding == 0)
	{
		memcpy(out, prop.data, prop.encodedLength);
		return prop.encodedLength;
	}
	else
	{
		uint64 decompressedBytes = decompress(prop.data, prop.encodedLength, out);
		return decompressedBytes;
	}
}

static std::vector<int32> readInt32Array(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_int32);

	std::vector<int32> result;
	result.resize(prop.numElements);

	uint64 readBytes = readArray(prop, (uint8*)result.data());
	ASSERT(readBytes == prop.numElements * sizeof(int32));

	return result;
}

static std::vector<double> readDoubleArray(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_double);

	std::vector<double> result;
	result.resize(prop.numElements);

	uint64 readBytes = readArray(prop, (uint8*)result.data());
	ASSERT(readBytes == prop.numElements * sizeof(double));

	return result;
}

static sized_string readString(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_string);
	return { (const char*)prop.data, prop.numElements };
}

static int32 readInt32(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_int32);
	return *(int32*)prop.data;
}

static int64 readInt64(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_int64);
	return *(int64*)prop.data;
}

static double readDouble(const fbx_property& prop)
{
	ASSERT(prop.type == fbx_property_type_double);
	return *(double*)prop.data;
}

static int32 decodeIndex(int32 idx)
{
	return (idx < 0) ? ~idx : idx;
}

static bool isTriangleMesh(const std::vector<int32>& indices)
{
	if (indices.size() % 3 != 0)
	{
		return false;
	}

	for (uint32 i = 0; i < (uint32)indices.size(); i += 3)
	{
		int32 a = indices[i + 0];
		int32 b = indices[i + 1];
		int32 c = indices[i + 2];
		if (a < 0 || b < 0 || c > 0)
		{
			return false;
		}
	}

	return true;
}

struct fbx_property_iterator
{
	const fbx_node* node;
	const std::vector<fbx_property>& properties;

	struct iterator
	{
		const fbx_property* prop;

		friend bool operator!=(const iterator& a, const iterator& b) { return a.prop != b.prop; }
		iterator& operator++() { ++prop; return *this; }
		iterator operator++(int) { iterator result = *this; ++prop; return result; }
		const fbx_property& operator*() { return *prop; }
	};

	iterator begin()
	{
		return iterator{ node ? properties.data() + node->firstProperty : 0 };
	}

	iterator end()
	{
		return iterator{ node ? properties.data() + (node->firstProperty + node->numProperties) : 0 };
	}
};

struct fbx_node_iterator
{
	const fbx_node* node;
	const std::vector<fbx_node>& nodes;

	struct iterator
	{
		const fbx_node* nodes;
		uint32 child;

		friend bool operator!=(const iterator& a, const iterator& b) { return a.child != b.child; }
		iterator& operator++() { child = nodes[child].next; return *this; }
		const fbx_node& operator*() { return nodes[child]; }
	};

	iterator begin()
	{
		return iterator{ nodes.data(), node ? node->firstChild : -1 };
	}

	iterator end()
	{
		return iterator{ nodes.data(), (uint32)-1 };
	}
};

static void printProperty(const fbx_property& prop, FILE* stream, uint32 indent = 0)
{
	fbx_property_type type = prop.type;

#define PRINT_HELPER(typeStr, type, format) \
	fprintf(stream, typeStr ": ");	\
	if (prop.numElements == 1) { fprintf(stream, format, *(type*)prop.data); } \
	else if (prop.encoding == 0) { fprintf(stream, "[ "); for (uint32 j = 0; j < prop.numElements; ++j) { fprintf(stream, format " ", ((type*)prop.data)[j]); } fprintf(stream, "]"); } \
	else { fprintf(stream, "[%u compressed elements] ", prop.numElements); }

	fprintf(stream, "%.*s- ", indent, indentStr);
	switch (type)
	{
		case fbx_property_type_bool:
		{
			PRINT_HELPER("Bool", bool, "%u");
		} break;
		case fbx_property_type_float:
		{
			PRINT_HELPER("Float", float, "%f");
		} break;
		case fbx_property_type_double:
		{
			PRINT_HELPER("Double", double, "%f");
		} break;
		case fbx_property_type_int16:
		{
			PRINT_HELPER("Int16", int16, "%d");
		} break;
		case fbx_property_type_int32:
		{
			PRINT_HELPER("Int32", int32, "%d");
		} break;
		case fbx_property_type_int64:
		{
			PRINT_HELPER("Int64", int64, "%lld");
		} break;
		case fbx_property_type_string:
		{
			fprintf(stream, "String: %.*s", prop.numElements, (char*)prop.data);
		} break;
		case fbx_property_type_raw:
		{
			fprintf(stream, "Raw: [%u compressed bytes] ", prop.numElements);
		} break;
		default:
		{
			ASSERT(false);
		} break;
	}
	fprintf(stream, "\n");
}

static void printFBXContent(const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, const fbx_node& parent, FILE* stream = stdout, uint32 indent = 0)
{
	for (const fbx_node& node : fbx_node_iterator{ &parent, nodes } )
	{
		fprintf(stream, "%.*sNODE '%.*s'\n", indent, indentStr, node.name.length, node.name.str);
		for (const fbx_property& prop : fbx_property_iterator{ &node, properties })
		{
			printProperty(prop, stream, indent);
		}
		printFBXContent(nodes, properties, node, stream, indent + 1);
	}
}




enum mapping_info
{
	mapping_info_by_polygon_vertex,
	mapping_info_by_polygon,
	mapping_info_by_vertex,
	mapping_info_all_same,
};

enum reference_info
{
	reference_info_index_to_direct,
	reference_info_direct,
};

struct offset_count
{
	uint32 offset;
	uint32 count;
};

template <typename T>
static std::tuple<std::vector<T>, std::vector<int>, mapping_info, reference_info> readGeometryData(const fbx_node* node,
	const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties,
	sized_string dataNodeName, sized_string indexNodeName)
{
	std::vector<T> result;
	std::vector<int32> indices;
	mapping_info mappingInfo = mapping_info_by_polygon_vertex;
	reference_info referenceInfo = reference_info_index_to_direct;

	for (const fbx_node& child : fbx_node_iterator{ node, nodes })
	{
		if (child.name == "MappingInformationType")
		{
			sized_string mappingInfoStr = readString(*child.getFirstProperty(properties));
			if (mappingInfoStr == "ByPolygonVertex") { mappingInfo = mapping_info_by_polygon_vertex; }
			else if (mappingInfoStr == "ByPolygon") { mappingInfo = mapping_info_by_polygon; }
			else if (mappingInfoStr == "ByVertice" || mappingInfoStr == "ByVertex") { mappingInfo = mapping_info_by_vertex; }
			else if (mappingInfoStr == "AllSame") { mappingInfo = mapping_info_all_same; }
		}
		else if (child.name == "ReferenceInformationType")
		{
			sized_string referenceInfoStr = readString(*child.getFirstProperty(properties));
			if (referenceInfoStr == "IndexToDirect" || referenceInfoStr == "Index") { referenceInfo = reference_info_index_to_direct; }
			else if (referenceInfoStr == "Direct") { referenceInfo = reference_info_direct; }
		}
		else if (child.name == dataNodeName)
		{
			if constexpr (std::is_same_v<T, double>)
			{
				result = readDoubleArray(*child.getFirstProperty(properties));
			}
			else if constexpr (std::is_same_v<T, int32>)
			{
				result = readInt32Array(*child.getFirstProperty(properties));
			}
		}
		else if (child.name == indexNodeName)
		{
			indices = readInt32Array(*child.getFirstProperty(properties));
		}
	}

	return { result, indices, mappingInfo, referenceInfo };
}

template <typename data_t>
static std::vector<data_t> mapDataToVertices(const std::vector<data_t>& data, const std::vector<int32>& dataIndices,
	mapping_info mapping, reference_info reference, const std::vector<offset_count>& vertexOffsetCounts, const std::vector<uint32>& originalToNewVertex,
	uint32 numVertices)
{
	// https://banexdevblog.files.wordpress.com/2014/06/example_english.png
	if (mapping == mapping_info_by_polygon_vertex)
	{
		if (reference == reference_info_direct)
		{
			ASSERT(data.size() == numVertices);
			return data;
		}
		else
		{
			ASSERT(dataIndices.size() == numVertices);
			std::vector<data_t> result(numVertices);
			for (uint32 i = 0; i < (uint32)dataIndices.size(); ++i)
			{
				int32 dataIndex = dataIndices[i];
				data_t out = (dataIndex == -1) ? data_t(0.f) : data[dataIndex];
				result[i] = out;
			}
			return result;
		}
	}
	else if (mapping == mapping_info_by_vertex)
	{
		if (reference == reference_info_direct)
		{
			ASSERT(data.size() == vertexOffsetCounts.size());
			std::vector<data_t> result(numVertices);
			for (uint32 i = 0; i < (uint32)data.size(); ++i)
			{
				data_t out = data[i];
				uint32 offset = vertexOffsetCounts[i].offset;
				uint32 count = vertexOffsetCounts[i].count;
				for (uint32 j = 0; j < count; ++j)
				{
					uint32 vertexIndex = originalToNewVertex[j + offset];
					result[vertexIndex] = out;
				}
			}
			return result;
		}
		else
		{
			ASSERT(dataIndices.size() == vertexOffsetCounts.size());
			std::vector<data_t> result(numVertices);
			for (uint32 i = 0; i < (uint32)dataIndices.size(); ++i)
			{
				int32 dataIndex = dataIndices[i];
				data_t out = (dataIndex == -1) ? data_t(0.f) : data[dataIndex];
				uint32 offset = vertexOffsetCounts[i].offset;
				uint32 count = vertexOffsetCounts[i].count;
				for (uint32 j = 0; j < count; ++j)
				{
					uint32 vertexIndex = originalToNewVertex[j + offset];
					result[vertexIndex] = out;
				}
			}
			return result;
		}
	}
	else if (mapping == mapping_info_all_same)
	{
		ASSERT(data.size() == 1);
		std::vector<data_t> result(numVertices, data[0]);
		return result;
	}

	ASSERT(false);
	return {};
}




struct fbx_object
{
	int64 id;
	sized_string name;
};

struct fbx_mesh : fbx_object
{
	mesh_geometry geometry;
};

struct fbx_texture : fbx_object
{
	sized_string filename;
	sized_string relativeFilename;
};

struct fbx_material : fbx_object
{
	sized_string shadingModel;
	int32 multiLayer;
	vec3 diffuseColor;
	vec3 ambientColor;
	float ambientFactor;
	vec3 specularColor;
	float specularFactor;
	float shininess;
	float shininessExponent;
	vec3 reflectionColor;

	fbx_texture* albedoTexture;
	fbx_texture* normalTexture;
	fbx_texture* roughnessTexture;
	fbx_texture* metallicTexture;
};

struct fbx_model : fbx_object
{
	quat localRotation = quat::identity;
	vec3 localTranslation = vec3(0.f);

	std::vector<fbx_mesh*> meshes;
	std::vector<fbx_material*> materials;
};

static std::pair<int64, sized_string> readObjectIDAndName(const fbx_node& node, const std::vector<fbx_property>& properties)
{
	int64 id = 0;
	sized_string name = {};

	for (const fbx_property& prop : fbx_property_iterator{ &node, properties })
	{
		if (prop.type == fbx_property_type_int64)
		{
			id = readInt64(prop);
		}
		if (prop.type == fbx_property_type_string && name.length == 0)
		{
			name = readString(prop);
		}
	}

	return { id, name };
}

static fbx_model readModel(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_model result = {};
	result.id = id;
	result.name = name;

	const fbx_node* propertiesNode = node.findChild(nodes, "Properties70");
	for (const fbx_node& P : fbx_node_iterator{ propertiesNode, nodes })
	{
		ASSERT(P.name == "P");

		auto propIterator = fbx_property_iterator{ &P, properties }.begin();
		sized_string type = readString(*propIterator++);
		if (type == "Lcl Translation")
		{
			++propIterator;
			++propIterator;
			float x = (float)readDouble(*propIterator++);
			float y = (float)readDouble(*propIterator++);
			float z = (float)readDouble(*propIterator++);
			result.localTranslation = vec3(x, y, z);
		}
		else if (type == "Lcl Rotation")
		{
			++propIterator;
			++propIterator;
			float x = (float)readDouble(*propIterator++);
			float y = (float)readDouble(*propIterator++);
			float z = (float)readDouble(*propIterator++);
			// TODO
		}
	}

	return result;
}

static fbx_mesh readMesh(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_mesh result = {};
	result.id = id;
	result.name = name;

	const fbx_node* positionsNode = node.findChild(nodes, "Vertices");
	std::vector<double> originalPositionsRaw = readDoubleArray(*positionsNode->getFirstProperty(properties));

	const fbx_node* indicesNode = node.findChild(nodes, "PolygonVertexIndex");
	std::vector<int32> originalIndices = readInt32Array(*indicesNode->getFirstProperty(properties));

	if (!isTriangleMesh(originalIndices))
	{
		return result;
	}

	struct vec2d
	{
		double x, y;
	};

	struct vec3d
	{
		double x, y, z;
	};

	vec3d* originalPositionsPtr = (vec3d*)originalPositionsRaw.data();
	uint32 numOriginalPositions = (uint32)originalPositionsRaw.size() / 3;


	result.geometry.positions.reserve(originalIndices.size());

	std::vector<offset_count> vertexOffsetCounts(numOriginalPositions, { 0, 0 });
	std::vector<uint32> originalToNewVertex(originalIndices.size());

	for (int32 index : originalIndices)
	{
		int32 decodedIndex = decodeIndex(index);
		vec3d position = originalPositionsPtr[decodedIndex];
		result.geometry.positions.push_back(vec3((float)position.x, (float)position.y, (float)position.z));
		++vertexOffsetCounts[decodedIndex].count;
	}

	uint32 offset = 0;
	for (uint32 i = 0; i < numOriginalPositions; ++i)
	{
		vertexOffsetCounts[i].offset = offset;
		offset += vertexOffsetCounts[i].count;
		vertexOffsetCounts[i].count = 0;
	}

	for (uint32 i = 0; i < (uint32)originalIndices.size(); ++i)
	{
		int32 index = originalIndices[i];
		int32 decodedIndex = decodeIndex(index);
		uint32 offset = vertexOffsetCounts[decodedIndex].offset;
		uint32 count = vertexOffsetCounts[decodedIndex].count++;
		originalToNewVertex[offset + count] = i;
	}




	// UVs.
	{
		const fbx_node* uvNode = findNode(nodes, { "Objects", "Geometry", "LayerElementUV" });
		auto [raw, indices, mapping, reference] = readGeometryData<double>(uvNode, nodes, properties, "UV", "UVIndex");
		ASSERT(raw.size() % 2 == 0);

		if (raw.size())
		{
			result.geometry.uvs.resize(raw.size() / 2);
			vec2d* ptr = (vec2d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.geometry.uvs.size(); ++i)
			{
				result.geometry.uvs[i] = vec2((float)ptr[i].x, (float)ptr[i].y);
			}

			result.geometry.uvs = mapDataToVertices(result.geometry.uvs, indices, mapping, reference, vertexOffsetCounts, originalToNewVertex,
				(uint32)result.geometry.positions.size());
		}
	}


	// Normals.
	{
		const fbx_node* normalsNode = findNode(nodes, { "Objects", "Geometry", "LayerElementNormal" });
		auto [raw, indices, mapping, reference] = readGeometryData<double>(normalsNode, nodes, properties, "Normals", "NormalsIndex");
		ASSERT(raw.size() % 3 == 0);

		if (raw.size())
		{
			result.geometry.normals.resize(raw.size() / 3);
			vec3d* ptr = (vec3d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.geometry.normals.size(); ++i)
			{
				result.geometry.normals[i] = vec3((float)ptr[i].x, (float)ptr[i].y, (float)ptr[i].z);
			}

			result.geometry.normals = mapDataToVertices(result.geometry.normals, indices, mapping, reference, vertexOffsetCounts, originalToNewVertex,
				(uint32)result.geometry.positions.size());
		}
	}


	// Materials.
	{
		const fbx_node* materialsNode = node.findChild(nodes, "LayerElementMaterial");
		auto [materials, _, mapping, reference] = readGeometryData<int32>(materialsNode, nodes, properties, "Materials", "");
	}


	result.geometry.indices.resize(originalIndices.size());
	for (int32 i = 0; i < result.geometry.indices.size(); ++i)
	{
		result.geometry.indices[i] = i;
	}


	result.geometry = removeDuplicateVertices(result.geometry);

	return result;
}

static fbx_material readMaterial(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_material result = {};
	result.id = id;
	result.name = name;

	for (const fbx_node& child : fbx_node_iterator{ &node, nodes })
	{
		if (child.name == "ShadingModel")
		{
			result.shadingModel = readString(*child.getFirstProperty(properties));
		}
		else if (child.name == "MultiLayer")
		{
			result.multiLayer = readInt32(*child.getFirstProperty(properties));
		}
		else if (child.name == "Properties70")
		{
			for (const fbx_node& P : fbx_node_iterator{ &child, nodes })
			{
				ASSERT(P.name == "P");

				auto propIterator = fbx_property_iterator{ &P, properties }.begin();
				sized_string name = readString(*propIterator++);
				sized_string type = readString(*propIterator++);
				sized_string dummy = readString(*propIterator++);

				vec3 color(0.f, 0.f, 0.f);
				float value = 0.f;

				if (type == "Color")
				{
					color.x = (float)readDouble(*propIterator++);
					color.y = (float)readDouble(*propIterator++);
					color.z = (float)readDouble(*propIterator++);
				}
				else if (type == "Number")
				{
					value = (float)readDouble(*propIterator++);
				}

				if (name == "DiffuseColor")
				{
					result.diffuseColor = color;
				}
				else if (name == "AmbientColor")
				{
					result.ambientColor = color;
				}
				else if (name == "AmbientFactor")
				{
					result.ambientFactor = value;
				}
				else if (name == "SpecularColor")
				{
					result.specularColor = color;
				}
				else if (name == "SpecularFactor")
				{
					result.specularFactor = value;
				}
				else if (name == "Shininess")
				{
					result.shininess = value;
				}
				else if (name == "ShininessExponent")
				{
					result.shininessExponent = value;
				}
				else if (name == "ReflectionColor")
				{
					result.reflectionColor = color;
				}
			}
		}
	}

	ASSERT(result.shadingModel == "Phong");

	return result;
}

static fbx_texture readTexture(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_texture result = {};
	result.id = id;
	result.name = name;

	for (const fbx_node& child : fbx_node_iterator{ &node, nodes })
	{
		if (child.name == "FileName")
		{
			result.filename = readString(*child.getFirstProperty(properties));
		}
		else if (child.name == "RelativeFilename")
		{
			result.relativeFilename = readString(*child.getFirstProperty(properties));
		}
	}

	return result;
}

static void readConnection(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties,
	std::vector<fbx_model>& models, std::vector<fbx_mesh>& meshes, std::vector<fbx_material>& materials, std::vector<fbx_texture>& textures)
{
		ASSERT(node.name == "C");

		auto propIterator = fbx_property_iterator{ &node, properties }.begin();
		sized_string type = readString(*propIterator++);
		int64 a = readInt64(*propIterator++);
		int64 b = readInt64(*propIterator++);

		if (type == "OO")
		{
			// Object-object connection.

			fbx_model* model = 0;
			fbx_mesh* mesh = 0;
			fbx_material* material = 0;

			for (fbx_model& m : models)
			{
				if (m.id == a || m.id == b)
				{
					model = &m;
					break;
				}
			}
			for (fbx_mesh& m : meshes)
			{
				if (m.id == a || m.id == b)
				{
					mesh = &m;
					break;
				}
			}
			for (fbx_material& m : materials)
			{
				if (m.id == a || m.id == b)
				{
					material = &m;
					break;
				}
			}
			
			if (model && mesh)
			{
				model->meshes.push_back(mesh);
			}
			else if (model && material)
			{
				model->materials.push_back(material);
			}
		}
		else if (type == "OP")
		{
			// Object-property connection.

			fbx_material* material = 0;
			fbx_texture* texture = 0;

			for (fbx_material& m : materials)
			{
				if (m.id == a || m.id == b)
				{
					material = &m;
					break;
				}
			}
			for (fbx_texture& t : textures)
			{
				if (t.id == a || t.id == b)
				{
					texture = &t;
					break;
				}
			}

			if (material && texture)
			{
				sized_string textureSlot = readString(*propIterator++);
				if (textureSlot == "DiffuseColor")
				{
					material->albedoTexture = texture;
				}
				else if (textureSlot == "NormalMap")
				{
					material->normalTexture = texture;
				}
				else if (textureSlot == "ShininessExponent")
				{
					material->roughnessTexture = texture;
				}
				else if (textureSlot == "ReflectionFactor")
				{
					material->metallicTexture = texture;
				}
				else
				{
					ASSERT(false);
				}
			}
		}
}

void loadFBX(const fs::path& path)
{
	std::string pathStr = path.string();
	const char* s = pathStr.c_str();
	entire_file file = loadFile(path);
	if (file.size < sizeof(fbx_header))
	{
		printf("File '%s' is smaller than FBX header.\n", s);
		freeFile(file);
		return;
	}

	fbx_header* header = file.consume<fbx_header>();
	if ((strcmp(header->magic, "Kaydara FBX Binary  ") != 0) || header->unknown[0] != 0x1A || header->unknown[1] != 0x00)
	{
		printf("Header of file '%s' does not match FBX spec.\n", s);
		freeFile(file);
		return;
	}

	uint32 version = header->version;

	std::vector<fbx_node> nodes;
	fbx_node node = {};
	node.parent = -1;
	node.level = -1;
	node.next = -1;
	node.firstChild = -1;
	node.lastChild = -1;
	nodes.push_back(node);

	std::vector<fbx_property> properties;

	parseNodes(version, file, nodes, properties, 0, 0);


#if 0
	FILE* outFile = fopen("fbx.txt", "w");
	printFBXContent(nodes, properties, nodes[0], outFile);
	fclose(outFile);
#endif

	std::vector<fbx_model> models;
	std::vector<fbx_mesh> meshes;
	std::vector<fbx_material> materials;
	std::vector<fbx_texture> textures;

	for (const fbx_node& objectNode : fbx_node_iterator{ findNode(nodes, { "Objects" }), nodes })
	{
		if (objectNode.name == "Model")
		{
			models.push_back(readModel(objectNode, nodes, properties));
		}
		if (objectNode.name == "Geometry")
		{
			meshes.push_back(readMesh(objectNode, nodes, properties));
		}
		else if (objectNode.name == "Material")
		{
			materials.push_back(readMaterial(objectNode, nodes, properties));
		}
		else if (objectNode.name == "Texture")
		{
			textures.push_back(readTexture(objectNode, nodes, properties));
		}
	}


	for (const fbx_node& connectionNode : fbx_node_iterator{ findNode(nodes, { "Connections" }), nodes })
	{
		readConnection(connectionNode, nodes, properties, models, meshes, materials, textures);
	}

	if (!meshes.empty())
	{
		const fbx_mesh& mesh = meshes.front();
		testDumpToPLY(mesh.geometry.positions, mesh.geometry.uvs, mesh.geometry.normals, mesh.geometry.indices);
	}

	freeFile(file);
}










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

static void testDumpToPLY(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<int32>& indices)
{
	std::ofstream outfile;
	std::ios::openmode mode = std::ios::out | std::ios::trunc | std::ios::binary;
	outfile.open("out.ply", mode);

	uint32 numPoints = (uint32)positions.size();
	uint32 numFaces = (uint32)indices.size() / 3;
	bool writeUVs = uvs.size() > 0;
	bool writeNormals = normals.size() > 0;
	writeHeaderToFile(outfile, numPoints, writeUVs, writeNormals, false, numFaces);

	for (uint32 i = 0; i < numPoints; ++i)
	{
		write(outfile, positions[i]);
		if (writeUVs) { write(outfile, uvs[i]); }
		if (writeNormals) { write(outfile, normals[i]); }
	}

	for (uint32 i = 0; i < (uint32)indices.size(); i += 3)
	{
		uint8 count = 3;
		write(outfile, count);
		write(outfile, indices[i + 0]);
		write(outfile, indices[i + 1]);
		write(outfile, indices[i + 2]);
	}

	outfile.close();
}

