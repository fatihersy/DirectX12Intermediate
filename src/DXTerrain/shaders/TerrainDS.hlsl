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
    uint4 textureIndices0;
    uint4 textureIndices1;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<TerrainConstants> terrainCB : register(b1);

SamplerState linearWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

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

struct DSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float2 hmUV : TEXCOORD1;
};

float SampleHeight(Texture2D<float> heightmap, float2 uv)
{
    return heightmap.SampleLevel(linearClampSampler, saturate(uv), 0).r * terrainCB.maxHeight;
}

[domain("quad")]
DSOutput DS_Terrain(
    HSPatchConstants patchConstants,
    float2 domainUV : SV_DomainLocation,
    const OutputPatch<HSOutput, 4> patch
)
{
    DSOutput output;

    float3 posBottom = lerp(patch[0].worldPos, patch[1].worldPos, domainUV.x);
    float3 posTop    = lerp(patch[3].worldPos, patch[2].worldPos, domainUV.x);
    float3 worldPos  = lerp(posBottom, posTop, domainUV.y);

    float2 uvBottom = lerp(patch[0].uv, patch[1].uv, domainUV.x);
    float2 uvTop    = lerp(patch[3].uv, patch[2].uv, domainUV.x);
    float2 uv       = lerp(uvBottom, uvTop, domainUV.y);

    Texture2D<float> heightmap = ResourceDescriptorHeap[terrainCB.textureIndices0.x];

    float2 hmUV = uv; // Vertex UV is already full heightmap UV.
    worldPos.y = SampleHeight(heightmap, hmUV);

    uint hmWidth;
    uint hmHeight;
    heightmap.GetDimensions(hmWidth, hmHeight);

    float2 texelUV = 1.0f / float2(max(hmWidth - 1u, 1u), max(hmHeight - 1u, 1u));

    float hL = SampleHeight(heightmap, hmUV + float2(-texelUV.x, 0.0f));
    float hR = SampleHeight(heightmap, hmUV + float2( texelUV.x, 0.0f));
    float hD = SampleHeight(heightmap, hmUV + float2(0.0f, -texelUV.y));
    float hU = SampleHeight(heightmap, hmUV + float2(0.0f,  texelUV.y));

    float3 normal = normalize(float3(
        hL - hR,
        2.0f * terrainCB.worldTexelSpacing,
        hD - hU
    ));

    float4 viewPos = mul(frameCB.viewMatrix, float4(worldPos, 1.0f));

    output.position = mul(frameCB.projectionMatrix, viewPos);
    output.worldPos = worldPos;
    output.normal = normal;
    output.uv = uv;
    output.hmUV = hmUV;

    return output;
}
