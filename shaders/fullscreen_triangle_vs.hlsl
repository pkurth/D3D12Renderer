
struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	float x = float((IN.vertexID & 1) << 2) - 1.f;
	float y = 1.f - float((IN.vertexID & 2) << 1);
	float u = x * 0.5f + 0.5f;
	float v = 1.f - (y * 0.5f + 0.5f);
	OUT.position = float4(x, y, 0.f, 1.f);
	OUT.uv = float2(u, v);

	return OUT;
}
