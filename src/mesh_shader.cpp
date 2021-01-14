#include "pch.h"
#include "dx_pipeline.h"
#include "dx_renderer.h"

static dx_pipeline cubePipeline;
static dx_pipeline meshPipeline;


struct subset
{
	uint32 offset;
	uint32 count;
};

struct mesh_shader_submesh_info
{
	uint32 firstVertex;
	uint32 numVertices;

	uint32 firstMeshlet;
	uint32 numMeshlets;

	uint32 firstUniqueVertexIndex;
	uint32 numUniqueVertexIndices;

	uint32 firstPackedTriangle;
	uint32 numPackedTriangles;

	uint32 firstMeshletSubset;
	uint32 numMeshletSubsets;
};

struct mesh_shader_mesh
{
	std::vector<mesh_shader_submesh_info> submeshes;
	std::vector<subset> subsets;

	ref<dx_buffer> vertices;
	ref<dx_buffer> meshlets;
	ref<dx_buffer> uniqueVertexIndices;
	ref<dx_buffer> primitiveIndices;
};

static ref<mesh_shader_mesh> loadMeshShaderMeshFromFile(const char* filename);


struct mesh_shader_cube_material : material_base
{
	static void setupPipeline(dx_command_list* cl, const common_material_info& info)
	{
		cl->setPipelineState(*cubePipeline.pipeline);
		cl->setGraphicsRootSignature(*cubePipeline.rootSignature);
	}

	void prepareForRendering(dx_command_list* cl)
	{
	}
};

struct mesh_shader_mesh_material : material_base
{
	ref<mesh_shader_mesh> mesh;

	static void setupPipeline(dx_command_list* cl, const common_material_info& info)
	{
		cl->setPipelineState(*meshPipeline.pipeline);
		cl->setGraphicsRootSignature(*meshPipeline.rootSignature);
	}

	void prepareForRendering(dx_command_list* cl)
	{
		cl->setRootGraphicsSRV(1, mesh->vertices);
		cl->setRootGraphicsSRV(2, mesh->meshlets);
		cl->setRootGraphicsSRV(3, mesh->uniqueVertexIndices);
		cl->setRootGraphicsSRV(4, mesh->primitiveIndices);
	}
};

//static ref<mesh_shader_cube_material> cubeMaterial;
static ref<mesh_shader_mesh_material> meshMaterial;

void initializeMeshShader()
{
	struct pipeline_state_stream : dx_pipeline_stream_base
	{
		// Will be set by reloader.
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_MS ms;
		CD3DX12_PIPELINE_STATE_STREAM_PS ps;

		// Initialized here.
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT inputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;

		void setRootSignature(dx_root_signature rs) override
		{
			rootSignature = rs.rootSignature.Get();
		}

		void setMeshShader(dx_blob blob) override
		{
			ms = CD3DX12_SHADER_BYTECODE(blob.Get());
		}

		void setPixelShader(dx_blob blob) override
		{
			ps = CD3DX12_SHADER_BYTECODE(blob.Get());
		}
	};

	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	renderTargetFormat.NumRenderTargets = 1;
	renderTargetFormat.RTFormats[0] = dx_renderer::hdrFormat[0];

	auto defaultRasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//defaultRasterizerDesc.FrontCounterClockwise = TRUE; // Righthanded coordinate system.

	pipeline_state_stream stream;
	stream.inputLayout = { nullptr, 0 };
	stream.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	stream.dsvFormat = dx_renderer::hdrDepthStencilFormat;
	stream.rtvFormats = renderTargetFormat;
	stream.rasterizer = defaultRasterizerDesc;

	graphics_pipeline_files files = {};
	files.ms = "mesh_shader_v1_ms";
	files.ps = "mesh_shader_ps";

	//cubePipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);

	files.ms = "mesh_shader_v2_ms";
	meshPipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);

	//cubeMaterial = make_ref<mesh_shader_cube_material>();
	meshMaterial = make_ref<mesh_shader_mesh_material>();
	meshMaterial->mesh = loadMeshShaderMeshFromFile("assets/meshes/Dragon_LOD0.bin");
}

void testRenderMeshShader(overlay_render_pass* overlayRenderPass)
{
#if 0
	overlayRenderPass->renderObjectWithMeshShader(1, 1, 1,
		cubeMaterial,
		createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 1.f)
	);
#else
	auto& sm = meshMaterial->mesh->submeshes[0];
	overlayRenderPass->renderObjectWithMeshShader(sm.numMeshlets, 1, 1,
		meshMaterial,
		createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 1.f)
	);
#endif
}

// Limits:
// Num threads in thread group <= 128
// Output vertices <= 256
// Output primitives <= 256
// Total output vertex data size <= 32k (for everything: positions, uvs, normals, indices, ...)








#include <fstream>

enum attribute_type
{
	attribute_type_position,
	attribute_type_normal,
	attribute_type_texCoord,
	attribute_type_tangent,
	attribute_type_bitangent,
	attribute_type_count,
};

struct mesh_header
{
	uint32 indices;
	uint32 indexSubsets;
	uint32 attributes[attribute_type_count];

	uint32 meshlets;
	uint32 meshletSubsets;
	uint32 uniqueVertexIndices;
	uint32 primitiveIndices;
	uint32 cullData;
};

struct buffer_view
{
	uint32 offset;
	uint32 size;
};

struct buffer_accessor
{
	uint32 bufferView;
	uint32 offset;
	uint32 size;
	uint32 stride;
	uint32 count;
};

struct file_header
{
	uint32 prolog;
	uint32 version;

	uint32 meshCount;
	uint32 accessorCount;
	uint32 bufferViewCount;
	uint32 bufferSize;
};

enum FileVersion
{
	FILE_VERSION_INITIAL = 0,
	CURRENT_FILE_VERSION = FILE_VERSION_INITIAL
};

// Meshlet stuff.

struct meshlet_info
{
	uint32 numVertices;
	uint32 firstVertex;
	uint32 numPrimitives;
	uint32 firstPrimitive;
};

struct packed_triangle
{
	uint32 i0 : 10;
	uint32 i1 : 10;
	uint32 i2 : 10;
};

struct mesh_vertex
{
	vec3 position;
	vec3 normal;
};

static ref<mesh_shader_mesh> loadMeshShaderMeshFromFile(const char* filename)
{
	std::ifstream stream(filename, std::ios::binary);
	if (!stream.is_open())
	{
		return 0;
	}

	std::vector<mesh_header> meshHeaders;
	std::vector<buffer_view> bufferViews;
	std::vector<buffer_accessor> accessors;

	file_header header;
	stream.read((char*)&header, sizeof(header));

	const uint32 prolog = 'MSHL';
	if (header.prolog != prolog)
	{
		return 0; // Incorrect file format.
	}

	if (header.version != CURRENT_FILE_VERSION)
	{
		return 0; // Version mismatch between export and import serialization code.
	}

	// Read mesh metdata.
	meshHeaders.resize(header.meshCount);
	stream.read((char*)meshHeaders.data(), meshHeaders.size() * sizeof(meshHeaders[0]));

	accessors.resize(header.accessorCount);
	stream.read((char*)accessors.data(), accessors.size() * sizeof(accessors[0]));

	bufferViews.resize(header.bufferViewCount);
	stream.read((char*)bufferViews.data(), bufferViews.size() * sizeof(bufferViews[0]));

	std::vector<uint8> m_buffer;
	m_buffer.resize(header.bufferSize);
	stream.read((char*)m_buffer.data(), header.bufferSize);

	char eofbyte;
	stream.read(&eofbyte, 1); // Read last byte to hit the eof bit.
	assert(stream.eof()); // There's a problem if we didn't completely consume the file contents..

	stream.close();

	std::vector<mesh_shader_submesh_info> submeshes(meshHeaders.size());

	//std::vector<uint32> indices;
	//std::vector<subset> indexSubsets;
	std::vector<mesh_vertex> vertices;
	std::vector<meshlet_info> meshlets;
	std::vector<uint32> uniqueVertexIndices;
	std::vector<packed_triangle> primitiveIndices;
	std::vector<subset> meshletSubsets;

	for (uint32_t i = 0; i < (uint32)meshHeaders.size(); ++i)
	{
		mesh_header& meshView = meshHeaders[i];
		mesh_shader_submesh_info& sm = submeshes[i];

#if 0
		// Index data.
		{
			buffer_accessor& accessor = accessors[meshView.indices];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			assert(accessor.size == sizeof(uint32));
			assert(accessor.count * accessor.size == bufferView.size);

			uint32* start = (uint32*)(m_buffer.data() + bufferView.offset);

			sm.firstIndex = (uint32)indices.size();
			sm.numIndices = accessor.count;

			indices.insert(indices.end(), start, start + sm.numIndices);
		}
#endif

#if 0
		// Index Subset data.
		{
			buffer_accessor& accessor = accessors[meshView.indexSubsets];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			assert(accessor.count * accessor.size == bufferView.size);

			subset* start = (subset*)(m_buffer.data() + bufferView.offset);

			sm.firstIndexSubset = (uint32)indexSubsets.size();
			sm.numIndexSubsets = accessor.count;

			indexSubsets.insert(indexSubsets.end(), start, start + sm.numIndexSubsets);
		}
#endif

		// Vertex data & layout metadata

		bool first = true;

		for (uint32 j = 0; j < attribute_type_count; ++j)
		{
			if (meshView.attributes[j] == -1)
			{
				continue;
			}

			buffer_accessor& accessor = accessors[meshView.attributes[j]];

			buffer_view& bufferView = bufferViews[accessor.bufferView];

			if (first)
			{
				sm.firstVertex = (uint32)vertices.size();
				sm.numVertices = accessor.count;
				vertices.resize(vertices.size() + sm.numVertices);
				first = false;
			}
			else
			{
				assert(sm.numVertices == accessor.count);
			}

			uint8* data = m_buffer.data() + bufferView.offset + accessor.offset;

			for (uint32 vertexID = 0; vertexID < sm.numVertices; ++vertexID)
			{
				mesh_vertex& v = vertices[sm.firstVertex + vertexID];

				uint8* attributeData = data + accessor.stride * vertexID;

				if (j == attribute_type_position)
				{
					v.position = *(vec3*)attributeData;
				}
				else if (j == attribute_type_normal)
				{
					v.normal = *(vec3*)attributeData;
				}
			}
		}

		// Meshlet data
		{
			buffer_accessor& accessor = accessors[meshView.meshlets];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			meshlet_info* start = (meshlet_info*)(m_buffer.data() + bufferView.offset);

			assert(accessor.count * accessor.size == bufferView.size);

			sm.firstMeshlet = (uint32)meshlets.size();
			sm.numMeshlets = accessor.count;

			meshlets.insert(meshlets.end(), start, start + sm.numMeshlets);
		}

		// Meshlet Subset data
		{
			buffer_accessor& accessor = accessors[meshView.meshletSubsets];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			subset* start = (subset*)(m_buffer.data() + bufferView.offset);

			assert(accessor.count * accessor.size == bufferView.size);

			sm.firstMeshletSubset = (uint32)meshletSubsets.size();
			sm.numMeshletSubsets = accessor.count;

			meshletSubsets.insert(meshletSubsets.end(), start, start + sm.numMeshletSubsets);
		}

		// Unique Vertex Index data
		{
			buffer_accessor& accessor = accessors[meshView.uniqueVertexIndices];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			assert(accessor.count * accessor.size == bufferView.size);

			sm.firstUniqueVertexIndex = (uint32)uniqueVertexIndices.size();
			sm.numUniqueVertexIndices = accessor.count;

			if (accessor.size == sizeof(uint32))
			{
				uint32* start = (uint32*)(m_buffer.data() + bufferView.offset);
				uniqueVertexIndices.insert(uniqueVertexIndices.end(), start, start + sm.numUniqueVertexIndices);
			}
			else
			{
				assert(accessor.size == sizeof(uint16));

				uint16* start = (uint16*)(m_buffer.data() + bufferView.offset);

				std::vector<uint16> temp;
				temp.insert(temp.end(), start, start + sm.numUniqueVertexIndices);

				for (uint16 t : temp)
				{
					uniqueVertexIndices.push_back((uint32)t);
				}
			}
		}

		// Primitive Index data
		{
			buffer_accessor& accessor = accessors[meshView.primitiveIndices];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			packed_triangle* start = (packed_triangle*)(m_buffer.data() + bufferView.offset);

			assert(accessor.count * accessor.size == bufferView.size);

			sm.firstPackedTriangle = (uint32)primitiveIndices.size();
			sm.numPackedTriangles = accessor.count;

			primitiveIndices.insert(primitiveIndices.end(), start, start + sm.numPackedTriangles);
		}

#if 0
		// Cull data
		{
			buffer_accessor& accessor = accessors[meshView.CullData];
			buffer_view& bufferView = bufferViews[accessor.BufferView];

			mesh.CullingData = MakeSpan(reinterpret_cast<CullData*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}
#endif
	}

#if 0
	// Build bounding spheres for each mesh
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_meshes.size()); ++i)
	{
		auto& m = m_meshes[i];

		uint32_t vbIndexPos = 0;

		// Find the index of the vertex buffer of the position attribute
		for (uint32_t j = 1; j < m.LayoutDesc.NumElements; ++j)
		{
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0)
			{
				vbIndexPos = j;
				break;
			}
		}

		// Find the byte offset of the position attribute with its vertex buffer
		uint32_t positionOffset = 0;

		for (uint32_t j = 0; j < m.LayoutDesc.NumElements; ++j)
		{
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0)
			{
				break;
			}

			if (desc.InputSlot == vbIndexPos)
			{
				positionOffset += GetFormatSize(m.LayoutElems[j].Format);
			}
		}

		XMFLOAT3* v0 = reinterpret_cast<XMFLOAT3*>(m.Vertices[vbIndexPos].data() + positionOffset);
		uint32_t stride = m.VertexStrides[vbIndexPos];

		BoundingSphere::CreateFromPoints(m.BoundingSphere, m.VertexCount, v0, stride);

		if (i == 0)
		{
			m_boundingSphere = m.BoundingSphere;
		}
		else
		{
			BoundingSphere::CreateMerged(m_boundingSphere, m_boundingSphere, m.BoundingSphere);
		}
	}
#endif

	ref<mesh_shader_mesh> result = make_ref<mesh_shader_mesh>();
	result->submeshes = std::move(submeshes);
	result->subsets = std::move(meshletSubsets);
	result->vertices = createBuffer(sizeof(mesh_vertex), (uint32)vertices.size(), vertices.data());
	result->meshlets = createBuffer(sizeof(meshlet_info), (uint32)meshlets.size(), meshlets.data());
	result->uniqueVertexIndices = createBuffer(sizeof(uint32), (uint32)uniqueVertexIndices.size(), uniqueVertexIndices.data());
	result->primitiveIndices = createBuffer(sizeof(packed_triangle), (uint32)primitiveIndices.size(), primitiveIndices.data());

	return result;
}


