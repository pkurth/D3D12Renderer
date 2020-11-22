#include "pch.h"
#include "raytracing.h"
#include "dx_context.h"
#include "dx_command_list.h"

#include <dxcapi.h>
#include <fstream>
#include <sstream>

raytracing_blas_builder::raytracing_blas_builder(acceleration_structure_rebuild_mode rebuildMode)
{
	this->rebuildMode = rebuildMode;
}

raytracing_blas_builder& raytracing_blas_builder::push(const dx_vertex_buffer& vertexBuffer, const dx_index_buffer& indexBuffer, submesh_info submesh, bool opaque, const trs& localTransform)
{
	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc;

	geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geomDesc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

	if (&localTransform == &trs::identity)
	{
		geomDesc.Triangles.Transform3x4 = UINT64_MAX;
	}
	else
	{
		geomDesc.Triangles.Transform3x4 = localTransforms.size();
		localTransforms.push_back(transpose(trsToMat4(localTransform)));
	}

	geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer.gpuVirtualAddress + (vertexBuffer.elementSize * submesh.baseVertex);
	geomDesc.Triangles.VertexBuffer.StrideInBytes = vertexBuffer.elementSize;
	geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc.Triangles.VertexCount = submesh.numVertices;

	geomDesc.Triangles.IndexBuffer = indexBuffer.gpuVirtualAddress + (indexBuffer.elementSize * submesh.firstTriangle * 3);
	geomDesc.Triangles.IndexFormat = getIndexBufferFormat(indexBuffer.elementSize);
	geomDesc.Triangles.IndexCount = submesh.numTriangles * 3;

	geometryDescs.push_back(geomDesc);

	return *this;
}

raytracing_blas raytracing_blas_builder::finish()
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	if (rebuildMode == acceleration_structure_refit)
	{
		inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}

	inputs.NumDescs = (uint32)geometryDescs.size();
	inputs.pGeometryDescs = geometryDescs.data();

	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;



	// Allocate.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	dxContext.device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	info.ScratchDataSizeInBytes = alignTo(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	info.ResultDataMaxSizeInBytes = alignTo(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	raytracing_blas blas = {};
	blas.scratch = createBuffer((uint32)info.ScratchDataSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	blas.result = createBuffer((uint32)info.ResultDataMaxSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	SET_NAME(blas.scratch.resource, "BLAS Scratch");
	SET_NAME(blas.result.resource, "BLAS Result");



	dx_command_list* cl = dxContext.getFreeRenderCommandList();
	dx_dynamic_constant_buffer localTransformsBuffer;
	if (localTransforms.size() > 0)
	{
		localTransformsBuffer = cl->uploadDynamicConstantBuffer(sizeof(mat4) * (uint32)localTransforms.size(), localTransforms.data());
	}

	for (auto& desc : geometryDescs)
	{
		if (desc.Triangles.Transform3x4 == UINT64_MAX)
		{
			desc.Triangles.Transform3x4 = 0;
		}
		else
		{
			desc.Triangles.Transform3x4 = localTransformsBuffer.gpuPtr + sizeof(mat4) * desc.Triangles.Transform3x4;
		}
	}

	blas.numGeometries = (uint32)geometryDescs.size();


	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = blas.result.gpuVirtualAddress;
	asDesc.ScratchAccelerationStructureData = blas.scratch.gpuVirtualAddress;

	/*if (rebuild)
	{
		assert(rebuildMode != acceleration_structure_no_rebuild);

		commandList->uavBarrier(raytracing.blas.result);

		if (rebuildMode == acceleration_structure_refit)
		{
			inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			asDesc.SourceAccelerationStructureData = raytracing.blas.result.gpuVirtualAddress;
		}
	}*/

	cl->commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
	cl->uavBarrier(blas.result);
	dxContext.executeCommandList(cl);

	return blas;
}

raytracing_tlas_builder::raytracing_tlas_builder(acceleration_structure_rebuild_mode rebuildMode)
{
	this->rebuildMode = rebuildMode;
}

raytracing_tlas_builder& raytracing_tlas_builder::push(const raytracing_blas& blas, const trs& transform)
{
	D3D12_RAYTRACING_INSTANCE_DESC instance = {};

	std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instancesOfThisBlas = instances[blas.result.gpuVirtualAddress];

	instance.Flags = 0;// D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
	instance.InstanceContributionToHitGroupIndex = blas.numGeometries; // Is used later. This is not the final contribution.
	
	mat4 m = transpose(trsToMat4(transform));
	memcpy(instance.Transform, &m, sizeof(instance.Transform));
	instance.AccelerationStructure = blas.result.gpuVirtualAddress;
	instance.InstanceMask = 0xFF;

	instancesOfThisBlas.push_back(instance);

	return *this;
}

raytracing_tlas raytracing_tlas_builder::finish(uint32 numRayTypes)
{
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

	if (rebuildMode == acceleration_structure_refit)
	{
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	}


	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> allInstances;

	uint32 indexOfThisBlas = 0;
	uint32 instanceContributionOffset = 0;
	for (auto& blas : instances)
	{
		uint32 numInstances = (uint32)blas.second.size();
		uint32 numGeometries = blas.second[0].InstanceContributionToHitGroupIndex; // Is set above, when pushing instances.
		for (uint32 i = 0; i < numInstances; ++i)
		{
			D3D12_RAYTRACING_INSTANCE_DESC desc = blas.second[i];
			desc.InstanceID = indexOfThisBlas; // This value will be exposed to the shader via InstanceID().
			desc.InstanceContributionToHitGroupIndex = instanceContributionOffset + i * numGeometries * numRayTypes;

			allInstances.push_back(desc);
		}
		instanceContributionOffset += numInstances * numGeometries * numRayTypes;

		++indexOfThisBlas;
	}

	uint32 totalNumInstances = (uint32)allInstances.size();

	inputs.NumDescs = totalNumInstances;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;


	// Allocate.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
	dxContext.device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	info.ScratchDataSizeInBytes = alignTo(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	info.ResultDataMaxSizeInBytes = alignTo(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	raytracing_tlas tlas;
	tlas.scratch = createBuffer((uint32)info.ScratchDataSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tlas.result = createBuffer((uint32)info.ResultDataMaxSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

	SET_NAME(tlas.scratch.resource, "TLAS Scratch");
	SET_NAME(tlas.result.resource, "TLAS Result");


	dx_command_list* cl = dxContext.getFreeRenderCommandList();

	dx_dynamic_constant_buffer gpuInstances = cl->uploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * totalNumInstances, allInstances.data());


	inputs.InstanceDescs = gpuInstances.gpuPtr;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = tlas.result.gpuVirtualAddress;
	asDesc.ScratchAccelerationStructureData = tlas.scratch.gpuVirtualAddress;

	/*if (rebuild)
	{
		assert(rebuildMode != acceleration_structure_no_rebuild);

		commandList->uavBarrier(tlas.result);

		if (rebuildMode == acceleration_structure_refit)
		{
			inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			asDesc.SourceAccelerationStructureData = tlas.result.gpuVirtualAddress;
		}
	}*/
	cl->commandList->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
	cl->uavBarrier(tlas.result);

	dxContext.executeCommandList(cl);

	return tlas;
}




static void reportShaderCompileError(com<IDxcBlobEncoding> blob)
{
	char infoLog[2048];
	memcpy(infoLog, blob->GetBufferPointer(), sizeof(infoLog) - 1);
	infoLog[sizeof(infoLog) - 1] = 0;
	std::cerr << "Error: " << infoLog << std::endl;
}

static com<IDxcBlob> compileLibrary(const std::wstring& filename)
{
	com<IDxcCompiler> compiler;
	com<IDxcLibrary> library;
	com<IDxcIncludeHandler> includeHandler;
	checkResult(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
	checkResult(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library)));
	checkResult(library->CreateIncludeHandler(&includeHandler));


	std::ifstream stream(filename);
	if (!stream.is_open())
	{
		//std::cerr <<  "File " << filename << " not found." << std::endl;
		return 0;
	}
	std::stringstream ss; ss << stream.rdbuf();
	std::string source = ss.str();


	// Create blob from the string
	com<IDxcBlobEncoding> textBlob;
	checkResult(library->CreateBlobWithEncodingFromPinned((LPBYTE)source.c_str(), (uint32)source.length(), 0, &textBlob));

	std::wstring wfilename(filename.begin(), filename.end());

	// Compile
	com<IDxcOperationResult> operationResult;
	checkResult(compiler->Compile(textBlob.Get(), wfilename.c_str(), L"", L"lib_6_3", 0, 0, 0, 0, includeHandler.Get(), &operationResult));

	// Verify the result
	HRESULT resultCode;
	checkResult(operationResult->GetStatus(&resultCode));
	if (FAILED(resultCode))
	{
		com<IDxcBlobEncoding> error;
		checkResult(operationResult->GetErrorBuffer(&error));
		reportShaderCompileError(error);
		assert(false);
		return 0;
	}

	com<IDxcBlob> blob;
	checkResult(operationResult->GetResult(&blob));

	return blob;
}





raytracing_pipeline_builder::raytracing_pipeline_builder(const wchar* shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth)
{
	this->shaderFilename = shaderFilename;
	this->payloadSize = payloadSize;
	this->maxRecursionDepth = maxRecursionDepth;
}

raytracing_pipeline_builder::raytracing_root_signature raytracing_pipeline_builder::createRaytracingRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc)
{
	raytracing_root_signature result;
	result.rootSignature = createRootSignature(desc);
	result.rootSignaturePtr = result.rootSignature.rootSignature.Get();
	return result;
}

raytracing_pipeline_builder& raytracing_pipeline_builder::globalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc)
{
	assert(!globalRS.rootSignature.rootSignature);

	globalRS = createRaytracingRootSignature(rootSignatureDesc);
	SET_NAME(globalRS.rootSignature.rootSignature, "Global raytracing root signature");

	auto& so = subobjects[numSubobjects++];
	so.pDesc = &globalRS.rootSignaturePtr;
	so.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;

	return *this;
}

static uint32 getShaderBindingTableSize(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc)
{
	uint32 size = 0;
	for (uint32 i = 0; i < rootSignatureDesc.NumParameters; ++i)
	{
		if (rootSignatureDesc.pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
		{
			size += alignTo(rootSignatureDesc.pParameters[i].Constants.Num32BitValues * 4, 8);
		} 
		else
		{
			size += 8;
		}
	}
	return size;
}

raytracing_pipeline_builder& raytracing_pipeline_builder::raygen(const wchar* entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc)
{
	assert(!raygenRS.rootSignature.rootSignature);

	D3D12_EXPORT_DESC& exp = exports[numExports++];
	exp.Name = entryPoint;
	exp.Flags = D3D12_EXPORT_FLAG_NONE;
	exp.ExportToRename = 0;

	raygenEntryPoint = entryPoint;


	rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	raygenRS = createRaytracingRootSignature(rootSignatureDesc);
	SET_NAME(raygenRS.rootSignature.rootSignature, "Local raytracing root signature");

	{
		auto& so = subobjects[numSubobjects++];
		so.pDesc = &raygenRS.rootSignaturePtr;
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}

	{
		auto& so = subobjects[numSubobjects++];
		auto& as = associations[numAssociations++];

		stringBuffer[numStrings++] = entryPoint;

		as.NumExports = 1;
		as.pExports = &stringBuffer[numStrings - 1];
		as.pSubobjectToAssociate = &subobjects[numSubobjects - 2];

		so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		so.pDesc = &as;
	}

	allExports.push_back(entryPoint);

	uint32 size = getShaderBindingTableSize(rootSignatureDesc);
	tableEntrySize = max(size, tableEntrySize);

	return *this;
}

raytracing_pipeline_builder& raytracing_pipeline_builder::hitgroup(const wchar* groupName, raytracing_hit_group_desc desc, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc, uint32 associateWith)
{
	missEntryPoints.push_back(desc.miss);

	D3D12_HIT_GROUP_DESC& hitGroup = hitGroups[numHitGroups++];
	
	hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
	hitGroup.AnyHitShaderImport = desc.anyHit;
	hitGroup.ClosestHitShaderImport = desc.closestHit;
	hitGroup.HitGroupExport = groupName;
	hitGroup.IntersectionShaderImport = 0;

	{
		auto& so = subobjects[numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		so.pDesc = &hitGroup;
	}

	for (uint32 i = 0; i < 3; ++i)
	{
		const wchar* entryPoint = desc.entryPoints[i];
		D3D12_EXPORT_DESC& exp = exports[numExports++];
		exp.Name = entryPoint;
		exp.Flags = D3D12_EXPORT_FLAG_NONE;
		exp.ExportToRename = 0;

		allExports.push_back(desc.entryPoints[i]);
	}


	uint32 unassociatedEntryPoints = associate_with_all;

	if (associateWith != associate_with_nothing)
	{
		rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		raytracing_root_signature rs = createRaytracingRootSignature(rootSignatureDesc);


		rootSignatures.push_back(rs);

		{
			auto& so = subobjects[numSubobjects++];
			so.pDesc = &rs.rootSignaturePtr;
			so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
		}

		{
			auto& so = subobjects[numSubobjects++];
			auto& as = associations[numAssociations++];

			const wchar** entryPoints = &stringBuffer[numStrings];
			uint32 numEntryPoints = 0;

			DWORD index;
			while (_BitScanForward(&index, associateWith))
			{
				entryPoints[numEntryPoints++] = desc.entryPoints[index];
				unsetBit(associateWith, index);
				unsetBit(unassociatedEntryPoints, index);
			}

			numStrings += numEntryPoints;

			as.NumExports = numEntryPoints;
			as.pExports = entryPoints;
			as.pSubobjectToAssociate = &subobjects[numSubobjects - 2];

			so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
			so.pDesc = &as;
		}

		uint32 size = getShaderBindingTableSize(rootSignatureDesc);
		tableEntrySize = max(size, tableEntrySize);
	}

	{
		emptyAssociations.push_back(desc.miss);

		DWORD index;
		while (_BitScanForward(&index, unassociatedEntryPoints))
		{
			emptyAssociations.push_back(desc.entryPoints[index]);
			unsetBit(unassociatedEntryPoints, index);
		}
	}

	return *this;
}

dx_raytracing_pipeline raytracing_pipeline_builder::finish()
{
	assert(raygenRS.rootSignature.rootSignature);
	assert(globalRS.rootSignature.rootSignature);

	auto shaderBlob = compileLibrary(shaderFilename);

	D3D12_DXIL_LIBRARY_DESC dxilLibDesc;
	dxilLibDesc.DXILLibrary.pShaderBytecode = shaderBlob->GetBufferPointer();
	dxilLibDesc.DXILLibrary.BytecodeLength = shaderBlob->GetBufferSize();
	dxilLibDesc.NumExports = numExports;
	dxilLibDesc.pExports = exports;

	D3D12_ROOT_SIGNATURE_DESC emptyRootSignatureDesc = {};
	emptyRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	raytracing_root_signature emptyRootSignature = createRaytracingRootSignature(emptyRootSignatureDesc);

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;

	{
		D3D12_STATE_SUBOBJECT& so = subobjects[numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		so.pDesc = &dxilLibDesc;
	}

	{
		D3D12_STATE_SUBOBJECT& so = subobjects[numSubobjects++];
		so.pDesc = &emptyRootSignature.rootSignaturePtr;
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}

	{
		auto& so = subobjects[numSubobjects++];
		auto& as = associations[numAssociations++];

		as.NumExports = (uint32)emptyAssociations.size();
		as.pExports = emptyAssociations.data();
		as.pSubobjectToAssociate = &subobjects[numSubobjects - 2];

		so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		so.pDesc = &as;
	}

	{
		shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2; // 2 floats for the BuiltInTriangleIntersectionAttributes.
		shaderConfig.MaxPayloadSizeInBytes = payloadSize;

		auto& so = subobjects[numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		so.pDesc = &shaderConfig;
	}

	{
		auto& so = subobjects[numSubobjects++];
		auto& as = associations[numAssociations++];

		as.NumExports = (uint32)allExports.size();
		as.pExports = allExports.data();
		as.pSubobjectToAssociate = &subobjects[numSubobjects - 2];

		so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		so.pDesc = &as;
	}

	{
		pipelineConfig.MaxTraceRecursionDepth = maxRecursionDepth;

		auto& so = subobjects[numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		so.pDesc = &pipelineConfig;
	}


	D3D12_STATE_OBJECT_DESC desc;
	desc.NumSubobjects = numSubobjects;
	desc.pSubobjects = subobjects;
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	dx_raytracing_pipeline result;
	checkResult(dxContext.device->CreateStateObject(&desc, IID_PPV_ARGS(&result.pipeline)));
	result.rootSignature = globalRS.rootSignature;


	for (auto& rs : rootSignatures)
	{
		freeRootSignature(rs.rootSignature);
	}
	freeRootSignature(raygenRS.rootSignature);


	com<ID3D12StateObjectProperties> rtsoProps;
	result.pipeline->QueryInterface(IID_PPV_ARGS(&rtsoProps));

	result.raygenIdentifier = rtsoProps->GetShaderIdentifier(raygenEntryPoint);

	for (uint32 i = 0; i < numHitGroups; ++i)
	{
		result.missIdentifiers.push_back(rtsoProps->GetShaderIdentifier(missEntryPoints[i]));
		result.hitGroupIdentifiers.push_back(rtsoProps->GetShaderIdentifier(hitGroups[i].HitGroupExport));
	}


	{
		auto& shaderBindingTableDesc = result.shaderBindingTableDesc;
		tableEntrySize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		shaderBindingTableDesc.entrySize = (uint32)alignTo(tableEntrySize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		uint32 numRaygenShaderEntries = 1;
		uint32 numMissShaderEntries = numHitGroups;

		shaderBindingTableDesc.raygenOffset = 0;
		shaderBindingTableDesc.missOffset = shaderBindingTableDesc.raygenOffset + (uint32)alignTo(numRaygenShaderEntries * shaderBindingTableDesc.entrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
		shaderBindingTableDesc.hitOffset = shaderBindingTableDesc.missOffset + (uint32)alignTo(numMissShaderEntries * shaderBindingTableDesc.entrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		shaderBindingTableDesc.sizeWithoutHitEntries = shaderBindingTableDesc.hitOffset;
	}

	return result;
}



