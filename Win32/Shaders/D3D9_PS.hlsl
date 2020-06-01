sampler2D Texture : register(s0);

float4 main(float2 texcoord : TEXCOORD) : SV_TARGET0
{
    return tex2D(Texture, texcoord);
}
