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

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float2 hmUV : TEXCOORD1;
};

uint GetSplatIndex(uint i)
{
    if (i == 0) return terrainCB.textureIndices0.y;
    if (i == 1) return terrainCB.textureIndices0.z;
    if (i == 2) return terrainCB.textureIndices0.w;
    return terrainCB.textureIndices1.x;
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
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

    float wSand  = lowland * flatness;
    float wGrass = midland * flatness;
    float wRock  = steepness * (1.0f - highland * 0.45f);
    float wSnow  = highland * (1.0f - smoothstep(0.35f, 0.72f, slope));

    float weightSum = max(wSand + wGrass + wRock + wSnow, 0.001f);
    wSand /= weightSum;
    wGrass /= weightSum;
    wRock /= weightSum;
    wSnow /= weightSum;

    float tiling = max(terrainCB.textureTilingFactor, 1.0f);
    float2 tiledUV = input.uv * tiling;
    float2 macroUV = input.uv * 18.0f;

    Texture2D<float4> splat0 = ResourceDescriptorHeap[GetSplatIndex(0)];
    Texture2D<float4> splat1 = ResourceDescriptorHeap[GetSplatIndex(1)];
    Texture2D<float4> splat2 = ResourceDescriptorHeap[GetSplatIndex(2)];
    Texture2D<float4> splat3 = ResourceDescriptorHeap[GetSplatIndex(3)];

    float3 sandTex  = splat3.Sample(linearWrapSampler, tiledUV * 0.65f).rgb;
    float3 grassTex = splat0.Sample(linearWrapSampler, tiledUV).rgb;
    float3 rockTex  = splat1.Sample(linearWrapSampler, tiledUV * 0.85f).rgb;
    float3 snowTex  = splat2.Sample(linearWrapSampler, tiledUV * 0.55f).rgb;

    float macro = lerp(0.82f, 1.16f, Hash21(floor(macroUV)));

    float3 sandTint  = float3(0.68f, 0.58f, 0.38f);
    float3 grassTint = float3(0.18f, 0.40f, 0.15f);
    float3 rockTint  = float3(0.38f, 0.37f, 0.35f);
    float3 snowTint  = float3(0.82f, 0.87f, 0.90f);

    float3 albedo =
        wSand  * sandTex  * sandTint +
        wGrass * grassTex * grassTint +
        wRock  * rockTex  * rockTint +
        wSnow  * snowTex  * snowTint;

    albedo *= macro;

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
    float specMask = saturate(wRock * 0.35f + wSnow * 0.18f);
    float3 specular = pow(ndoth, 32.0f) * specMask * frameCB.lightColor.rgb;

    float3 color = ambient + diffuse + specular;

    color = color / (color + 1.0f);
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, 1.0f);
}
