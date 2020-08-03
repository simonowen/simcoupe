Texture1D texPalette : register(t0);
Texture2D texSamScreen : register(t1);
sampler sampPoint : register(s1);

struct VS_Output
{
    float4 pos : SV_POSITION;	// unused
    float2 uv : TEXCOORD;
};

float4 main(VS_Output input) : SV_TARGET
{
    return texPalette.Sample(sampPoint, texSamScreen.Sample(sampPoint, input.uv).r * 2.0f);
}
