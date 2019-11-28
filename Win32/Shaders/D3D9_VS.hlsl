float4 scaleConsts : register(c0);
float4 texConsts : register(c1);

struct VertexShaderInput
{
	float2 pos : POSITION0;
};

struct VertexShaderOutput
{
	float4 pos : POSITION0;
	float2 coord0 : TEXCOORD0;
	float2 coord1 : TEXCOORD1;
};


VertexShaderOutput main (VertexShaderInput input)
{
	VertexShaderOutput output;

	float2 pos = input.pos;
	pos.xy *= scaleConsts.xy;
	pos.xy += float2(-1.0, 1.0);

	output.pos = float4(pos, 0.0f, 1.0f);

	output.coord0 = input.pos += texConsts.xx;
	output.coord0 *= texConsts.yy;

	output.coord1 = input.pos * scaleConsts.zw;

	return output;
}
