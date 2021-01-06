

struct ps_input
{
	float clipDepth		: CLIP_DEPTH;
};

void main(ps_input IN)
{
	clip(IN.clipDepth);
}
