sampler2D Texture : register(s0);
float4 consts0 : register(c0);

struct PixelShaderInput
{
	float2 coord0 : TEXCOORD0;
	float2 coord1 : TEXCOORD1;
};

struct PixelShaderOutput
{
    float4 colour : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	output.colour = tex2D(Texture, input.coord0);

	float level = clamp(round(frac(input.coord1.y)) + consts0.x, 0.0f, 1.0f);
	output.colour.rgb *= level;

	return output;
}
