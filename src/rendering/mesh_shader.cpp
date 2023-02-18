#include "pch.h"
#include "mesh_shader.h"
#include "dx/dx_pipeline.h"
#include "dx/dx_command_list.h"
#include "dx/dx_profiling.h"
#include "material.h"
#include "render_utils.h"

#if defined(SDK_SUPPORTS_MESH_SHADERS) && defined(MESH_SHADER_SUPPORTED)

static dx_pipeline meshletPipeline;
static dx_pipeline blobPipeline;
static dx_pipeline kochPipeline;

static ref<dx_buffer> marchingCubesBuffer;

struct marching_cubes_lookup
{
	uint32 indices[4];
	uint8 vertices[12];
	uint16 triangleCount;
	uint16 vertexCount;
};

extern const marching_cubes_lookup marchingCubesLookup[256];

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



struct meta_ball
{
	vec3 pos;
	vec3 dir;
	float radius;
};

static const uint32 DEFAULT_SHIFT = 7;
static const uint32 DEFAULT_BALL_COUNT = 32;
static const uint32 MAX_BALL_COUNT = 128;
static const uint32 BALL_COUNT = DEFAULT_BALL_COUNT;
static const uint32 SHIFT = DEFAULT_SHIFT;

struct mesh_shader_blob_render_data
{
	meta_ball balls[MAX_BALL_COUNT];

	mesh_shader_blob_render_data()
	{
		for (uint32 i = 0; i < BALL_COUNT; ++i)
		{
			// Random positions in [0.25, 0.75]
			balls[i].pos.x = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;
			balls[i].pos.y = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;
			balls[i].pos.z = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;

			// Random directions in [-0.6, 0.6]
			balls[i].dir.x = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;
			balls[i].dir.y = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;
			balls[i].dir.z = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;

			// Random radius in [0.02, 0.06]
			balls[i].radius = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.04f + 0.02f;
		}
	}
};

struct mesh_shader_blob_pipeline
{
	using render_data_t = mesh_shader_blob_render_data;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(mesh_shader_blob_pipeline)
{
	cl->setPipelineState(*blobPipeline.pipeline);
	cl->setGraphicsRootSignature(*blobPipeline.rootSignature);

	cl->setGraphicsDynamicConstantBuffer(1, common.cameraCBV);
	cl->setRootGraphicsSRV(2, marchingCubesBuffer);

	cl->setDescriptorHeapSRV(3, 0, common.sky);
}

PIPELINE_RENDER_IMPL(mesh_shader_blob_pipeline)
{
	DX_PROFILE_BLOCK(cl, "Mesh shader blob");

	struct constant_cb
	{
		vec4 balls[MAX_BALL_COUNT];
	};

	constant_cb cb;

	for (uint32 i = 0; i < BALL_COUNT; ++i)
	{
		float radius = rc.data.balls[i].radius;
		cb.balls[i] = vec4(rc.data.balls[i].pos, radius * radius);
	}

	auto b = dxContext.uploadDynamicConstantBuffer(cb);

	cl->setGraphicsDynamicConstantBuffer(0, b);


	const uint32 gridSize = (1 << SHIFT);
	cl->dispatchMesh((gridSize / 4)* (gridSize / 4)* (gridSize / 4), 1, 1);
};



struct mesh_shader_koch_pipeline
{
	using render_data_t = void*;

	PIPELINE_SETUP_DECL;
	PIPELINE_RENDER_DECL;
};

PIPELINE_SETUP_IMPL(mesh_shader_koch_pipeline)
{
	cl->setPipelineState(*kochPipeline.pipeline);
	cl->setGraphicsRootSignature(*kochPipeline.rootSignature);

	cl->setGraphicsDynamicConstantBuffer(0, common.cameraCBV);
	cl->setRootGraphicsSRV(1, marchingCubesBuffer);

	cl->setDescriptorHeapSRV(2, 0, common.sky);
}

PIPELINE_RENDER_IMPL(mesh_shader_koch_pipeline)
{
	DX_PROFILE_BLOCK(cl, "Mesh shader koch");

	const uint32 gridSize = (1 << SHIFT);
	cl->dispatchMesh((gridSize / 4) * (gridSize / 4) * (gridSize / 4), 1, 1);
};


void initializeMeshShader()
{
	D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
	renderTargetFormat.NumRenderTargets = 1;
	renderTargetFormat.RTFormats[0] = hdrFormat;

	{
		struct pipeline_state_stream : dx_pipeline_stream_base
		{
			// Will be set by reloader.
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_MS ms;
			CD3DX12_PIPELINE_STATE_STREAM_PS ps;

			// Initialized here.
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;

			void setRootSignature(dx_root_signature rs) override { rootSignature = rs.rootSignature.Get(); }
			void setMeshShader(dx_blob blob) override { ms = CD3DX12_SHADER_BYTECODE(blob.Get()); }
			void setPixelShader(dx_blob blob) override { ps = CD3DX12_SHADER_BYTECODE(blob.Get()); }
		};

		pipeline_state_stream stream;
		stream.dsvFormat = depthStencilFormat;
		stream.rtvFormats = renderTargetFormat;

		graphics_pipeline_files files = {};
		files.ms = "meshlet_ms";
		files.ps = "mesh_shader_ps";
		meshletPipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);
	}
	{
		struct pipeline_state_stream : dx_pipeline_stream_base
		{
			// Will be set by reloader.
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_AS as;
			CD3DX12_PIPELINE_STATE_STREAM_MS ms;
			CD3DX12_PIPELINE_STATE_STREAM_PS ps;

			// Initialized here.
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;

			void setRootSignature(dx_root_signature rs) override { rootSignature = rs.rootSignature.Get(); }
			void setAmplificationShader(dx_blob blob) override { as = CD3DX12_SHADER_BYTECODE(blob.Get()); }
			void setMeshShader(dx_blob blob) override { ms = CD3DX12_SHADER_BYTECODE(blob.Get()); }
			void setPixelShader(dx_blob blob) override { ps = CD3DX12_SHADER_BYTECODE(blob.Get()); }
		};

		auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
		rasterizerDesc.FrontCounterClockwise = TRUE; // Righthanded coordinate system.

		pipeline_state_stream stream;
		stream.dsvFormat = depthStencilFormat;
		stream.rtvFormats = renderTargetFormat;
		stream.rasterizer = rasterizerDesc;

		graphics_pipeline_files files = {};
		files.as = "meta_ball_as";
		files.ms = "meta_ball_ms";
		files.ps = "glass_ps";
		blobPipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);


		files.as = "koch_as";
		files.ms = "koch_ms";
		kochPipeline = createReloadablePipelineFromStream(stream, files, rs_in_mesh_shader);


		marchingCubesBuffer = createBuffer(sizeof(marching_cubes_lookup), arraysize(marchingCubesLookup), (void*)marchingCubesLookup);

	}
}

void testRenderMeshShader(transparent_render_pass* ldrRenderPass, float dt)
{
	/*overlayRenderPass->renderObjectWithMeshShader(1, 1, 1,
		cubeMaterial,
		createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 1.f),
		true
	);*/


	/*if (meshMaterial->mesh)
	{
		auto& sm = meshMaterial->mesh->submeshes[0];
		overlayRenderPass->renderObjectWithMeshShader(sm.numMeshlets, 1, 1,
			meshMaterial,
			createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 0.3f),
			true
		);
	}*/




	static mesh_shader_blob_render_data blobData;

	const float animationSpeed = 0.25f;
	const float frameTime = animationSpeed * dt;
	for (uint32 i = 0; i < BALL_COUNT; ++i)
	{
		vec3 d = vec3(0.5f, 0.5f, 0.5f) - blobData.balls[i].pos;
		blobData.balls[i].dir += d * (5.f * frameTime / (2.f + dot(d, d)));
		blobData.balls[i].pos += blobData.balls[i].dir * frameTime;
	}


	ldrRenderPass->renderObject<mesh_shader_blob_pipeline>(blobData);
	//ldrRenderPass->renderObject<mesh_shader_koch_pipeline>(0);
}







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
		std::cerr << "Could not find file '" << filename << "'.\n";
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




const marching_cubes_lookup marchingCubesLookup[256] =
{
	/*  Packed 8bit indices                                 Two 3bit (octal) corner indices defining the edge                 */
	{ { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, { 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 0, 0 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 010, 020, 040, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 010, 051, 031, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 031, 020, 040, 051, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 020, 032, 062, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 010, 032, 062, 040, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 031, 010, 051, 032, 062, 020, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 031, 032, 062, 051, 040, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 031, 073, 032, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 020, 040, 031, 073, 032, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 051, 073, 032, 010, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 032, 020, 040, 073, 051, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 020, 031, 073, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 010, 031, 073, 040, 062, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 020, 010, 051, 062, 073, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x01020100, 0x00000203, 0x00000000, 0x00000000 }, { 051, 073, 040, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 054, 040, 064, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 054, 010, 020, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 051, 031, 040, 064, 054, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 054, 051, 031, 064, 020, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 040, 064, 054, 020, 032, 062, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 062, 064, 054, 032, 010, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 051, 031, 010, 040, 064, 054, 032, 062, 020, 000, 000, 000 }, 3, 9 },
	{ { 0x03020100, 0x04030001, 0x04050301, 0x00000000 }, { 054, 062, 064, 051, 032, 031, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 031, 073, 032, 040, 064, 054, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 020, 064, 054, 010, 031, 073, 032, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 051, 073, 032, 010, 040, 064, 054, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 032, 051, 073, 064, 020, 054, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 020, 031, 073, 062, 064, 054, 040, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 031, 073, 062, 054, 010, 064, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 054, 040, 064, 051, 062, 010, 073, 020, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 054, 062, 064, 051, 073, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 051, 054, 075, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 051, 054, 075, 010, 020, 040, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 010, 054, 075, 031, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 040, 054, 075, 020, 031, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 051, 054, 075, 032, 062, 020, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 010, 032, 062, 040, 054, 075, 051, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 010, 054, 075, 031, 032, 062, 020, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 032, 075, 031, 040, 062, 054, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 031, 073, 032, 051, 054, 075, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 020, 040, 010, 031, 073, 032, 054, 075, 051, 000, 000, 000 }, 3, 9 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 075, 073, 032, 054, 010, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x04030001, 0x04050301, 0x00000000 }, { 032, 075, 073, 020, 054, 040, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 073, 062, 020, 031, 051, 054, 075, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 054, 075, 051, 010, 031, 040, 073, 062, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 075, 010, 054, 062, 073, 020, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 075, 040, 054, 073, 062, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 051, 040, 064, 075, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 051, 010, 020, 075, 064, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 010, 040, 064, 031, 075, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x01020100, 0x00000203, 0x00000000, 0x00000000 }, { 031, 020, 075, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 064, 075, 051, 040, 020, 032, 062, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 051, 064, 075, 032, 010, 062, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 032, 062, 020, 010, 040, 031, 064, 075, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 062, 031, 032, 064, 075, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 051, 040, 064, 075, 073, 032, 031, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 073, 032, 031, 051, 010, 075, 020, 064, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 040, 032, 010, 075, 064, 073, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 032, 075, 073, 020, 064, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x01020100, 0x05040203, 0x05070406, 0x00000000 }, { 051, 040, 075, 064, 073, 020, 031, 062, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 075, 010, 064, 051, 062, 031, 073, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 062, 010, 073, 020, 075, 040, 064, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 062, 075, 073, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 064, 062, 076, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 020, 040, 010, 062, 076, 064, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 051, 031, 062, 076, 064, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 040, 051, 031, 020, 062, 076, 064, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 064, 020, 032, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 064, 040, 010, 076, 032, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 032, 076, 064, 020, 010, 051, 031, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 031, 032, 076, 040, 051, 064, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 073, 032, 031, 076, 064, 062, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 031, 073, 032, 020, 040, 010, 076, 064, 062, 000, 000, 000 }, 3, 9 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 032, 010, 051, 073, 076, 064, 062, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 076, 064, 062, 032, 020, 073, 040, 051, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 073, 076, 064, 031, 020, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x02030200, 0x05040304, 0x00000000 }, { 073, 076, 064, 031, 040, 010, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 010, 064, 020, 073, 051, 076, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 064, 073, 076, 040, 051, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 076, 054, 040, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 020, 062, 076, 010, 054, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 040, 062, 076, 054, 051, 031, 010, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 051, 076, 054, 020, 031, 062, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 040, 020, 032, 054, 076, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x02020100, 0x00000301, 0x00000000, 0x00000000 }, { 010, 032, 054, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 031, 010, 051, 032, 054, 020, 076, 040, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 031, 054, 051, 032, 076, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 076, 054, 040, 062, 032, 031, 073, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 031, 073, 032, 020, 062, 010, 076, 054, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x05040302, 0x07050606, 0x00000000 }, { 054, 040, 062, 076, 010, 051, 032, 073, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 073, 020, 051, 032, 054, 062, 076, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 040, 020, 031, 076, 054, 073, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 073, 010, 031, 076, 054, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 054, 020, 076, 040, 073, 010, 051, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 073, 054, 051, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 054, 075, 051, 064, 062, 076, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 010, 020, 040, 054, 075, 051, 062, 076, 064, 000, 000, 000 }, 3, 9 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 075, 031, 010, 054, 064, 062, 076, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 062, 076, 064, 040, 054, 020, 075, 031, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 064, 020, 032, 076, 075, 051, 054, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 051, 054, 075, 010, 076, 040, 032, 064, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x05040302, 0x07050606, 0x00000000 }, { 020, 032, 076, 064, 031, 010, 075, 054, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 076, 040, 032, 064, 031, 054, 075, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 051, 054, 075, 073, 032, 031, 064, 062, 076, 000, 000, 000 }, 3, 9 },
	{ { 0x03020100, 0x07060504, 0x0B0A0908, 0x00000000 }, { 076, 064, 062, 031, 073, 032, 010, 020, 040, 054, 075, 051 }, 4, 12},
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 064, 062, 076, 075, 073, 054, 032, 010, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 020, 040, 054, 075, 032, 073, 062, 076, 064, 000, 000, 000 }, 5, 9 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 051, 054, 075, 073, 076, 031, 064, 020, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 031, 073, 076, 064, 010, 040, 051, 054, 075, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 054, 073, 010, 075, 020, 076, 064, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 064, 073, 076, 040, 075, 054, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 076, 075, 051, 062, 040, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x02030200, 0x05040304, 0x00000000 }, { 020, 062, 076, 010, 075, 051, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 010, 040, 062, 075, 031, 076, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 076, 020, 062, 075, 031, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 075, 051, 040, 032, 076, 020, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 051, 076, 075, 010, 032, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 031, 040, 075, 010, 076, 020, 032, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 031, 076, 075, 032, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 031, 073, 032, 051, 062, 075, 040, 076, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 010, 020, 062, 076, 051, 075, 031, 073, 032, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 062, 075, 040, 076, 010, 073, 032, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 076, 020, 062, 075, 032, 073, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 031, 076, 020, 073, 040, 075, 051, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 073, 010, 031, 076, 051, 075, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 040, 020, 075, 073, 076, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 073, 076, 075, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 073, 075, 076, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 020, 040, 075, 076, 073, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 051, 031, 010, 075, 076, 073, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 031, 020, 040, 051, 075, 076, 073, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 032, 062, 020, 073, 075, 076, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 062, 040, 010, 032, 073, 075, 076, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 010, 051, 031, 032, 062, 020, 075, 076, 073, 000, 000, 000 }, 3, 9 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 075, 076, 073, 031, 032, 051, 062, 040, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 031, 075, 076, 032, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 031, 075, 076, 032, 020, 040, 010, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 051, 075, 076, 010, 032, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 075, 040, 051, 032, 076, 020, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 076, 062, 020, 075, 031, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 010, 062, 040, 075, 031, 076, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x04030001, 0x04050301, 0x00000000 }, { 020, 076, 062, 010, 075, 051, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 076, 051, 075, 062, 040, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 075, 076, 073, 054, 040, 064, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 054, 010, 020, 064, 076, 073, 075, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 031, 010, 051, 075, 076, 073, 040, 064, 054, 000, 000, 000 }, 3, 9 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 073, 075, 076, 031, 064, 051, 020, 054, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x03020100, 0x07060504, 0x00000008, 0x00000000 }, { 020, 032, 062, 064, 054, 040, 073, 075, 076, 000, 000, 000 }, 3, 9 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 075, 076, 073, 054, 032, 064, 010, 062, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x03020100, 0x07060504, 0x0B0A0908, 0x00000000 }, { 010, 051, 031, 054, 040, 064, 032, 062, 020, 075, 076, 073 }, 4, 12},
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 051, 031, 032, 062, 054, 064, 075, 076, 073, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 076, 032, 031, 075, 054, 040, 064, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x01020100, 0x05040203, 0x05070406, 0x00000000 }, { 031, 075, 032, 076, 020, 054, 010, 064, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 040, 064, 054, 051, 075, 010, 076, 032, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 064, 051, 020, 054, 032, 075, 076, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 040, 064, 054, 020, 075, 062, 031, 076, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 075, 062, 031, 076, 010, 064, 054, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 010, 051, 075, 076, 020, 062, 040, 064, 054, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 076, 051, 075, 062, 054, 064, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 073, 051, 054, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 054, 076, 073, 051, 010, 020, 040, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 073, 031, 010, 076, 054, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 040, 031, 020, 076, 054, 073, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 073, 051, 054, 076, 062, 020, 032, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x01020100, 0x05040203, 0x05070406, 0x00000000 }, { 010, 032, 040, 062, 054, 073, 051, 076, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 020, 032, 062, 010, 076, 031, 054, 073, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 076, 031, 054, 073, 040, 032, 062, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 031, 051, 054, 032, 076, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 020, 040, 010, 031, 051, 032, 054, 076, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x01020100, 0x00000203, 0x00000000, 0x00000000 }, { 010, 054, 032, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 040, 032, 020, 054, 076, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 051, 054, 076, 020, 031, 062, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 040, 031, 062, 010, 076, 051, 054, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 020, 076, 062, 010, 054, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 076, 040, 054, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 064, 076, 073, 040, 051, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 010, 020, 064, 073, 051, 076, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x04030001, 0x04050301, 0x00000000 }, { 073, 064, 076, 031, 040, 010, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 073, 064, 076, 031, 020, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 032, 062, 020, 073, 040, 076, 051, 064, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 032, 064, 010, 062, 051, 076, 073, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 031, 010, 040, 064, 073, 076, 032, 062, 020, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 062, 031, 032, 064, 073, 076, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04000103, 0x01050303, 0x00000000 }, { 031, 076, 032, 040, 051, 064, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 032, 051, 076, 031, 064, 010, 020, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 064, 010, 040, 076, 032, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 064, 032, 020, 076, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 040, 076, 051, 064, 031, 062, 020, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 010, 031, 051, 062, 064, 076, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 064, 010, 040, 076, 020, 062, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 064, 076, 062, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000200, 0x00000000, 0x00000000 }, { 062, 073, 075, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 062, 073, 075, 064, 040, 010, 020, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x05040302, 0x00000006, 0x00000000 }, { 075, 064, 062, 073, 031, 010, 051, 000, 000, 000, 000, 000 }, 3, 7 },
	{ { 0x00020100, 0x05040302, 0x07050606, 0x00000000 }, { 073, 075, 064, 062, 051, 031, 040, 020, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 032, 073, 075, 020, 064, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 040, 010, 032, 075, 064, 073, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 051, 031, 010, 075, 020, 073, 064, 032, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 051, 032, 040, 031, 064, 073, 075, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 062, 032, 031, 064, 075, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 010, 020, 040, 031, 064, 032, 075, 062, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 051, 075, 064, 032, 010, 062, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 064, 032, 075, 062, 051, 020, 040, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x02020100, 0x00000301, 0x00000000, 0x00000000 }, { 031, 075, 020, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 010, 064, 040, 031, 075, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 051, 020, 010, 075, 064, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 051, 064, 040, 075, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 075, 054, 040, 073, 062, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 075, 054, 010, 062, 073, 020, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x06030504, 0x05070404, 0x00000000 }, { 010, 051, 031, 040, 073, 054, 062, 075, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 073, 054, 062, 075, 020, 051, 031, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x02030200, 0x05040304, 0x00000000 }, { 032, 073, 075, 020, 054, 040, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 075, 032, 073, 054, 010, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 020, 032, 073, 075, 040, 054, 010, 051, 031, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 075, 032, 073, 054, 031, 051, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x03000302, 0x02030504, 0x00000000 }, { 032, 031, 075, 040, 062, 054, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 010, 062, 054, 020, 075, 032, 031, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 010, 075, 032, 051, 062, 054, 040, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 051, 075, 054, 032, 020, 062, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 040, 075, 054, 020, 031, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 010, 075, 054, 031, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 040, 075, 054, 020, 051, 010, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 051, 075, 054, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00020100, 0x02030302, 0x00000004, 0x00000000 }, { 054, 064, 062, 051, 073, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x04050504, 0x07060506, 0x00000000 }, { 010, 020, 040, 054, 064, 051, 062, 073, 000, 000, 000, 000 }, 4, 8 },
	{ { 0x00020100, 0x04000103, 0x03010503, 0x00000000 }, { 031, 062, 073, 054, 010, 064, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 020, 054, 031, 040, 073, 064, 062, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x03000302, 0x05020304, 0x00000000 }, { 032, 073, 051, 064, 020, 054, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 051, 064, 073, 054, 032, 040, 010, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x00020100, 0x01020103, 0x01060504, 0x00060104 }, { 020, 073, 064, 032, 054, 031, 010, 000, 000, 000, 000, 000 }, 5, 7 },
	{ { 0x03020100, 0x00000504, 0x00000000, 0x00000000 }, { 031, 032, 073, 040, 054, 064, 000, 000, 000, 000, 000, 000 }, 2, 6 },
	{ { 0x03020100, 0x02030200, 0x05040304, 0x00000000 }, { 054, 064, 062, 051, 032, 031, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x03000302, 0x03040504, 0x00080706 }, { 051, 054, 064, 062, 031, 032, 010, 020, 040, 000, 000, 000 }, 5, 9 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 062, 054, 064, 032, 010, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 062, 054, 064, 032, 040, 020, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 054, 031, 051, 064, 020, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 054, 031, 051, 064, 010, 040, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 054, 020, 010, 064, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 054, 064, 040, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x02020100, 0x00000301, 0x00000000, 0x00000000 }, { 051, 040, 073, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 020, 051, 010, 062, 073, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 010, 073, 031, 040, 062, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 020, 073, 031, 062, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 032, 040, 020, 073, 051, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 051, 032, 073, 010, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 032, 040, 020, 073, 010, 031, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 031, 032, 073, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00020100, 0x04030103, 0x00000001, 0x00000000 }, { 031, 062, 032, 051, 040, 000, 000, 000, 000, 000, 000, 000 }, 3, 5 },
	{ { 0x00020100, 0x01040103, 0x03010505, 0x00000000 }, { 020, 051, 010, 062, 031, 032, 000, 000, 000, 000, 000, 000 }, 4, 6 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 010, 062, 032, 040, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 020, 062, 032, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x03020100, 0x00000001, 0x00000000, 0x00000000 }, { 031, 040, 020, 051, 000, 000, 000, 000, 000, 000, 000, 000 }, 2, 4 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 010, 031, 051, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00020100, 0x00000000, 0x00000000, 0x00000000 }, { 010, 040, 020, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 1, 3 },
	{ { 0x00000000, 0x00000000, 0x00000000, 0x00000000 }, { 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000 }, 0, 0 },
};


#else

void initializeMeshShader() {}
void testRenderMeshShader(transparent_render_pass* ldrRenderPass, float dt) {}

#endif

