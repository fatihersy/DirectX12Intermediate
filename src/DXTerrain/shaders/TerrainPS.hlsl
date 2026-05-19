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
    float worldTexelSpacingX;
    float worldTexelSpacingZ;
    float tessFactorScale;
    float textureTilingFactor;
    uint3 PADDING_0;
    float2 chunkUVOffset;
    float2 chunkUVScale;
    uint heightmapSrvIndex;
    uint terrainDiffuseSrvIndex;
    uint2 PADDING_1;
    uint4 splatSrvIndices;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<TerrainConstants> terrainCB : register(b1);

SamplerState linearWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float2 hmUV : TEXCOORD1;
};

float3 ApplyBasicTerrainGrade(float3 color)
{
    const float exposure = 0.72f;
    const float contrast = 1.35f;
    const float saturation = 1.08f;
    const float3 contrastPivot = float3(0.18f, 0.18f, 0.18f);

    color *= exposure;
    color = max((color - contrastPivot) * contrast + contrastPivot, 0.0f);

    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    return max(lerp(float3(luminance, luminance, luminance), color, saturation), 0.0f);
}
float3 GreenCorrection(float3 color)
{
    float greenMask = saturate((color.g - max(color.r, color.b)) * 4.0f);
    greenMask = smoothstep(0.00f, 0.12f, greenMask);

    float3 correctedGreen = color * float3(0.62f, 0.66f, 0.50f);

    return lerp(color, correctedGreen, greenMask);
}

float4 PS_Terrain(PSInput input) : SV_TARGET
{
    float3 N = normalize(input.normal);
    float height01 = saturate(input.worldPos.y / max(terrainCB.maxHeight, 0.001f));
    float slope = saturate(1.0f - abs(N.y));

    float flatness = 1.0f - smoothstep(0.18f, 0.62f, slope);
    float steepness = smoothstep(0.25f, 0.68f, slope);

    float lowland = 1.0f - smoothstep(0.18f, 0.36f, height01);
    float midland = smoothstep(0.22f, 0.38f, height01) * (1.0f - smoothstep(0.52f, 0.74f, height01));
    float highland = smoothstep(0.56f, 0.82f, height01);

    float wDirt  = lowland * flatness;
    float wGrass = midland * flatness;
    float wRock  = steepness * (1.0f - highland * 0.45f);
    float wSnow  = highland * (1.0f - smoothstep(0.35f, 0.72f, slope));

    float weightSum = max(wDirt + wGrass + wRock + wSnow, 0.001f);
    wDirt /= weightSum;
    wGrass /= weightSum;
    wRock /= weightSum;
    wSnow /= weightSum;

    float tiling = max(terrainCB.textureTilingFactor, 1.0f);
    float2 tiledUV = input.uv * tiling;

    Texture2D<float4> grassMap = ResourceDescriptorHeap[terrainCB.splatSrvIndices.x];
    Texture2D<float4> rockMap  = ResourceDescriptorHeap[terrainCB.splatSrvIndices.y];
    Texture2D<float4> snowMap  = ResourceDescriptorHeap[terrainCB.splatSrvIndices.z];
    Texture2D<float4> dirtMap  = ResourceDescriptorHeap[terrainCB.splatSrvIndices.w];
    Texture2D<float4> terrainDiffuseMap = ResourceDescriptorHeap[terrainCB.terrainDiffuseSrvIndex];

    float3 grassTex = grassMap.Sample(linearWrapSampler, tiledUV).rgb;
    float3 rockTex  = rockMap.Sample(linearWrapSampler, tiledUV * 0.85f).rgb;
    float3 snowTex  = snowMap.Sample(linearWrapSampler, tiledUV * 0.55f).rgb;
    float3 dirtTex  = dirtMap.Sample(linearWrapSampler, tiledUV * 0.65f).rgb;
    float3 terrainDiffuseTex = terrainDiffuseMap.Sample(linearClampSampler, input.hmUV).rgb;
    terrainDiffuseTex = GreenCorrection(terrainDiffuseTex);
    terrainDiffuseTex = ApplyBasicTerrainGrade(terrainDiffuseTex);

    float3 grassTint = float3(0.18f, 0.40f, 0.15f);
    float3 rockTint  = float3(0.38f, 0.37f, 0.35f);
    float3 snowTint  = float3(0.82f, 0.87f, 0.90f);
    float3 dirtTint  = float3(0.68f, 0.58f, 0.38f);

    float3 albedo =
        wDirt  * lerp(terrainDiffuseTex, dirtTex  * dirtTint,  0.00f) +
        wGrass * lerp(terrainDiffuseTex, grassTex * grassTint, 0.00f) +
        wRock  * lerp(terrainDiffuseTex, rockTex  * rockTint,  0.00f) +
        wSnow  * lerp(terrainDiffuseTex, snowTex  * snowTint,  0.00f);

    float3 L = normalize(-frameCB.lightDir.xyz);
    float3 V = normalize(frameCB.camPos - input.worldPos);
    float3 H = normalize(L + V);

    float ndotl = saturate(dot(N, L));
    float ndoth = saturate(dot(N, H));

    float hemi = saturate(N.y * 0.5f + 0.5f);
    float3 skyAmbient = float3(0.28f, 0.32f, 0.36f);
    float3 groundAmbient = float3(0.055f, 0.050f, 0.045f);
    float3 ambient = albedo * lerp(groundAmbient, skyAmbient, hemi);

    float3 diffuse = ndotl * albedo * frameCB.lightColor.rgb * max(frameCB.lightColor.a, 1.0f);
    float specMask = saturate(wRock * 0.05f + wSnow * 0.18f);
    float3 specular = pow(ndoth, 32.0f) * specMask * frameCB.lightColor.rgb;

    float3 color = ambient + diffuse + specular;

    color = color / (color + 1.0f);
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, 1.0f);
}
