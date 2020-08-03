Texture2D texBlended : register(t2);
sampler sampLinear : register(s0);

struct VS_Output
{
    float4 pos : SV_POSITION;	// unused
    float2 uv : TEXCOORD;
};

float4 main(VS_Output input) : SV_Target
{
    return texBlended.Sample(sampLinear, input.uv);
}
