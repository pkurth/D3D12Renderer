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

struct raytracing_blas
{
	dx_buffer scratch;
	dx_buffer result;
	uint32 numGeometries;
};

struct raytracing_tlas
{
	dx_buffer scratch;
	dx_buffer result;
};

struct raytracing_blas_builder
{
	raytracing_blas_builder(acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_no_rebuild);
	raytracing_blas_builder& push(const dx_vertex_buffer& vertexBuffer, const dx_index_buffer& indexBuffer, submesh_info submesh, bool opaque = true, const trs& localTransform = trs::identity);
	raytracing_blas finish();

private:
	acceleration_structure_rebuild_mode rebuildMode;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	std::vector<mat4> localTransforms;
};

struct raytracing_tlas_builder
{
	raytracing_tlas_builder(acceleration_structure_rebuild_mode rebuildMode = acceleration_structure_no_rebuild);
	raytracing_tlas_builder& push(const raytracing_blas& blas, const trs& transform);
	raytracing_tlas finish(uint32 numRayTypes);

private:
	acceleration_structure_rebuild_mode rebuildMode;
	std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, std::vector<D3D12_RAYTRACING_INSTANCE_DESC>> instances;
};







struct raytracing_shader_binding_table_desc
{
	uint32 entrySize;
	uint32 numHitShaders;

	uint32 raygenOffset;
	uint32 missOffset;
	uint32 hitOffset;

	uint32 sizeWithoutHitEntries;
};

struct dx_raytracing_pipeline
{
	dx_raytracing_pipeline_state pipeline;
	dx_root_signature rootSignature;

	void* raygenIdentifier;
	std::vector<void*> hitGroupIdentifiers;
	std::vector<void*> missIdentifiers;

	raytracing_shader_binding_table_desc shaderBindingTableDesc;
};

enum raytracing_association
{
	associate_with_nothing = 0,
	associate_with_closest_hit = (1 << 0),
	associate_with_any_hit = (1 << 1),
	associate_with_all = (associate_with_closest_hit | associate_with_any_hit),
};

union raytracing_hit_group_desc
{
	struct
	{
		const wchar* closestHit;
		const wchar* anyHit;
		const wchar* miss;
	};
	const wchar* entryPoints[3];
};

struct raytracing_pipeline_builder
{
	raytracing_pipeline_builder(const wchar* shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth);

	raytracing_pipeline_builder& globalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc);
	raytracing_pipeline_builder& raygen(const wchar* entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc);
	raytracing_pipeline_builder& hitgroup(const wchar* groupName, raytracing_hit_group_desc desc, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {}, uint32 associateWith = associate_with_nothing);

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

	std::vector<raytracing_root_signature> rootSignatures;
	std::vector<const wchar*> emptyAssociations;
	std::vector<const wchar*> allExports;

	const wchar* raygenEntryPoint;
	std::vector<const wchar*> missEntryPoints;

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
};
