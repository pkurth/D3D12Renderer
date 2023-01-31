
#define RS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_HULL_SHADER_ROOT_ACCESS |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_PIXEL_SHADER_ROOT_ACCESS)," \
    "SRV(t0, visibility=SHADER_VISIBILITY_VERTEX),"  \
    "CBV(b0, visibility=SHADER_VISIBILITY_VERTEX)"

[RootSignature(RS)]
float4 main() : SV_TARGET
{
	return float4(1.f, 1.f, 1.f, 1.f);
}
