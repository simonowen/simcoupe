Texture2D tex : register(t2);
Texture2D texPrev : register(t3);
sampler sampLinear : register(s0);

cbuffer gConstants : register(b0)
{
    float blend_factor;
    float3 unused;
};

struct VS_Output
{
    float4 pos : SV_POSITION;	// unused
    float2 coord0 : TEXCOORD0;
};

float4 main(VS_Output input) : SV_Target
{
    float3 colour = tex.Sample(sampLinear, input.coord0).rgb;
    float3 prev_colour = texPrev.Sample(sampLinear, input.coord0).rgb * blend_factor;
    float3 mix = max(colour, prev_colour);

    return float4(mix, 1.0f);
}
