cbuffer Constants : register(b0)
{
    float2 scale_target;
};

struct VS_Output
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VS_Output main (uint id : SV_VertexID)
{
    VS_Output output;

    float2 pos = float2(id / 2, id % 2);

    output.pos.xy = pos * float2(2.0f, 2.0f) * scale_target;
    output.pos.xy -= float2(1.0f, 1.0f) * scale_target;
    output.pos.zw = float2(0.0f, 1.0f);
    output.uv = float2(pos.x, 1.0f - pos.y);

    return output;
}
