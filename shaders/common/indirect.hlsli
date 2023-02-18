#ifndef INDIRECT_HLSLI
#define INDIRECT_HLSLI

#ifdef HLSL
struct D3D12_DISPATCH_ARGUMENTS
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
};

struct D3D12_DRAW_ARGUMENTS
{
	uint32 VertexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartVertexLocation;
	uint32 StartInstanceLocation;
};

struct D3D12_DRAW_INDEXED_ARGUMENTS
{
	uint32 IndexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartIndexLocation;
	int BaseVertexLocation;
	uint32 StartInstanceLocation;
};

#endif

#endif
