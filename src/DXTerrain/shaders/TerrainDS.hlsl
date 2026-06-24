#ifndef DEPTH_PREPASS
    struct FrameConstants
    {
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float4 lightDir;
        float4 lightColor;
        float3 camPos;
        uint PADDING_0;
    };
#else
    struct FrameConstants
    {
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float3 camPos;
        uint _padding0;
    };
#endif

struct TerrainConstants
{
    float4x4 worldMatrix;
    float maxHeight;
    float worldTexelSpacingX;
    float worldTexelSpacingZ;
    float tessFactorScale;
    float textureTilingFactor;
    uint3 PADDING_0;
    float2 chunkUVOffset;
    float2 chunkUVScale;
    uint heightmapSrvIndex;
    uint terrainDiffuseSrvIndex;
    uint pageHalo;
    uint PADDING_1;
    uint4 splatSrvIndices;
    uint impostorHeightmapSrvIndex;
    uint impostorDiffuseSrvIndex;
    float morphNear;
    float morphFar;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<TerrainConstants> terrainCB : register(b1);

#ifndef DEPTH_PREPASS
    SamplerState linearWrapSampler : register(s0);
#endif
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

    #ifndef DEPTH_PREPASS
        float3 worldPos : WORLD_POSITION;
        float3 normal : NORMAL;
        float2 uv : TEXCOORD0;
        float2 hmUV : TEXCOORD1;
    #endif
};

float SampleHeight(Texture2D<float> heightmap, float2 uv)
{
    return heightmap.SampleLevel(linearClampSampler, saturate(uv), 0).r * terrainCB.maxHeight;
}

[domain("quad")]
DSOutput DS_Terrain(HSPatchConstants patchConstants, float2 domainUV : SV_DomainLocation, const OutputPatch<HSOutput, 4> patch)
{
    DSOutput output;

    float3 posBottom = lerp(patch[0].worldPos, patch[1].worldPos, domainUV.x);
    float3 posTop    = lerp(patch[3].worldPos, patch[2].worldPos, domainUV.x);
    float3 worldPos  = lerp(posBottom, posTop, domainUV.y);

    float2 uvBottom = lerp(patch[0].uv, patch[1].uv, domainUV.x);
    float2 uvTop    = lerp(patch[3].uv, patch[2].uv, domainUV.x);
    float2 uv       = lerp(uvBottom, uvTop, domainUV.y);

    Texture2D<float> heightmap = ResourceDescriptorHeap[terrainCB.heightmapSrvIndex];

    // Page-local position in [0,1] across the page's core (non-halo) region.
    float2 pageLocal = (uv - terrainCB.chunkUVOffset) / terrainCB.chunkUVScale;

    uint hmWidth;
    uint hmHeight;
    heightmap.GetDimensions(hmWidth, hmHeight);

    // Map the page-local position into the haloed heightmap. The heightmap core is
    // grid-centred (first/last core texels sit on the page edges), offset inward by
    // the halo gutter so border taps can reach real cross-page neighbours.
    float halo = (float)terrainCB.pageHalo;
    float2 coreSpan = float2(hmWidth, hmHeight) - 2.0f * halo - 1.0f; // (core texels - 1)
    float2 hmUV = (halo + pageLocal * coreSpan + 0.5f) / float2(hmWidth, hmHeight);

    // Distance-driven impostor->page morph. Blend the page surface toward the whole-map
    // impostor (sampled at the same global uv) so far detail rises out of it with no pop.
    // Crack-free: a pure function of world position + camera, so adjacent pages always agree
    // at shared edges. Runs in both the main and depth-prepass variants so depth stays valid.
    Texture2D<float> impostorHM = ResourceDescriptorHeap[terrainCB.impostorHeightmapSrvIndex];
    float wImp = smoothstep(terrainCB.morphNear, terrainCB.morphFar, distance(worldPos, frameCB.camPos));
    float hImp = impostorHM.SampleLevel(linearClampSampler, saturate(uv), 0).r * terrainCB.maxHeight;
    worldPos.y = lerp(worldPos.y, hImp, wImp);

    #ifndef DEPTH_PREPASS
        // One full heightmap texel; thanks to the halo gutter these neighbour taps read
        // real cross-page data at borders instead of clamping onto the page's own edge.
        float2 texelUV = 1.0f / float2(hmWidth, hmHeight);

        float hL = SampleHeight(heightmap, hmUV + float2(-texelUV.x, 0.0f));
        float hR = SampleHeight(heightmap, hmUV + float2( texelUV.x, 0.0f));
        float hD = SampleHeight(heightmap, hmUV + float2(0.0f, -texelUV.y));
        float hU = SampleHeight(heightmap, hmUV + float2(0.0f,  texelUV.y));

        float spacingX = max(terrainCB.worldTexelSpacingX, 0.0001f);
        float spacingZ = max(terrainCB.worldTexelSpacingZ, 0.0001f);

        float3 normal = normalize(float3(
            (hL - hR) / (2.0f * spacingX),
            1.0f,
            (hD - hU) / (2.0f * spacingZ)
        ));

        // Morph the normal toward the impostor's so lighting doesn't pop as the height blends.
        // Same spacing as the page (the impostor shares the worldTexel formula).
        uint imW;
        uint imH;
        impostorHM.GetDimensions(imW, imH);
        float2 imTexel = 1.0f / float2(imW, imH);

        float ihL = impostorHM.SampleLevel(linearClampSampler, saturate(uv + float2(-imTexel.x, 0.0f)), 0).r * terrainCB.maxHeight;
        float ihR = impostorHM.SampleLevel(linearClampSampler, saturate(uv + float2( imTexel.x, 0.0f)), 0).r * terrainCB.maxHeight;
        float ihD = impostorHM.SampleLevel(linearClampSampler, saturate(uv + float2(0.0f, -imTexel.y)), 0).r * terrainCB.maxHeight;
        float ihU = impostorHM.SampleLevel(linearClampSampler, saturate(uv + float2(0.0f,  imTexel.y)), 0).r * terrainCB.maxHeight;

        float3 impNormal = normalize(float3(
            (ihL - ihR) / (2.0f * spacingX),
            1.0f,
            (ihD - ihU) / (2.0f * spacingZ)
        ));
        normal = normalize(lerp(normal, impNormal, wImp));
    #endif

    float4 viewPos = mul(frameCB.viewMatrix, float4(worldPos, 1.0f));

    output.position = mul(frameCB.projectionMatrix, viewPos);

    #ifndef DEPTH_PREPASS
        output.worldPos = worldPos;
        output.normal = normal;
        output.uv = uv;
        output.hmUV = pageLocal;
    #endif

    return output;
}
