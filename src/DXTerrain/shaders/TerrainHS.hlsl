struct FrameConstants
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 lightDir;
    float4 lightColor;
    float3 camPos;
    uint _padding0;
};

struct TerrainConstants
{
    float4x4 worldMatrix;
    float maxHeight;
    float worldTexelSpacing;
    float tessFactorScale;
    float textureTilingFactor;
    float2 chunkUVOffset;
    float2 chunkUVScale;

    // Matches C++ packing:
    // x = heightmapIndex, yzw = splatIndices[0..2]
    uint4 textureIndices0;
    // x = splatIndices[3], yzw = padding
    uint4 textureIndices1;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<TerrainConstants> terrainCB : register(b1);

struct HSInput
{
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
};

struct HSOutput
{
    float3 worldPos : POSITION;
    float2 uv : TEXCOORD0;
};

struct HSPatchConstants
{
    float edgeTess[4] : SV_TessFactor;
    float insideTess[2] : SV_InsideTessFactor;
};

float EdgeTessFactor(float3 a, float3 b)
{
    float3 mid = 0.5f * (a + b);
    float d = distance(mid, frameCB.camPos);

    float maxTess = max(1.0f, 16.0f * max(terrainCB.tessFactorScale, 0.01f));
    float t = saturate((d - 64.0f) / 1024.0f);

    return lerp(maxTess, 1.0f, t);
}

HSPatchConstants PatchConstants(InputPatch<HSInput, 4> patch)
{
    HSPatchConstants output;

    float bottom = EdgeTessFactor(patch[0].worldPos, patch[1].worldPos);
    float right  = EdgeTessFactor(patch[1].worldPos, patch[2].worldPos);
    float top    = EdgeTessFactor(patch[3].worldPos, patch[2].worldPos);
    float left   = EdgeTessFactor(patch[0].worldPos, patch[3].worldPos);

    output.edgeTess[0] = left;
    output.edgeTess[1] = bottom;
    output.edgeTess[2] = right;
    output.edgeTess[3] = top;

    output.insideTess[0] = 0.5f * (left + right);
    output.insideTess[1] = 0.5f * (bottom + top);

    return output;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstants")]
HSOutput HS_Terrain(InputPatch<HSInput, 4> patch, uint i : SV_OutputControlPointID)
{
    HSOutput output;
    output.worldPos = patch[i].worldPos;
    output.uv = patch[i].uv;
    return output;
}
