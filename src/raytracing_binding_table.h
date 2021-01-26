#include "raytracing.h"

template <typename shader_data>
struct raytracing_binding_table
{
    void initialize(dx_raytracing_pipeline* pipeline);

    // This expects an array of length numRayTypes, i.e. shader data for all hit groups.
    void push(const shader_data* sd);

    void build();

    ref<dx_buffer> getBuffer() { return bindingTableBuffer; }
    uint32 getNumberOfHitGroups() { return currentHitGroup; }

private:

    struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) binding_table_entry
    {
        uint8 identifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

        shader_data shaderData;
    };

    void allocate();

    void* bindingTable = 0;
    uint32 totalBindingTableSize;

    uint32 maxNumHitGroups;
    uint32 currentHitGroup = 0;

    uint32 numRayTypes;

    binding_table_entry* raygen;
    binding_table_entry* miss;
    binding_table_entry* hit;

    dx_raytracing_pipeline* pipeline;

    ref<dx_buffer> bindingTableBuffer;
};


template<typename shader_data>
inline void raytracing_binding_table<shader_data>::initialize(dx_raytracing_pipeline* pipeline)
{
    this->pipeline = pipeline;

    assert(pipeline->shaderBindingTableDesc.entrySize == sizeof(binding_table_entry));

    numRayTypes = (uint32)pipeline->shaderBindingTableDesc.hitGroups.size();
    maxNumHitGroups = 1024;
    allocate();

    memcpy(raygen->identifier, pipeline->shaderBindingTableDesc.raygen, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    for (uint32 i = 0; i < numRayTypes; ++i)
    {
        binding_table_entry* m = miss + i;
        memcpy(m->identifier, pipeline->shaderBindingTableDesc.miss[i], D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    }
}

template<typename shader_data>
inline void raytracing_binding_table<shader_data>::allocate()
{
    totalBindingTableSize = (uint32)
        (alignTo(sizeof(binding_table_entry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT)
            + alignTo(sizeof(binding_table_entry) * numRayTypes * maxNumHitGroups, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    if (!bindingTable)
    {
        bindingTable = _aligned_malloc(totalBindingTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    }
    else
    {
        bindingTable = _aligned_realloc(bindingTable, totalBindingTableSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    }

    assert(pipeline->shaderBindingTableDesc.raygenOffset == 0);
    assert(pipeline->shaderBindingTableDesc.missOffset == alignTo(sizeof(binding_table_entry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));
    assert(pipeline->shaderBindingTableDesc.hitOffset == alignTo((1 + numRayTypes) * sizeof(binding_table_entry), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT));

    raygen = (binding_table_entry*)bindingTable;
    miss = (binding_table_entry*)alignTo(raygen + 1, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    hit = (binding_table_entry*)alignTo(miss + numRayTypes, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
}

template<typename shader_data>
inline void raytracing_binding_table<shader_data>::push(const shader_data* sd)
{
    if (currentHitGroup >= maxNumHitGroups)
    {
        maxNumHitGroups *= 2;
        allocate();
    }

    binding_table_entry* base = hit + (numRayTypes * currentHitGroup);
    ++currentHitGroup;

    for (uint32 i = 0; i < numRayTypes; ++i)
    {
        binding_table_entry* h = base + i;

        memcpy(h->identifier, pipeline->shaderBindingTableDesc.hitGroups[i].mesh, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        h->shaderData = sd[i];
    }
}

template<typename shader_data>
inline void raytracing_binding_table<shader_data>::build()
{
    bindingTableBuffer = createBuffer(1, totalBindingTableSize, bindingTable);
}
