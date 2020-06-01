float4 consts : register(c0);

struct VertexShaderInput
{
    float2 pos : POSITION0;
};

struct VertexShaderOutput
{
    float4 pos : POSITION0;
    float2 coord : TEXCOORD;
};


VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float2 pos = input.pos;
    pos.xy *= consts.xy;
    pos.xy += float2(-1.0, 1.0);

    output.pos = float4(pos, 0.0f, 1.0f);

    output.coord = input.pos += consts.zz;
    output.coord *= consts.ww;

    return output;
}
