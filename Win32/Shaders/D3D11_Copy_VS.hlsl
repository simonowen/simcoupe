struct VS_Output
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VS_Output main (uint id : SV_VertexID)
{
    VS_Output output;

    output.uv = float2(id / 2, id % 2);
    output.pos.xy = output.uv * float2(2.0f, 2.0f) + float2(-1.0f, -1.0f);
    output.pos.zw = float2(0.0f, 1.0f);
    output.uv.y = 1.0f - output.uv.y;

    return output;
}
