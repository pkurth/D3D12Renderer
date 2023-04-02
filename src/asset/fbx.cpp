#include "pch.h"
#include "deflate.h"
#include "io.h"
#include "model_asset.h"
#include "core/math.h"
#include "core/cpu_profiling.h"
#include "core/color.h"
#include "geometry/mesh.h"

#include "mesh_postprocessing.h"

#include "core/yaml.h"


void testDumpToPLY(const std::string& filename,
	const std::vector<vec3>& positions, const std::vector<vec2>& uvs, const std::vector<vec3>& normals, const std::vector<indexed_triangle16>& triangles,
	uint8 r = 255, uint8 g = 255, uint8 b = 255);


enum rotation_order
{
	rotation_order_xyz,
	rotation_order_xzy,
	rotation_order_yzx,
	rotation_order_yxz,
	rotation_order_zxy,
	rotation_order_zyx,
	rotation_order_spheric_xyz,

	rotation_order_count,
};


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


struct fbx_property;

struct fbx_node
{
	sized_string name;

	uint32 parent;
	uint32 next;
	uint32 firstChild;
	uint32 lastChild;
	uint32 level;
	uint32 numChildren;

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

	const fbx_node* getFirstChild(const std::vector<fbx_node>& nodes) const
	{
		return numChildren ? &nodes[firstChild] : 0;
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
		node.numChildren = 0;
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
			++outNodes[parent].numChildren;
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

static float convertTime(int64 t)
{
	return (float)((double)t / 46186158000.0);
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
struct fbx_animation_stack;
struct fbx_animation_layer;
struct fbx_animation_curve_node;
struct fbx_animation_curve;

struct fbx_definitions
{
	union
	{
		struct
		{
			int32 numGlobalSettings;
			//int32 numNodeAttributes;
			int32 numModels;
			int32 numMeshes;
			//int32 numPoses;
			int32 numDeformers;
			int32 numMaterials;
			int32 numTextures;
			//int32 numVideos;
			int32 numAnimationStacks;
			int32 numAnimationLayers;
			int32 numAnimationCurveNodes;
			int32 numAnimationCurves;
		};
		int32 counts[10];
	};

	rotation_order defaultRotationOrder;
};

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

	float frameRate;
};

static fbx_definitions readDefinitions(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	fbx_definitions result = {};

	for (const fbx_node& objectTypeNode : fbx_node_iterator{ &node, nodes })
	{
		if (objectTypeNode.name == "ObjectType")
		{
			sized_string type = readString(*objectTypeNode.getFirstProperty(properties));
			const fbx_node* countNode = objectTypeNode.findChild(nodes, "Count");
			int32 count = countNode ? readInt32(*countNode->getFirstProperty(properties)) : 0;

			const fbx_node* propTemplate = objectTypeNode.findChild(nodes, "PropertyTemplate");
			const fbx_node* properties70 = propTemplate ? propTemplate->findChild(nodes, "Properties70") : 0;

			if (type == "GlobalSettings") { result.numGlobalSettings = count; }
			//else if (type == "NodeAttribute") { result.numNodeAttributes = count; }
			//else if (type == "Pose") { result.numPoses = count; }
			else if (type == "Deformer") { result.numDeformers = count; }
			else if (type == "AnimationCurve") { result.numAnimationCurves = count; }
			else if (type == "Material") { result.numMaterials = count; }
			else if (type == "Texture") { result.numTextures = count; }
			//else if (type == "Video") { result.numVideos = count; }
			else if (type == "AnimationStack") { result.numAnimationStacks = count; }
			else if (type == "AnimationLayer") { result.numAnimationLayers = count; }
			else if (type == "AnimationCurveNode") { result.numAnimationCurveNodes = count; }
			else if (type == "Geometry") { result.numMeshes = count; }
			else if (type == "Model")
			{
				result.numModels = count;

				for (const fbx_node& P : fbx_node_iterator{ properties70, nodes })
				{
					ASSERT(P.name == "P");
					auto propIterator = fbx_property_iterator{ &P, properties }.begin();

					sized_string pType = readString(*propIterator++);
					if (pType == "RotationOrder")
					{
						sized_string e = readString(*propIterator++);
						ASSERT(e == "enum");
						result.defaultRotationOrder = (rotation_order)readInt32(*propIterator++);
					}
				}
			}
		}
	}

	return result;
}

static fbx_global_settings readGlobalSettings(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	const fbx_node* properties70 = node.findChild(nodes, "Properties70");

	fbx_global_settings result = {};

	for (const fbx_node& P : fbx_node_iterator{ properties70, nodes })
	{
		ASSERT(P.name == "P");

		auto propIterator = fbx_property_iterator{ &P, properties }.begin();
		sized_string name = readString(*propIterator++);

		auto skipTwo = [&propIterator]()
		{
			sized_string dataType = readString(*propIterator++);
			sized_string longDataType = readString(*propIterator++);
		};

		if (name == "UpAxis") { skipTwo(); result.upAxis = readInt32(*propIterator++); }
		else if (name == "UpAxisSign") { skipTwo(); result.upAxisSign = readInt32(*propIterator++); }
		else if (name == "FrontAxis") { skipTwo(); result.frontAxis = readInt32(*propIterator++); }
		else if (name == "FrontAxisSign") { skipTwo(); result.frontAxisSign = readInt32(*propIterator++); }
		else if (name == "CoordAxis") { skipTwo(); result.coordAxis = readInt32(*propIterator++); }
		else if (name == "CoordAxisSign") { skipTwo(); result.coordAxisSign = readInt32(*propIterator++); }
		else if (name == "OriginalUpAxis") { skipTwo(); result.originalUpAxis = readInt32(*propIterator++); }
		else if (name == "OriginalUpAxisSign") { skipTwo(); result.originalUpAxisSign = readInt32(*propIterator++); }
		else if (name == "CustomFrameRate") { skipTwo(); result.frameRate = (float)readDouble(*propIterator++); }
	}

	return result;
}

enum fbx_object_type
{
	fbx_object_type_unknown,

	fbx_object_type_global,
	fbx_object_type_model,
	fbx_object_type_mesh,
	fbx_object_type_material,
	fbx_object_type_texture,
	fbx_object_type_deformer,
	fbx_object_type_animation_stack,
	fbx_object_type_animation_layer,
	fbx_object_type_animation_curve_node,
	fbx_object_type_animation_curve,
};

static const char* objectTypeNames[] =
{
	"Unknown",
	"Global",
	"Model",
	"Mesh",
	"Material",
	"Texture",
	"Deformer",
	"Animation stack",
	"Animation layer",
	"Animation curve node",
	"Animation curve",
};

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

struct fbx_mesh : fbx_object
{
	std::vector<vec3> positions;
	std::vector<vec2> uvs;
	std::vector<vec3> normals;
	std::vector<vec3> tangents;
	std::vector<uint32> colors;
	std::vector<skinning_weights> skin;
	std::vector<uint8> faceSizes;

	std::vector<offset_count> vertexOffsetCounts;
	std::vector<uint32> originalToNewVertex;

	std::vector<int32> materialIndexPerFace;

	std::vector<submesh_asset> submeshes;
	int64 skeletonID;

	fbx_model* model;
};

struct fbx_material : fbx_object
{
	vec4 emission;
	vec4 albedoTint;
	float roughnessOverride;
	float metallicOverride;
	pbr_material_shader shader;
	float uvScale;
	float translucency;

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

	fbx_model* model;
	int64 skeletonID;
	uint32 parentID = INVALID_JOINT;
	uint32 jointID = INVALID_JOINT;

	mat4 invBindMatrix;
};

struct fbx_animation_stack : fbx_object
{
	std::vector<fbx_animation_layer*> layers;
	int64 duration;
};

struct fbx_animation_layer : fbx_object
{
	std::vector<fbx_animation_curve_node*> curveNodes;
};

struct fbx_animation_curve_node : fbx_object
{
	vec3 d;

	fbx_animation_curve* xCurve;
	fbx_animation_curve* yCurve;
	fbx_animation_curve* zCurve;

	fbx_model* model;
	int32 propertyIndex = -1;
};

struct fbx_animation_curve : fbx_object
{
	float defaultValue;
	uint32 first;
	uint32 count;
	int32 flags;
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

static fbx_mesh readMesh(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties, uint32 flags)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_mesh result = {};
	result.id = id;
	result.name = name;

	const fbx_node* positionsNode = node.findChild(nodes, "Vertices");
	std::vector<double> originalPositionsRaw = readDoubleArray(*positionsNode->getFirstProperty(properties));

	const fbx_node* indicesNode = node.findChild(nodes, "PolygonVertexIndex");
	std::vector<int32> originalIndices = readInt32Array(*indicesNode->getFirstProperty(properties));

	struct vec2d
	{
		double x, y;
	};

	struct vec3d
	{
		double x, y, z;
	};

	struct vec4d
	{
		double x, y, z, w;
	};

	vec3d* originalPositionsPtr = (vec3d*)originalPositionsRaw.data();
	uint32 numOriginalPositions = (uint32)originalPositionsRaw.size() / 3;



	result.positions.reserve(originalIndices.size());
	result.faceSizes.reserve(originalIndices.size());

	result.vertexOffsetCounts.resize(numOriginalPositions, { 0, 0 });
	result.originalToNewVertex.resize(originalIndices.size());

	uint8 faceSize = 0;
	for (int32 index : originalIndices)
	{
		int32 decodedIndex = decodeIndex(index);
		vec3d position = originalPositionsPtr[decodedIndex];
		result.positions.push_back(vec3((float)position.x, (float)position.y, (float)position.z));
		++result.vertexOffsetCounts[decodedIndex].count;

		++faceSize;
		if (index < 0)
		{
			result.faceSizes.push_back(faceSize);
			faceSize = 0;
		}
	}

	uint32 offset = 0;
	for (uint32 i = 0; i < numOriginalPositions; ++i)
	{
		result.vertexOffsetCounts[i].offset = offset;
		offset += result.vertexOffsetCounts[i].count;
		result.vertexOffsetCounts[i].count = 0;
	}

	for (uint32 i = 0; i < (uint32)originalIndices.size(); ++i)
	{
		int32 index = originalIndices[i];
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
				if (flags & mesh_flag_flip_uvs_vertically)
				{
					result.uvs[i].y = 1.f - result.uvs[i].y;
				}
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


	// Tangents.
	if (flags & mesh_flag_load_tangents)
	{
		const fbx_node* tangentsNode = node.findChild(nodes, "LayerElementTangents");
		auto [raw, indices, mapping, reference] = readGeometryData<double>(tangentsNode, nodes, properties, "Tangents", "TangentsIndex");
		if (raw.empty())
		{
			std::tie(raw, indices, mapping, reference) = readGeometryData<double>(tangentsNode, nodes, properties, "Tangent", "TangentIndex");
		}
		ASSERT(raw.size() % 3 == 0);

		if (raw.size())
		{
			result.tangents.resize(raw.size() / 3);
			vec3d* ptr = (vec3d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.tangents.size(); ++i)
			{
				result.tangents[i] = vec3((float)ptr[i].x, (float)ptr[i].y, (float)ptr[i].z);
			}

			result.tangents = mapDataToVertices(result.tangents, indices, mapping, reference, result.vertexOffsetCounts, result.originalToNewVertex,
				(uint32)result.positions.size());
		}
	}


	// Colors.
	if (flags & mesh_flag_load_colors)
	{
		const fbx_node* colorsNode = node.findChild(nodes, "LayerElementColor");
		auto [raw, indices, mapping, reference] = readGeometryData<double>(colorsNode, nodes, properties, "Colors", "ColorIndex");
		ASSERT(raw.size() % 4 == 0);

		if (raw.size())
		{
			result.colors.resize(raw.size() / 4);
			vec4d* ptr = (vec4d*)raw.data();
			for (uint32 i = 0; i < (uint32)result.colors.size(); ++i)
			{
				result.colors[i] = packColor((float)ptr[i].x, (float)ptr[i].y, (float)ptr[i].z, (float)ptr[i].w);
			}

			result.colors = mapDataToVertices(result.colors, indices, mapping, reference, result.vertexOffsetCounts, result.originalToNewVertex,
				(uint32)result.positions.size());
		}
	}



	// Materials.
	{
		const fbx_node* materialsNode = node.findChild(nodes, "LayerElementMaterial");
		auto [materials, indices, mapping, reference] = readGeometryData<int32>(materialsNode, nodes, properties, "Materials", "");

		if (materials.size() > 0)
		{
			if (mapping == mapping_info_all_same)
			{
				int32 materialIndex = !materials.empty() ? materials[0] : 0;
				result.materialIndexPerFace.resize(result.faceSizes.size(), materialIndex);
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
		result.materialIndexPerFace.resize(result.faceSizes.size(), 0);
	}

	return result;
}

static fbx_material readMaterial(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);

	fbx_material result = {};
	result.id = id;
	result.name = name;

	result.albedoTint = vec4(1.f);
	result.emission = vec4(0.f);
	result.uvScale = 1.f;

	for (const fbx_node& child : fbx_node_iterator{ &node, nodes })
	{
		if (child.name == "ShadingModel")
		{
			sized_string shadingModel = readString(*child.getFirstProperty(properties));
			ASSERT(shadingModel == "Phong" || shadingModel == "phong");
		}
		else if (child.name == "MultiLayer")
		{
			int32 multiLayer = readInt32(*child.getFirstProperty(properties));
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
					result.albedoTint = vec4(color, 1.f);
				}
				else if (name == "EmissiveColor")
				{
					result.emission = vec4(color, 1.f);
				}
				else if (name == "ReflectionFactor")
				{
					result.metallicOverride = value;
				}
				else if (name == "ShininessExponent")
				{
					result.roughnessOverride = 1.f - (sqrt(value) * 0.1f);
				}
				else
				{
					//printf("Material property '%.*s' not handled.\n", name.length, name.str);
				}
			}
		}
	}

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

static fbx_deformer readDeformer(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
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

	return result;
}

static fbx_animation_stack readAnimationStack(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);
	fbx_animation_stack result = {};
	result.id = id;
	result.name = name;

	for (const fbx_node& P : fbx_node_iterator{ node.findChild(nodes, "Properties70"), nodes })
	{
		ASSERT(P.name == "P");

		sized_string type = readString(*P.getFirstProperty(properties));
		if (type == "LocalStop")
		{
			for (const fbx_property& prop : fbx_property_iterator{ &P, properties })
			{
				if (prop.type == fbx_property_type_int64)
				{
					result.duration = readInt64(prop);
				}
			}
		}
	}

	if (result.duration == 0)
	{
		printf("Animation stack has zero duration!\n");
	}

	return result;
}

static fbx_animation_layer readAnimationLayer(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);
	fbx_animation_layer result = {};
	result.id = id;
	result.name = name;

	return result;
}

static fbx_animation_curve_node readAnimationCurveNode(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties)
{
	auto [id, name] = readObjectIDAndName(node, properties);
	fbx_animation_curve_node result = {};
	result.id = id;
	result.name = name;

	for (const fbx_node& P : fbx_node_iterator{ node.findChild(nodes, "Properties70"), nodes })
	{
		ASSERT(P.name == "P");

		auto propIterator = fbx_property_iterator{ &P, properties }.begin();
		sized_string type = readString(*propIterator++);
		sized_string numberStr = readString(*propIterator++);
		sized_string aStr = readString(*propIterator++);

		ASSERT(numberStr == "Number");
		ASSERT(aStr == "A");

		float value = (float)readDouble(*propIterator++);
		if (type == "d|X") { result.d.x = value; }
		else if (type == "d|Y") { result.d.y = value; }
		else if (type == "d|Z") { result.d.z = value; }
	}

	return result;
}

static fbx_animation_curve readAnimationCurve(const fbx_node& node, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties,
	std::vector<int64>& times, std::vector<float>& values)
{
	auto [id, name] = readObjectIDAndName(node, properties);
	fbx_animation_curve result = {};
	result.id = id;
	result.name = name;

	uint32 first = (uint32)times.size();

	for (const fbx_node& child : fbx_node_iterator{ &node, nodes })
	{
		if (child.name == "Default")
		{
			result.defaultValue = (float)readDouble(*child.getFirstProperty(properties));
		}
		else if (child.name == "KeyTime")
		{
			const fbx_property& prop = *child.getFirstProperty(properties);
			ASSERT(prop.type == fbx_property_type_int64);

			uint32 count = prop.numElements;
			times.resize(times.size() + count);
			readArray(prop, (uint8*)(times.data() + first));
		}
		else if (child.name == "KeyValueFloat")
		{
			const fbx_property& prop = *child.getFirstProperty(properties);
			ASSERT(prop.type == fbx_property_type_float);

			uint32 count = prop.numElements;
			values.resize(values.size() + count);
			readArray(prop, (uint8*)(values.data() + first));
		}
		else if (child.name == "KeyAttrFlags")
		{
			result.flags = readInt32(*child.getFirstProperty(properties));
		}
	}

	ASSERT(times.size() == values.size());

	uint32 count = (uint32)times.size() - first;
	result.first = first;
	result.count = count;

	return result;
}









struct fbx_animation_joint
{
	union
	{
		struct
		{
			fbx_animation_curve_node* translation;
			fbx_animation_curve_node* rotation;
			fbx_animation_curve_node* scaling;
		};
		fbx_animation_curve_node* curveNodes[3];
	};
};

struct fbx_animation
{
	std::string name;
	std::unordered_map<int64, fbx_animation_joint> joints;
	int64 duration;
};

struct fbx_skeleton
{
	std::vector<fbx_deformer*> joints;
};

struct fbx_object_lut
{
	using fbx_object_index = std::pair<fbx_object_type, uint32>;

	std::unordered_map<int64, fbx_object_index> idToObject;

	std::vector<fbx_model> models;
	std::vector<fbx_mesh> meshes;
	std::vector<fbx_material> materials;
	std::vector<fbx_texture> textures;
	std::vector<fbx_deformer> deformers;

	std::vector<fbx_animation_stack> animationStacks;
	std::vector<fbx_animation_layer> animationLayers;
	std::vector<fbx_animation_curve_node> animationCurveNodes;
	std::vector<fbx_animation_curve> animationCurves;

	std::unordered_map<int64, fbx_skeleton> skeletons;
	std::vector<fbx_animation> animations;


	void reserve(const fbx_definitions& definitions)
	{
		models.reserve(definitions.numModels);
		meshes.reserve(definitions.numMeshes);
		materials.reserve(definitions.numMaterials);
		textures.reserve(definitions.numTextures);
		deformers.reserve(definitions.numDeformers);
		animationStacks.reserve(definitions.numAnimationStacks);
		animationLayers.reserve(definitions.numAnimationLayers);
		animationCurveNodes.reserve(definitions.numAnimationCurveNodes);
		animationCurves.reserve(definitions.numAnimationCurves);

		int32 totalCount = 0;
		for (uint32 i = 0; i < arraysize(definitions.counts); ++i)
		{
			totalCount += definitions.counts[i];
		}
		idToObject.reserve(totalCount);
	}

	template <typename T>
	void push(std::vector<T>& vec, T&& t, fbx_object_type type)
	{
		uint32 index = (uint32)vec.size();
		int64 id = t.id;
		vec.push_back(std::move(t));
		idToObject[id] = { type, index };
	}

	void push(fbx_model&& t) { push(models, std::move(t), fbx_object_type_model); }
	void push(fbx_mesh&& t) { push(meshes, std::move(t), fbx_object_type_mesh); }
	void push(fbx_material&& t) { push(materials, std::move(t), fbx_object_type_material); }
	void push(fbx_texture&& t) { push(textures, std::move(t), fbx_object_type_texture); }
	void push(fbx_deformer&& t) { push(deformers, std::move(t), fbx_object_type_deformer); }
	void push(fbx_animation_stack&& t) { push(animationStacks, std::move(t), fbx_object_type_animation_stack); }
	void push(fbx_animation_layer&& t) { push(animationLayers, std::move(t), fbx_object_type_animation_layer); }
	void push(fbx_animation_curve_node&& t) { push(animationCurveNodes, std::move(t), fbx_object_type_animation_curve_node); }
	void push(fbx_animation_curve&& t) { push(animationCurves, std::move(t), fbx_object_type_animation_curve); }

	fbx_object_index find(int64 id) 
	{ 
		auto it = idToObject.find(id);
		if (it == idToObject.end())
		{
			return { fbx_object_type_unknown, 0 };
		}
		return it->second; 
	}
};



static void resolveConnections(const fbx_node* connectionsNode, const std::vector<fbx_node>& nodes, const std::vector<fbx_property>& properties,
	fbx_object_lut& lut)
{
	for (const fbx_node& node : fbx_node_iterator{ connectionsNode, nodes })
	{
		ASSERT(node.name == "C");

		auto propIterator = fbx_property_iterator{ &node, properties }.begin();
		sized_string type = readString(*propIterator++);
		int64 idA = readInt64(*propIterator++);
		int64 idB = readInt64(*propIterator++);

		auto [typeA, indexA] = lut.find(idA);
		auto [typeB, indexB] = lut.find(idB);

		if (type == "OO")
		{
			// Object-object connection.
			if (typeA == fbx_object_type_global || typeB == fbx_object_type_global)
			{
				// Do nothing.
			}
			else if (typeA == fbx_object_type_model && typeB == fbx_object_type_model)
			{
				// Model-model connection -> A is child of B.
				fbx_model& a = lut.models[indexA];
				fbx_model& b = lut.models[indexB];
				b.children.push_back(&a);
				a.parent = &b;
			}
			else if (typeA == fbx_object_type_material && typeB == fbx_object_type_model)
			{
				// Material-model connection -> A is one of B's materials.
				fbx_material& a = lut.materials[indexA];
				fbx_model& b = lut.models[indexB];
				b.materials.push_back(&a);
			}
			else if (typeA == fbx_object_type_model && typeB == fbx_object_type_deformer)
			{
				// Model-deformer connection -> B influences A.
				fbx_model& a = lut.models[indexA];
				fbx_deformer& b = lut.deformers[indexB];
				b.model = &a;
				a.deformer = &b;
			}
			else if (typeA == fbx_object_type_mesh && typeB == fbx_object_type_model)
			{
				// Mesh-model connection -> B has mesh A.
				fbx_mesh& a = lut.meshes[indexA];
				fbx_model& b = lut.models[indexB];
				b.meshes.push_back(&a);
				a.model = &b;
			}
			else if (typeA == fbx_object_type_deformer && typeB == fbx_object_type_mesh)
			{
				// Deformer-mesh connection -> A deforms B.
				fbx_deformer& a = lut.deformers[indexA];
				fbx_mesh& b = lut.meshes[indexB];
				ASSERT(b.skeletonID == 0);
				b.skeletonID = idA;
			}
			else if (typeA == fbx_object_type_deformer && typeB == fbx_object_type_deformer)
			{
				// Deformer-deformer connection -> A is part of B's skeleton, not necessarily its direct child!.
				fbx_deformer& a = lut.deformers[indexA];
				fbx_deformer& b = lut.deformers[indexB];

				fbx_skeleton& skel = lut.skeletons[idB];
				a.jointID = (uint32)skel.joints.size();
				a.skeletonID = idB;
				skel.joints.push_back(&a);
			}
			else if (typeA == fbx_object_type_animation_layer && typeB == fbx_object_type_animation_stack)
			{
				// Layer-stack connection -> A is one of B's layers.
				fbx_animation_layer& a = lut.animationLayers[indexA];
				fbx_animation_stack& b = lut.animationStacks[indexB];
				b.layers.push_back(&a);
			}
			else if (typeA == fbx_object_type_animation_curve_node && typeB == fbx_object_type_animation_layer)
			{
				// Curvenode-layer connection -> A influences one of B's joints.
				fbx_animation_curve_node& a = lut.animationCurveNodes[indexA];
				fbx_animation_layer& b = lut.animationLayers[indexB];
				b.curveNodes.push_back(&a);
			}
			else
			{
				//printf("Unhandled OO connection: %s - %s.\n", objectTypeNames[typeA], objectTypeNames[typeB]);
			}
		}
		else if (type == "OP")
		{
			// Object-property connection.
			sized_string slot = readString(*propIterator++);

			if (typeA == fbx_object_type_texture && typeB == fbx_object_type_material)
			{
				// Texture-material connection -> B uses A.
				fbx_texture& a = lut.textures[indexA];
				fbx_material& b = lut.materials[indexB];

				if (slot == "DiffuseColor") { b.albedoTexture = &a; }
				else if (slot == "NormalMap") { b.normalTexture = &a; }
				else if (slot == "ShininessExponent") { b.roughnessTexture = &a; }
				else if (slot == "ReflectionFactor") { b.metallicTexture = &a; }
				else { printf("Unknown texture slot '%.*s'.\n", slot.length, slot.str); }
			}
			else if (typeA == fbx_object_type_animation_curve_node && typeB == fbx_object_type_model)
			{
				// Curvenode-model connection -> A influences properties of B (translation, rotation, or scale).
				fbx_animation_curve_node& a = lut.animationCurveNodes[indexA];
				fbx_model& b = lut.models[indexB];

				int32 index = -1;
				if (slot == "Lcl Translation") { index = 0; }
				else if (slot == "Lcl Rotation") { index = 1; }
				else if (slot == "Lcl Scaling") { index = 2; }
				else { printf("Unknown curve node slot '%.*s'.\n", slot.length, slot.str); }

				a.model = &b;
				a.propertyIndex = index;
			}
			else if (typeA == fbx_object_type_animation_curve && typeB == fbx_object_type_animation_curve_node)
			{
				// Curve-curvenode connection -> A describes changes in B (x, y, or z).
				fbx_animation_curve& a = lut.animationCurves[indexA];
				fbx_animation_curve_node& b = lut.animationCurveNodes[indexB];

				if (slot == "d|X") { b.xCurve = &a; }
				else if (slot == "d|Y") { b.yCurve = &a; }
				else if (slot == "d|Z") { b.zCurve = &a; }
				else { printf("Unknown curve slot '%.*s'.\n", slot.length, slot.str); }
			}
			else
			{
				//printf("Unhandled OP connection: %s - %s.\n", objectTypeNames[typeA], objectTypeNames[typeB]);
			}
		}
	}

	for (fbx_deformer& deformer : lut.deformers)
	{
		if (fbx_model* model = deformer.model)
		{
			if (fbx_model* parentModel = model->parent)
			{
				if (fbx_deformer* parentDeformer = parentModel->deformer)
				{
					deformer.parentID = parentDeformer->jointID;
				}
			}
		}
	}


	for (fbx_animation_stack& stack : lut.animationStacks)
	{
		fbx_animation& anim = lut.animations.emplace_back();
		anim.duration = stack.duration;
		anim.name = nameToString(stack.name);

		ASSERT(stack.layers.size() == 1);
		for (fbx_animation_layer* layer : stack.layers)
		{
			for (fbx_animation_curve_node* curveNode : layer->curveNodes)
			{
				if (fbx_model* model = curveNode->model)
				{
					fbx_animation_joint& joint = anim.joints[model->id];
					joint.curveNodes[curveNode->propertyIndex] = curveNode;
				}
			}
		}
	}
}

static void finishMesh(fbx_mesh& mesh, uint32 flags, std::unordered_map<int64, fbx_skeleton>& skeletons)
{
	// Assign materials and skinning weights, remove duplicate vertices and triangulate.

	if (mesh.skeletonID && flags & mesh_flag_load_skin)
	{
		CPU_PRINT_PROFILE_BLOCK("Assigning skinning weights");

		mesh.skin.resize(mesh.positions.size(), {});
		fbx_skeleton& skeleton = skeletons[mesh.skeletonID];

		for (uint32 jointID = 0; jointID < (uint32)skeleton.joints.size(); ++jointID)
		{
			fbx_deformer* joint = skeleton.joints[jointID];

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
							//printf("Warning: Vertex is influences by more than 4 joints. Ditching joint with weight %f.\n", skin.skinWeights[3] / 255.f);
						}
						for (int32 k = 3; k > slot; --k)
						{
							skin.skinIndices[k] = skin.skinIndices[k - 1];
							skin.skinWeights[k] = skin.skinWeights[k - 1];
						}
						skin.skinIndices[slot] = (uint8)jointID;
						skin.skinWeights[slot] = weight;
					}
					else
					{
						//printf("Warning: Vertex is influences by more than 4 joints. Ditching joint with weight %f.\n", weights[i]);
					}
				}
			}
		}
	}



	std::unordered_map<int32, per_material> materialToMesh;

	{
		CPU_PRINT_PROFILE_BLOCK("Triangulating & duplicate vertex removal");
		for (int32 faceIndex = 0, firstIndex = 0; faceIndex < (int32)mesh.faceSizes.size(); ++faceIndex)
		{
			int32 faceSize = mesh.faceSizes[faceIndex];
			int32 material = mesh.materialIndexPerFace[faceIndex];

			per_material& perMat = materialToMesh[material];
			perMat.sub.materialIndex = material;

			perMat.addTriangles(mesh.positions, mesh.uvs, mesh.normals, mesh.tangents, mesh.colors, mesh.skin, firstIndex, faceSize, mesh.submeshes);

			firstIndex += faceSize;
		}
	}

	for (auto [i, perMat] : materialToMesh)
	{
		perMat.flush(mesh.submeshes);
	}

	generateNormalsAndTangents(mesh.submeshes, flags);
}


static float sampleAnimationCurve(fbx_animation_curve* curve, int64 time, const std::vector<int64>& animationTimes, const std::vector<float>& animationValues)
{
	uint32 first = curve->first;
	uint32 count = curve->count;
	if (count == 0)
	{
		return curve->defaultValue;
	}
	else if (time >= animationTimes[first + count - 1])
	{
		return animationValues[first + count - 1];
	}
	else if (time <= 0)
	{
		return animationValues[first];
	}

	uint32 index = first;
	while (index < first + count - 1)
	{
		if (time < animationTimes[index + 1])
		{
			break;
		}

		++index;
	}

	uint32 i0 = index;
	uint32 i1 = index + 1;

	float t = (float)(time - animationTimes[i0]) / (float)(animationTimes[i1] - animationTimes[i0]);
	return lerp(animationValues[i0], animationValues[i1], t);
};

static offset_count transferAnimationCurve(fbx_animation_curve_node* curveNode, std::vector<vec3>& outValues, std::vector<float>& outTimes, int64 animationDuration,
	const std::vector<int64>& animationTimes, const std::vector<float>& animationValues)
{
	fbx_animation_curve* x = curveNode->xCurve;
	fbx_animation_curve* y = curveNode->yCurve;
	fbx_animation_curve* z = curveNode->zCurve;

	uint32 count = max(max(x->count, y->count), max(z->count, 2u));
	offset_count result = { (uint32)outValues.size(), count };

	int64 step = animationDuration / (count - 1);
	int64 time = 0;

	for (uint32 i = 0; i < count; ++i)
	{
		vec3 value;
		value.x = sampleAnimationCurve(x, time, animationTimes, animationValues);
		value.y = sampleAnimationCurve(y, time, animationTimes, animationValues);
		value.z = sampleAnimationCurve(z, time, animationTimes, animationValues);

		outValues.push_back(value);
		outTimes.push_back(convertTime(time));

		time += step;
	}

	return result;
};

static quat convertRotation(vec3 rotation, rotation_order order)
{
	quat x(vec3(1.f, 0.f, 0.f), deg2rad(rotation.x));
	quat y(vec3(0.f, 1.f, 0.f), deg2rad(rotation.y));
	quat z(vec3(0.f, 0.f, 1.f), deg2rad(rotation.z));

	switch (order)
	{
		case rotation_order_xyz: return z * y * x;
		case rotation_order_xzy: return y * z * x;
		case rotation_order_yzx: return x * z * y;
		case rotation_order_yxz: return z * x * y;
		case rotation_order_zxy: return y * x * z;
		case rotation_order_zyx: return x * y * z;
		case rotation_order_spheric_xyz: ASSERT(false); return quat::identity;
		default: ASSERT(false); return quat::identity;
	}
}

static offset_count transferAnimationCurve(fbx_animation_curve_node* curveNode, std::vector<quat>& outValues, std::vector<float>& outTimes, int64 animationDuration,
	const std::vector<int64>& animationTimes, const std::vector<float>& animationValues, rotation_order rotationOrder)
{
	fbx_animation_curve* x = curveNode->xCurve;
	fbx_animation_curve* y = curveNode->yCurve;
	fbx_animation_curve* z = curveNode->zCurve;

	uint32 count = max(max(x->count, y->count), max(z->count, 2u));
	offset_count result = { (uint32)outValues.size(), count };

	int64 step = animationDuration / (count - 1);
	int64 time = 0;

	for (uint32 i = 0; i < count; ++i)
	{
		vec3 value;
		value.x = sampleAnimationCurve(x, time, animationTimes, animationValues);
		value.y = sampleAnimationCurve(y, time, animationTimes, animationValues);
		value.z = sampleAnimationCurve(z, time, animationTimes, animationValues);

		outValues.push_back(convertRotation(value, rotationOrder));
		outTimes.push_back(convertTime(time));

		time += step;
	}

	return result;
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

model_asset loadFBX(const fs::path& path, uint32 flags)
{
	std::string pathStr = path.string();
	const char* s = pathStr.c_str();
	entire_file file = loadFile(path);
	if (file.size < sizeof(fbx_header))
	{
		printf("File '%s' is smaller than FBX header.\n", s);
		freeFile(file);
		return {};
	}

	fbx_header* header = file.consume<fbx_header>();
	if ((strcmp(header->magic, "Kaydara FBX Binary  ") != 0) || header->unknown[0] != 0x1A || header->unknown[1] != 0x00)
	{
		printf("Header of file '%s' does not match FBX spec.\n", s);
		freeFile(file);
		return {};
	}

	uint32 version = header->version;

	std::vector<fbx_node> nodes;
	fbx_node node = {};
	node.parent = -1;
	node.level = -1;
	node.next = -1;
	node.firstChild = -1;
	node.lastChild = -1;
	node.numChildren = 0;
	nodes.push_back(node);

	std::vector<fbx_property> properties;

	{
		CPU_PRINT_PROFILE_BLOCK("Parse FBX nodes");
		parseNodes(version, file, nodes, properties, 0, 0);
	}

#if 0
	{
		YAML::Node out;
		writeFBXContentToYAML(nodes, properties, nodes[0], out);
		std::ofstream fout("fbx.yaml");
		fout << out;
		return {};
	}
#endif

	fbx_definitions definitions = readDefinitions(*findNode(nodes, { "Definitions" }), nodes, properties);
	fbx_global_settings globalSettings = readGlobalSettings(*findNode(nodes, { "GlobalSettings" }), nodes, properties);

	
	fbx_object_lut objectLUT;
	objectLUT.reserve(definitions);

	std::vector<int64> animationTimes;
	std::vector<float> animationValues;


	{
		CPU_PRINT_PROFILE_BLOCK("Reading FBX objects");

		const fbx_node* objectsNode = findNode(nodes, { "Objects" });
		objectLUT.idToObject.reserve(objectsNode->numChildren + 1);
		objectLUT.idToObject[0] = { fbx_object_type_global, 0 };

		for (const fbx_node& objectNode : fbx_node_iterator{ objectsNode, nodes })
		{
			if (objectNode.name == "Model")
			{
				objectLUT.push(readModel(objectNode, nodes, properties));
			}
			if (objectNode.name == "Geometry")
			{
				objectLUT.push(readMesh(objectNode, nodes, properties, flags));
			}
			else if (objectNode.name == "Material")
			{
				objectLUT.push(readMaterial(objectNode, nodes, properties));
			}
			else if (objectNode.name == "Texture")
			{
				objectLUT.push(readTexture(objectNode, nodes, properties));
			}
			else if (objectNode.name == "Deformer")
			{
				objectLUT.push(readDeformer(objectNode, nodes, properties));
			}
			else if (objectNode.name == "AnimationStack")
			{
				objectLUT.push(readAnimationStack(objectNode, nodes, properties));
			}
			else if (objectNode.name == "AnimationLayer")
			{
				objectLUT.push(readAnimationLayer(objectNode, nodes, properties));
			}
			else if (objectNode.name == "AnimationCurveNode")
			{
				objectLUT.push(readAnimationCurveNode(objectNode, nodes, properties));
			}
			else if (objectNode.name == "AnimationCurve")
			{
				objectLUT.push(readAnimationCurve(objectNode, nodes, properties, animationTimes, animationValues));
			}
		}
	}

	{
		CPU_PRINT_PROFILE_BLOCK("Resolving FBX connections");
		resolveConnections(findNode(nodes, { "Connections" }), nodes, properties, objectLUT);
	}

	{
		CPU_PRINT_PROFILE_BLOCK("Finishing FBX meshes");
		for (fbx_mesh& mesh : objectLUT.meshes)
		{
			finishMesh(mesh, flags, objectLUT.skeletons);
		}
	}


	std::unordered_map<fbx_material*, int32> materialToGlobalIndex;
	int32 globalMaterialIndex = 0;
	for (fbx_material& mat : objectLUT.materials)
	{
		materialToGlobalIndex[&mat] = globalMaterialIndex++;
	}

	for (fbx_mesh& mesh : objectLUT.meshes)
	{
		fbx_model* model = mesh.model;
		for (submesh_asset& sub : mesh.submeshes)
		{
			fbx_material* mat = model->materials[sub.materialIndex];
			sub.materialIndex = materialToGlobalIndex[mat];
		}
	}

	model_asset result;

	result.materials.reserve(objectLUT.materials.size());
	for (fbx_material& material : objectLUT.materials)
	{
		pbr_material_desc out;
		out.albedo = material.albedoTexture ? relativeFilepath(material.albedoTexture->relativeFilename, path) : std::string();
		out.normal = material.normalTexture ? relativeFilepath(material.normalTexture->relativeFilename, path) : std::string();
		out.roughness = material.roughnessTexture ? relativeFilepath(material.roughnessTexture->relativeFilename, path) : std::string();
		out.metallic = material.metallicTexture ? relativeFilepath(material.metallicTexture->relativeFilename, path) : std::string();
		out.albedoTint = material.albedoTint;
		out.emission = material.emission;
		out.metallicOverride = material.metallicOverride;
		out.roughnessOverride = material.roughnessOverride;
		out.shader = material.shader;
		out.translucency = material.translucency;
		out.uvScale = material.uvScale;

		result.materials.push_back(std::move(out));
	}
	
	result.meshes.reserve(objectLUT.meshes.size());
	for (fbx_mesh& mesh : objectLUT.meshes)
	{
		result.meshes.push_back(mesh_asset{ nameToString(mesh.model->name), std::move(mesh.submeshes), -1 });
	}

	for (auto& [id, skeleton] : objectLUT.skeletons)
	{
		uint32 numJoints = (uint32)skeleton.joints.size();

		skeleton_asset out;
		out.joints.reserve(numJoints);
		for (uint32 i = 0; i < numJoints; ++i)
		{
			fbx_deformer* joint = skeleton.joints[i];
			std::string name = nameToString(joint->model->name);

			out.nameToJointID[name] = i;
			out.joints.push_back({ std::move(name), limb_type_unknown, false, joint->invBindMatrix, invert(joint->invBindMatrix), joint->parentID });
		}

		ASSERT(out.joints.size() == out.nameToJointID.size());


		for (uint32 i = 0; i < (uint32)objectLUT.meshes.size(); ++i)
		{
			if (objectLUT.meshes[i].skeletonID == id)
			{
				result.meshes[i].skeletonIndex = (uint32)result.skeletons.size();
			}
		}

		result.skeletons.push_back(std::move(out));
	}

	for (fbx_animation& animation : objectLUT.animations)
	{
		uint32 numJoints = (uint32)animation.joints.size();

		if (numJoints)
		{
			animation_asset out;
			out.duration = convertTime(animation.duration);
			out.joints.reserve(animation.joints.size());
			out.name = animation.name;

			for (auto [id, j] : animation.joints)
			{
				auto [modelType, modelIndex] = objectLUT.find(id);
				ASSERT(modelType == fbx_object_type_model);

				fbx_model& model = objectLUT.models[modelIndex];
				std::string name = nameToString(model.name);

				animation_joint& joint = out.joints[name];
				joint.isAnimated = true;

				offset_count position = transferAnimationCurve(j.curveNodes[0], out.positionKeyframes, out.positionTimestamps, animation.duration, 
					animationTimes, animationValues);
				joint.firstPositionKeyframe = position.offset;
				joint.numPositionKeyframes = position.count;

				offset_count rotation = transferAnimationCurve(j.curveNodes[1], out.rotationKeyframes, out.rotationTimestamps, animation.duration,
					animationTimes, animationValues, definitions.defaultRotationOrder);
				joint.firstRotationKeyframe = rotation.offset;
				joint.numRotationKeyframes = rotation.count;

				offset_count scale = transferAnimationCurve(j.curveNodes[2], out.scaleKeyframes, out.scaleTimestamps, animation.duration,
					animationTimes, animationValues);
				joint.firstScaleKeyframe = scale.offset;
				joint.numScaleKeyframes = scale.count;
			}

			result.animations.push_back(std::move(out));
		}
	}

	freeFile(file);









#if 0
	for (uint32 i = 0; i < (uint32)result.meshes.size(); ++i)
	{
		const mesh_asset& mesh = result.meshes[i];
		std::string indexedName = "Mesh_" + std::to_string(i);

		for (uint32 j = 0; j < (uint32)mesh.submeshes.size(); ++j)
		{
			const submesh_asset& sub = mesh.submeshes[j];

			vec3 diffuseColor = !result.materials.empty() ? result.materials[sub.materialIndex].diffuseColor : vec3(1.f, 1.f, 1.f);

			std::string indexedName2 = indexedName + "_" + std::to_string(j) + ".ply";

			testDumpToPLY(indexedName2, sub.positions, sub.uvs, sub.normals, sub.triangles,
				(uint8)(diffuseColor.x * 255), (uint8)(diffuseColor.y * 255), (uint8)(diffuseColor.z * 255));
		}
	}
#endif






	return result;
}





