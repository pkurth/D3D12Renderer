#pragma once

#include "geometry/mesh.h"
#include "dx/dx_buffer.h"
#include "core/math.h"
#include "dx/dx_pipeline.h"

// Formula for hit shader index calculation: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#hit-group-table-indexing


enum raytracing_as_rebuild_mode
{
	raytracing_as_rebuild,
	raytracing_as_refit,
};

enum raytracing_geometry_type
{
	raytracing_mesh_geometry,
	raytracing_procedural_geometry,
};

struct raytracing_blas_geometry
{
	raytracing_geometry_type type;

	// Only valid for mesh geometry.
	vertex_buffer_group vertexBuffer;
	ref<dx_index_buffer> indexBuffer;
	submesh_info submesh;
};

struct raytracing_blas
{
	ref<dx_buffer> scratch;
	ref<dx_buffer> blas;

	std::vector<raytracing_blas_geometry> geometries;
};

struct raytracing_blas_builder
{
	raytracing_blas_builder& push(vertex_buffer_group vertexBuffer, ref<dx_index_buffer> indexBuffer, submesh_info submesh, bool opaque = true, const trs& localTransform = trs::identity);
	raytracing_blas_builder& push(const std::vector<bounding_box>& boundingBoxes, bool opaque);
	ref<raytracing_blas> finish(bool keepScratch = false);

private:
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;

	std::vector<raytracing_blas_geometry> geometries;

	std::vector<mat4> localTransforms;				// For meshes.
	std::vector<D3D12_RAYTRACING_AABB> aabbDescs;	// For procedurals.
};

struct raytracing_object_type
{
	ref<raytracing_blas> blas;
	uint32 instanceContributionToHitGroupIndex;
};


struct raytracing_shader
{
	void* mesh;
	void* procedural;
};

struct raytracing_shader_binding_table_desc
{
	uint32 entrySize;

	void* raygen;
	std::vector<void*> miss;
	std::vector<raytracing_shader> hitGroups;

	uint32 raygenOffset;
	uint32 missOffset;
	uint32 hitOffset;
};

struct dx_raytracing_pipeline
{
	dx_raytracing_pipeline_state pipeline;
	dx_root_signature rootSignature;

	raytracing_shader_binding_table_desc shaderBindingTableDesc;
};







struct raytracing_mesh_hitgroup
{
	const wchar* closestHit;	// Optional.
	const wchar* anyHit;		// Optional.
};

struct raytracing_procedural_hitgroup
{
	const wchar* intersection;
	const wchar* closestHit;	// Optional.
	const wchar* anyHit;		// Optional.
};


struct raytracing_pipeline_builder
{
	raytracing_pipeline_builder(const wchar* shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth, bool hasMeshGeometry, bool hasProceduralGeometry);

	raytracing_pipeline_builder& globalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc);
	raytracing_pipeline_builder& raygen(const wchar* entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {});

	// The root signature describes parameters for both hit shaders. Miss will not get any arguments for now.
	raytracing_pipeline_builder& hitgroup(const wchar* groupName, const wchar* miss, 
		raytracing_mesh_hitgroup mesh, D3D12_ROOT_SIGNATURE_DESC meshRootSignatureDesc = {}, 
		raytracing_procedural_hitgroup procedural = {}, D3D12_ROOT_SIGNATURE_DESC proceduralRootSignatureDesc = {});

	dx_raytracing_pipeline finish();


private:
	struct raytracing_root_signature
	{
		dx_root_signature rootSignature;
		ID3D12RootSignature* rootSignaturePtr;
	};

	raytracing_root_signature createRaytracingRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);


	raytracing_root_signature globalRS;
	raytracing_root_signature raygenRS;

	std::vector<const wchar*> emptyAssociations;
	std::vector<const wchar*> allExports;

	const wchar* raygenEntryPoint;
	std::vector<const wchar*> missEntryPoints;

	std::vector<const wchar*> shaderNameDefines;

	uint32 payloadSize;
	uint32 maxRecursionDepth;

	bool hasMeshGeometry; 
	bool hasProceduralGeometry;

	uint32 tableEntrySize = 0;

	const wchar* shaderFilename;

	// Since these store pointers to each other, they are not resizable arrays.
	D3D12_STATE_SUBOBJECT subobjects[512];
	uint32 numSubobjects = 0;

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION associations[16];
	uint32 numAssociations = 0;

	D3D12_HIT_GROUP_DESC hitGroups[8];
	uint32 numHitGroups = 0;

	D3D12_EXPORT_DESC exports[24];
	uint32 numExports = 0;

	const wchar* stringBuffer[128];
	uint32 numStrings = 0;

	raytracing_root_signature rootSignatures[8];
	uint32 numRootSignatures = 0;

	std::wstring groupNameStorage[8];
	uint32 groupNameStoragePtr = 0;
};
