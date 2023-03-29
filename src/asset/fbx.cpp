#include "pch.h"
#include "deflate.h"
#include "core/math.h"
#include "geometry/mesh.h"

#include "mesh_postprocessing.h"

#include "core/yaml.h"


static void testDumpToPLY(const std::string& filename,
	const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<indexed_triangle16>& triangles,
	uint8 r = 255, uint8 g = 255, uint8 b = 255);


enum mesh_flags
{
	mesh_flag_load_uvs,
	mesh_flag_load_normals,
	mesh_flag_load_materials,
};



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


struct fbx_node_wrapper
{
	const fbx_node& node;
	const std::vector<fbx_node>& nodes;
	const std::vector<fbx_property>& properties;
};

struct fbx_prop_wrapper
{
	const fbx_property& prop;
};

namespace YAML
{
	template<>
	struct convert<fbx_property>
	{
		static Node encode(const fbx_property& prop)
		{
			Node n;
			Node dataNode;
			dataNode.SetStyle(EmitterStyle::Flow);

			fbx_property_type type = prop.type;

			switch (type)
			{
				case fbx_property_type_bool:
				{
					n["Type"] = "Bool";
					if (prop.numElements == 1) { dataNode = *(bool*)prop.data; }
					else if (prop.encoding == 0) {for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((bool*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_float:
				{
					n["Type"] = "Float";
					if (prop.numElements == 1) { dataNode = *(float*)prop.data; }
					else if (prop.encoding == 0) { for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((float*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_double:
				{
					n["Type"] = "Double";
					if (prop.numElements == 1) { dataNode = *(double*)prop.data; }
					else if (prop.encoding == 0) { for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((double*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_int16:
				{
					n["Type"] = "Int16";
					if (prop.numElements == 1) { dataNode = *(int16*)prop.data; }
					else if (prop.encoding == 0) { for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((int16*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_int32:
				{
					n["Type"] = "Int32";
					if (prop.numElements == 1) { dataNode = *(int32*)prop.data; }
					else if (prop.encoding == 0) { for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((int32*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_int64:
				{
					n["Type"] = "Int64";
					if (prop.numElements == 1) { dataNode = *(int64*)prop.data; }
					else if (prop.encoding == 0) { for (uint32 j = 0; j < prop.numElements; ++j) { dataNode.push_back(((int64*)prop.data)[j]); } }
					else { dataNode.push_back(std::to_string(prop.numElements) + " compressed elements"); }
				} break;
				case fbx_property_type_string:
				{
					n["Type"] = "String";
					dataNode = std::string((const char*)prop.data, prop.numElements);
				} break;
				case fbx_property_type_raw:
				{
					n["Type"] = "Raw";
				} break;
				default:
				{
					ASSERT(false);
				} break;
			}

			n["Data"] = dataNode;
			return n;
		}
	};

	template<>
	struct convert<fbx_node_wrapper>
	{
		static Node encode(const fbx_node_wrapper& c)
		{
			Node n;

			for (const fbx_property& prop : fbx_property_iterator{ &c.node, c.properties })
			{
				n.push_back(prop);
			}

			for (const fbx_node& node : fbx_node_iterator{ &c.node, c.nodes })
			{
				std::string key(node.name.str, node.name.length);

				Node nestedNode;
				nestedNode[key] = fbx_node_wrapper{ node, c.nodes, c.properties };
				n.push_back(nestedNode);
			}

			return n;
		}
	};
}

static void writeFBXContentToYAML(const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, const fbx_node& parent, YAML::Node& out)
{
	out = fbx_node_wrapper{ parent, nodes, properties };
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
	else if (mapping == mapping_info_by_polygon)
	{
		return {};
	}
	else if (mapping == mapping_info_all_same)
	{
		ASSERT(dataIndices.size() == 0);
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

struct fbx_model;
struct fbx_mesh;
struct fbx_material;
struct fbx_texture;
struct fbx_deformer;

struct fbx_global_settings
{
	int32 upAxis;
	int32 upAxisSign;

	int32 frontAxis;
	int32 frontAxisSign;

	int32 coordAxis;
	int32 coordAxisSign;

	int32 originalUpAxis;
	int32 originalUpAxisSign;
};

static fbx_global_settings readGlobalSettings(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	const fbx_node* properties70 = node.findChild(nodes, "Properties70");

	fbx_global_settings result = {};

	for (const fbx_node& P : fbx_node_iterator{ properties70, nodes })
	{
		ASSERT(P.name == "P");

		auto propIterator = fbx_property_iterator{ &P, properties }.begin();
		sized_string name = readString(*propIterator++);

		auto readAxis = [&propIterator]()
		{
			sized_string dataType = readString(*propIterator++);
			ASSERT(dataType == "int");

			sized_string longDataType = readString(*propIterator++);
			ASSERT(longDataType == "Integer");

			int32 value = readInt32(*propIterator++);
			return value;
		};

		if (name == "UpAxis") { result.upAxis = readAxis(); }
		else if (name == "UpAxisSign") { result.upAxisSign = readAxis(); }
		else if (name == "FrontAxis") { result.frontAxis = readAxis(); }
		else if (name == "FrontAxisSign") { result.frontAxisSign = readAxis(); }
		else if (name == "CoordAxis") { result.coordAxis = readAxis(); }
		else if (name == "CoordAxisSign") { result.coordAxisSign = readAxis(); }
		else if (name == "OriginalUpAxis") { result.originalUpAxis = readAxis(); }
		else if (name == "OriginalUpAxisSign") { result.originalUpAxisSign = readAxis(); }
	}

	return result;
}

struct fbx_model : fbx_object
{
	quat localRotation = quat::identity;
	vec3 localTranslation = vec3(0.f);

	std::vector<fbx_mesh*> meshes;
	std::vector<fbx_material*> materials;
	std::vector<fbx_model*> children;

	fbx_model* parent;
	fbx_deformer* deformer;
};

struct fbx_submesh
{
	int32 materialIndex;

	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<skinning_weights> skin;

	std::vector<indexed_triangle16> triangles;
};

struct fbx_mesh : fbx_object
{
	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<skinning_weights> skin;
	std::vector<int32> originalIndices;

	std::vector<offset_count> vertexOffsetCounts;
	std::vector<uint32> originalToNewVertex;

	std::vector<int32> materialIndexPerFace;

	fbx_deformer* deformer;

	std::vector<fbx_submesh> submeshes;
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

struct fbx_texture : fbx_object
{
	sized_string filename;
	sized_string relativeFilename;
};

struct fbx_deformer : fbx_object
{
	std::vector<int32> vertexIndices;
	std::vector<float> weights;
	std::vector<fbx_deformer*> joints;

	fbx_model* model;
	fbx_deformer* parentJoint;

	mat4 invBindMatrix;
};

enum fbx_connection_type
{
	fbx_connection_type_object_object,
	fbx_connection_type_object_property,
};

enum fbx_texture_slot
{
	fbx_texture_slot_none,
	fbx_texture_slot_albedo,
	fbx_texture_slot_normal,
	fbx_texture_slot_roughness,
	fbx_texture_slot_metallic,
};

struct fbx_connection
{
	int64 idA;
	int64 idB;
	fbx_connection_type type;
	fbx_texture_slot textureSlot;
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

static bool readModel(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, std::vector<fbx_model>& outModels)
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

	outModels.push_back(std::move(result));
	return true;
}

static bool readMesh(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, uint32 flags, std::vector<fbx_mesh>& outMeshes)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_mesh result = {};
	result.id = id;
	result.name = name;

	const fbx_node* positionsNode = node.findChild(nodes, "Vertices");
	std::vector<double> originalPositionsRaw = readDoubleArray(*positionsNode->getFirstProperty(properties));

	const fbx_node* indicesNode = node.findChild(nodes, "PolygonVertexIndex");
	result.originalIndices = readInt32Array(*indicesNode->getFirstProperty(properties));

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



	result.positions.reserve(result.originalIndices.size());

	result.vertexOffsetCounts.resize(numOriginalPositions, { 0, 0 });
	result.originalToNewVertex.resize(result.originalIndices.size());
	uint32 numFaces = 0;

	for (int32 index : result.originalIndices)
	{
		int32 decodedIndex = decodeIndex(index);
		vec3d position = originalPositionsPtr[decodedIndex];
		result.positions.push_back(vec3((float)position.x, (float)position.y, (float)position.z));
		++result.vertexOffsetCounts[decodedIndex].count;

		if (index < 0)
		{
			++numFaces;
		}
	}

	uint32 offset = 0;
	for (uint32 i = 0; i < numOriginalPositions; ++i)
	{
		result.vertexOffsetCounts[i].offset = offset;
		offset += result.vertexOffsetCounts[i].count;
		result.vertexOffsetCounts[i].count = 0;
	}

	for (uint32 i = 0; i < (uint32)result.originalIndices.size(); ++i)
	{
		int32 index = result.originalIndices[i];
		int32 decodedIndex = decodeIndex(index);
		uint32 offset = result.vertexOffsetCounts[decodedIndex].offset;
		uint32 count = result.vertexOffsetCounts[decodedIndex].count++;
		result.originalToNewVertex[offset + count] = i;
	}




	// UVs.
	if (flags & mesh_flag_load_uvs)
	{
		const fbx_node* uvNode = node.findChild(nodes, "LayerElementUV");
		auto [raw, indices, mapping, reference] = readGeometryData<double>(uvNode, nodes, properties, "UV", "UVIndex");
		ASSERT(raw.size() % 2 == 0);

		if (raw.size())
		{
			result.uvs.resize(raw.size() / 2);
			vec2d* ptr = (vec2d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.uvs.size(); ++i)
			{
				result.uvs[i] = vec2((float)ptr[i].x, (float)ptr[i].y);
			}

			result.uvs = mapDataToVertices(result.uvs, indices, mapping, reference, result.vertexOffsetCounts, result.originalToNewVertex,
				(uint32)result.positions.size());
		}
	}


	// Normals.
	if (flags & mesh_flag_load_normals)
	{
		const fbx_node* normalsNode = node.findChild(nodes, "LayerElementNormal");
		auto [raw, indices, mapping, reference] = readGeometryData<double>(normalsNode, nodes, properties, "Normals", "NormalsIndex");
		ASSERT(raw.size() % 3 == 0);

		if (raw.size())
		{
			result.normals.resize(raw.size() / 3);
			vec3d* ptr = (vec3d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.normals.size(); ++i)
			{
				result.normals[i] = vec3((float)ptr[i].x, (float)ptr[i].y, (float)ptr[i].z);
			}

			result.normals = mapDataToVertices(result.normals, indices, mapping, reference, result.vertexOffsetCounts, result.originalToNewVertex,
				(uint32)result.positions.size());
		}
	}



	// Materials.
	if (flags & mesh_flag_load_materials)
	{
		const fbx_node* materialsNode = node.findChild(nodes, "LayerElementMaterial");
		auto [materials, indices, mapping, reference] = readGeometryData<int32>(materialsNode, nodes, properties, "Materials", "");

		if (materials.size() > 0)
		{
			if (mapping == mapping_info_all_same)
			{
				int32 materialIndex = !materials.empty() ? materials[0] : 0;
				result.materialIndexPerFace.resize(numFaces, materialIndex);
			}
			else if (mapping == mapping_info_by_polygon)
			{
				result.materialIndexPerFace = std::move(materials);
			}
			else
			{
				ASSERT(false);
			}
		}
	}
	if (result.materialIndexPerFace.empty())
	{
		result.materialIndexPerFace.resize(numFaces, 0);
	}

	outMeshes.push_back(std::move(result));
	return true;
}

static bool readMaterial(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, std::vector<fbx_material>& outMaterials)
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
				else
				{
					printf("Material property '%.*s' not handled.\n", name.length, name.str);
				}
			}
		}
	}

	ASSERT(result.shadingModel == "Phong" || result.shadingModel == "phong");

	outMaterials.push_back(std::move(result));
	return true;
}

static bool readTexture(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, std::vector<fbx_texture>& outTextures)
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

	outTextures.push_back(std::move(result));
	return true;
}

static bool readDeformer(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, std::vector<fbx_deformer>& outDeformers)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_deformer result = {};
	result.id = id;
	result.name = name;
	result.invBindMatrix = mat4::identity;

	for (const fbx_node& child : fbx_node_iterator{ &node, nodes })
	{
		if (child.name == "Indexes")
		{
			result.vertexIndices = readInt32Array(*child.getFirstProperty(properties));
		}
		else if (child.name == "Weights")
		{
			std::vector<double> weights = readDoubleArray(*child.getFirstProperty(properties));
			result.weights.resize(weights.size());
			for (uint32 i = 0; i < (uint32)weights.size(); ++i)
			{
				result.weights[i] = (float)weights[i];
			}
		}
		else if (child.name == "Transform")
		{
			std::vector<double> matrix = readDoubleArray(*child.getFirstProperty(properties));
			ASSERT(matrix.size() == 16);
			for (uint32 i = 0; i < 16; ++i)
			{
				result.invBindMatrix.m[i] = (float)matrix[i];
			}
		}
	}

	ASSERT(result.vertexIndices.size() == result.weights.size());

	outDeformers.push_back(std::move(result));
	return true;
}

static bool readConnection(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, std::vector<fbx_connection>& outConnections)
{
	ASSERT(node.name == "C");

	fbx_connection result;

	auto propIterator = fbx_property_iterator{ &node, properties }.begin();
	sized_string type = readString(*propIterator++);
	result.idA = readInt64(*propIterator++);
	result.idB = readInt64(*propIterator++);

	if (type == "OO")
	{
		// Object-object connection.
		result.type = fbx_connection_type_object_object;
	}
	else if (type == "OP")
	{
		// Object-property connection.
		result.type = fbx_connection_type_object_property;

		if (propIterator != fbx_property_iterator{ &node, properties }.end())
		{
			sized_string textureSlot = readString(*propIterator++);
			if (textureSlot == "DiffuseColor")
			{
				result.textureSlot = fbx_texture_slot_albedo;
			}
			else if (textureSlot == "NormalMap")
			{
				result.textureSlot = fbx_texture_slot_normal;
			}
			else if (textureSlot == "ShininessExponent")
			{
				result.textureSlot = fbx_texture_slot_roughness;
			}
			else if (textureSlot == "ReflectionFactor")
			{
				result.textureSlot = fbx_texture_slot_metallic;
			}
		}
	}

	outConnections.push_back(result);
	return true;
}

template <typename T>
static T* findObjectWithID(std::vector<T>& objects, int64 id)
{
	for (T& t : objects)
	{
		if (t.id == id)
		{
			return &t;
		}
	}
	return 0;
}

static void resolveConnections(std::vector<fbx_model>& models, std::vector<fbx_mesh>& meshes, std::vector<fbx_material>& materials, std::vector<fbx_texture>& textures,
	std::vector<fbx_deformer>& deformers, const std::vector<fbx_connection>& connections)
{
	for (fbx_connection conn : connections)
	{
		if (conn.type == fbx_connection_type_object_object)
		{
			fbx_model* modelA = findObjectWithID(models, conn.idA);
			fbx_model* modelB = findObjectWithID(models, conn.idB);
			fbx_mesh* meshA = findObjectWithID(meshes, conn.idA);
			fbx_mesh* meshB = findObjectWithID(meshes, conn.idB);
			fbx_material* materialA = findObjectWithID(materials, conn.idA);
			fbx_material* materialB = findObjectWithID(materials, conn.idB);
			fbx_deformer* deformerA = findObjectWithID(deformers, conn.idA);
			fbx_deformer* deformerB = findObjectWithID(deformers, conn.idB);

			if (modelA && modelB)
			{
				modelA->parent = modelB;
				modelB->children.push_back(modelA);
			}
			else if (modelA && deformerB)
			{
				deformerB->model = modelA;
				modelA->deformer = deformerB;
			}
			else if (meshA && modelB)
			{
				modelB->meshes.push_back(meshA);
			}
			else if (deformerA && meshB)
			{
				meshB->deformer = deformerA;
			}
			else if (deformerA && deformerB)
			{
				deformerB->joints.push_back(deformerA);
			}
			else if (materialA && modelB)
			{
				modelB->materials.push_back(materialA);
			}
			else
			{
				int a = 0;
			}
			// TODO: Note Attributes
		}
		else if (conn.type == fbx_connection_type_object_property)
		{
			fbx_material* materialA = findObjectWithID(materials, conn.idA);
			fbx_material* materialB = findObjectWithID(materials, conn.idB);
			fbx_texture* textureA = findObjectWithID(textures, conn.idA);
			fbx_texture* textureB = findObjectWithID(textures, conn.idB);

			if (textureA && materialB)
			{
				if (conn.textureSlot == fbx_texture_slot_albedo) { materialB->albedoTexture = textureA; }
				else if (conn.textureSlot == fbx_texture_slot_normal) { materialB->normalTexture = textureA; }
				else if (conn.textureSlot == fbx_texture_slot_roughness) { materialB->roughnessTexture = textureA; }
				else if (conn.textureSlot == fbx_texture_slot_metallic) { materialB->metallicTexture = textureA; }
			}
		}
	}

	for (fbx_deformer& deformer : deformers)
	{
		if (fbx_model* model = deformer.model)
		{
			if (fbx_model* parentModel = model->parent)
			{
				if (fbx_deformer* parentDeformer = parentModel->deformer)
				{
					deformer.parentJoint = parentDeformer;
				}
			}
		}
	}
}

static void finishMesh(fbx_mesh& mesh)
{
	// Assign materials and skinning weights, remove duplicate vertices and triangulate.

	if (mesh.deformer)
	{
		mesh.skin.resize(mesh.positions.size(), {});

		for (int32 jointID = 0; jointID < (int32)mesh.deformer->joints.size(); ++jointID)
		{
			fbx_deformer* joint = mesh.deformer->joints[jointID];

			auto& indices = joint->vertexIndices;
			auto& weights = joint->weights;
			ASSERT(indices.size() == weights.size());

			for (uint32 i = 0; i < (uint32)indices.size(); ++i)
			{
				int32 index = indices[i];
				uint8 weight = (uint8)clamp(weights[i] * 255.f, 0.f, 255.f);
				if (weight == 0)
				{
					continue;
				}

				uint32 offset = mesh.vertexOffsetCounts[index].offset;
				uint32 count = mesh.vertexOffsetCounts[index].count;
				for (uint32 j = 0; j < count; ++j)
				{
					uint32 vertexIndex = mesh.originalToNewVertex[j + offset];

					skinning_weights& skin = mesh.skin[vertexIndex];

					int32 slot = -1;
					for (int32 k = 0; k < 4; ++k)
					{
						if (skin.skinWeights[k] < weight)
						{
							slot = k;
							break;
						}
					}
					if (slot != -1)
					{
						if (skin.skinWeights[3] != 0)
						{
							printf("Warning: Vertex has more than 4 joints attached.\n");
						}
						for (int32 k = 3; k > slot; --k)
						{
							skin.skinIndices[k] = skin.skinIndices[k - 1];
							skin.skinWeights[k] = skin.skinWeights[k - 1];
						}
						skin.skinIndices[slot] = jointID;
						skin.skinWeights[slot] = weight;
					}
					else
					{
						printf("Warning: Vertex has more than 4 joints attached.\n");
					}
				}
			}
		}
	}

	struct per_material
	{
		std::unordered_map<full_vertex, uint16> vertexToIndex;
		fbx_submesh sub;

		void addTriangles(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<skinning_weights>& skins,
			int32 firstIndex, int32 faceSize, std::vector<fbx_submesh>& outSubmeshes)
		{
			if (faceSize < 3)
			{
				// Ignore lines and points.
				return;
			}

			int32 aIndex = firstIndex++;
			int32 bIndex = firstIndex++;
			add_vertex_result a = addVertex(positions, uvs, normals, skins, aIndex);
			add_vertex_result b = addVertex(positions, uvs, normals, skins, bIndex);
			for (int32 i = 2; i < faceSize; ++i)
			{
				int32 cIndex = firstIndex++;
				add_vertex_result c = addVertex(positions, uvs, normals, skins, cIndex);

				if (!(a.success && b.success && c.success))
				{
					flush(outSubmeshes);
					a = addVertex(positions, uvs, normals, skins, aIndex);
					b = addVertex(positions, uvs, normals, skins, bIndex);
					c = addVertex(positions, uvs, normals, skins, cIndex);
					printf("Too many vertices for 16-bit indices. Splitting mesh!\n");
				}

				sub.triangles.push_back(indexed_triangle16{ a.index, b.index, c.index });

				b = c;
				bIndex = cIndex;
			}
		}

		void flush(std::vector<fbx_submesh>& outSubmeshes)
		{
			if (vertexToIndex.size() > 0)
			{
				outSubmeshes.push_back(std::move(sub));
				vertexToIndex.clear();
			}
		}

	private:

		struct add_vertex_result
		{
			uint16 index;
			bool success;
		};

		add_vertex_result addVertex(const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<skinning_weights>& skins,
			int32 index)
		{
			vec3 position = positions[index];
			vec2 uv = !uvs.empty() ? uvs[index] : vec2(0.f, 0.f);
			vec3 normal = !normals.empty() ? normals[index] : vec3(0.f, 0.f, 0.f);
			skinning_weights skin = !skins.empty() ? skins[index] : skinning_weights{};

			full_vertex vertex = { position, uv, normal, skin, };
			auto it = vertexToIndex.find(vertex);
			if (it == vertexToIndex.end())
			{
				uint32 vertexIndex = (uint32)sub.positions.size();
				if (vertexIndex > UINT16_MAX)
				{
					return { 0, false };
				}


				vertexToIndex.insert({ vertex, (uint16)vertexIndex });

				sub.positions.push_back(position);
				if (!uvs.empty()) { sub.uvs.push_back(uv); }
				if (!normals.empty()) { sub.normals.push_back(normal); }
				if (!skins.empty()) { sub.skin.push_back(skin); }

				return { (uint16)vertexIndex, true };
			}
			else
			{
				return { it->second, true };
			}
		}
	};

	std::unordered_map<int32, per_material> materialToMesh;


	int32 faceIndex = 0;
	for (int32 firstIndex = 0, end = (int32)mesh.originalIndices.size(); firstIndex < end;)
	{
		int32 faceSize = 0;
		while (firstIndex + faceSize < end)
		{
			int32 i = firstIndex + faceSize++;
			if (mesh.originalIndices[i] < 0)
			{
				break;
			}
		}

		int32 material = mesh.materialIndexPerFace[faceIndex++];

		per_material& perMat = materialToMesh[material];
		perMat.sub.materialIndex = material;

		perMat.addTriangles(mesh.positions, mesh.uvs, mesh.normals, mesh.skin, firstIndex, faceSize, mesh.submeshes);

		firstIndex += faceSize;
	}

	for (auto [i, perMat] : materialToMesh)
	{
		perMat.flush(mesh.submeshes);
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

void loadFBX(const fs::path& path)
{
	uint32 flags = mesh_flag_load_uvs | mesh_flag_load_normals | mesh_flag_load_materials;

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
	{
		YAML::Node out;
		writeFBXContentToYAML(nodes, properties, nodes[0], out);
		std::ofstream fout("fbx.yaml");
		fout << out;
		return;
	}
#endif

	fbx_global_settings globalSettings = readGlobalSettings(*findNode(nodes, { "GlobalSettings" }), nodes, properties);

	std::vector<fbx_model> models;
	std::vector<fbx_mesh> meshes;
	std::vector<fbx_material> materials;
	std::vector<fbx_texture> textures;
	std::vector<fbx_deformer> deformers;
	std::vector<fbx_connection> connections;

	for (const fbx_node& objectNode : fbx_node_iterator{ findNode(nodes, { "Objects" }), nodes })
	{
		if (objectNode.name == "Model")
		{
			readModel(objectNode, nodes, properties, models);
		}
		if (objectNode.name == "Geometry")
		{
			readMesh(objectNode, nodes, properties, flags, meshes);
		}
		else if (objectNode.name == "Material")
		{
			if (flags & mesh_flag_load_materials)
			{
				readMaterial(objectNode, nodes, properties, materials);
			}
		}
		else if (objectNode.name == "Texture")
		{
			if (flags & mesh_flag_load_materials)
			{
				readTexture(objectNode, nodes, properties, textures);
			}
		}
		else if (objectNode.name == "Deformer")
		{
			readDeformer(objectNode, nodes, properties, deformers);
		}
	}


	for (const fbx_node& connectionNode : fbx_node_iterator{ findNode(nodes, { "Connections" }), nodes })
	{
		readConnection(connectionNode, nodes, properties, connections);
	}

	resolveConnections(models, meshes, materials, textures, deformers, connections);

	for (fbx_mesh& mesh : meshes)
	{
		finishMesh(mesh);
	}

	for (const fbx_model& model : models)
	{
		std::string name(model.name.str, model.name.length);
		for (uint32 i = 0; i < model.name.length; ++i)
		{
			if (name[i] == 0x0 || name[i] == 0x1)
			{
				name[i] = ' ';
			}
		}

		for (uint32 i = 0; i < (uint32)model.meshes.size(); ++i)
		{
			const fbx_mesh* mesh = model.meshes[i];
			std::string indexedName = name + "_" + std::to_string(i);

			for (uint32 j = 0; j < (uint32)mesh->submeshes.size(); ++j)
			{
				const fbx_submesh& sub = mesh->submeshes[j];

				vec3 diffuseColor = !model.materials.empty() ? model.materials[sub.materialIndex]->diffuseColor : vec3(1.f, 1.f, 1.f);

				std::string indexedName2 = indexedName + "_" + std::to_string(j) + ".ply";

				testDumpToPLY(indexedName2, sub.positions, sub.uvs, sub.normals, sub.triangles,
					(uint8)(diffuseColor.x * 255), (uint8)(diffuseColor.y * 255), (uint8)(diffuseColor.z * 255));
			}

		}
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

static void testDumpToPLY(const std::string& filename, 
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

