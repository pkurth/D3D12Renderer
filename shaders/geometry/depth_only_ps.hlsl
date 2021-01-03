
// This shader outputs the screen space velocities.


struct ps_input
{
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
};

float2 main(ps_input IN) : SV_Target0
{
	float2 screenUV = (IN.ndc.xy / IN.ndc.z) * 0.5f + 0.5f;
	float2 prevFrameScreenUV = (IN.prevFrameNDC.xy / IN.prevFrameNDC.z) * 0.5f + 0.5f;

	return screenUV - prevFrameScreenUV;
}
