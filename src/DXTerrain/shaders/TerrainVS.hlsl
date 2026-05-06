struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VS_Terrain(VSInput input)
{
    VSOutput output;
    output.worldPos = input.position;
    output.uv = input.texcoord; // Already full terrain/heightmap UV.
    return output;
}
