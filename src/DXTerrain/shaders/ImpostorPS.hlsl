struct FrameConstants
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 lightDir;
    float4 lightColor;
    float3 camPos;
    uint PADDING_0;
};

struct ImpostorConstants
{
    float4x4 worldMatrix;
    float maxHeight;
    float worldTexelSpacingX;
    float worldTexelSpacingZ;
    float cullMargin;
    float2 streamMin;
    float2 streamMax;
    uint heightmapSrvIndex;
    uint diffuseSrvIndex;
    uint streamValid;
    float sinkStart;
    float sinkRate;
    float maxSink;
    float PADDING_0;
    float PADDING_1;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<ImpostorConstants> impostorCB : register(b1);

SamplerState linearWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

// Color correction as the same with TerrainPS
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

float4 PS_Impostor(PSInput input) : SV_TARGET
{
    Texture2D<float4> diffuseMap = ResourceDescriptorHeap[impostorCB.diffuseSrvIndex];

    float3 albedo = diffuseMap.Sample(linearClampSampler, input.uv).rgb;
    albedo = GreenCorrection(albedo);
    albedo = ApplyBasicTerrainGrade(albedo);

    float3 N = normalize(input.normal);
    float3 L = normalize(-frameCB.lightDir.xyz);
    float ndotl = saturate(dot(N, L));

    float hemi = saturate(N.y * 0.5f + 0.5f);
    float3 skyAmbient = float3(0.28f, 0.32f, 0.36f);
    float3 groundAmbient = float3(0.055f, 0.050f, 0.045f);
    float3 ambient = albedo * lerp(groundAmbient, skyAmbient, hemi);

    float3 diffuse = ndotl * albedo * frameCB.lightColor.rgb * max(frameCB.lightColor.a, 1.0f);

    float3 color = ambient + diffuse;

    color = color / (color + 1.0f);
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, 1.0f);
}
