#pragma once

#include "dx_render_primitives.h"
#include "math.h"

#include <unordered_map>

enum acceleration_structure_rebuild_mode
{
	acceleration_structure_no_rebuild,
	acceleration_structure_rebuild,
	acceleration_structure_refit,
};

struct raytracing_blas_geometry
{
	ref<dx_vertex_buffer> vertexBuffer;
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
	raytracing_blas_builder();
	raytracing_blas_builder& push(ref<dx_vertex_buffer> vertexBuffer, ref<dx_index_buffer> indexBuffer, submesh_info submesh, bool opaque = true, const trs& localTransform = trs::identity);
	raytracing_blas finish();

private:
	std::vector<raytracing_blas_geometry> geometries;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	std::vector<mat4> localTransforms;
};

struct raytracing_tlas
{
	ref<dx_buffer> scratch;
	ref<dx_buffer> tlas;
};




struct raytracing_shader_binding_table_desc
{
	uint32 entrySize;

	void* raygen;
	std::vector<void*> miss;
	std::vector<void*> hitGroups;
};

struct dx_raytracing_pipeline
{
	dx_raytracing_pipeline_state pipeline;
	dx_root_signature rootSignature;

	raytracing_shader_binding_table_desc shaderBindingTableDesc;
};

struct raytracing_pipeline_builder
{
	raytracing_pipeline_builder(const wchar* shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth);

	raytracing_pipeline_builder& globalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc);
	raytracing_pipeline_builder& raygen(const wchar* entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {});
	raytracing_pipeline_builder& hitgroup(const wchar* groupName, const wchar* closestHit, const wchar* anyHit, const wchar* miss, 
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}); // The root signature describes parameters for both hit shaders. Miss will not get any arguments for now.

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

	D3D12_ROOT_SIGNATURE_DESC raygenRSDesc;
	std::vector<D3D12_ROOT_SIGNATURE_DESC> hitGroupRSDescs;

	uint32 payloadSize;
	uint32 maxRecursionDepth;

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
};
