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

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float SampleH(Texture2D<float> heightmap, float2 uv)
{
    return heightmap.SampleLevel(linearClampSampler, saturate(uv), 0).r * impostorCB.maxHeight;
}

VSOutput VS_Impostor(VSInput input)
{
    VSOutput output;

    Texture2D<float> heightmap = ResourceDescriptorHeap[impostorCB.heightmapSrvIndex];

    float2 uv = input.uv;
    float3 worldPos = input.position;
    worldPos.y = SampleH(heightmap, uv);

    if (impostorCB.streamValid != 0u)
    {
        // Penetration: signed distance into the resident region (>0 inside, distance to
        // the nearest edge). The detail pages cover this rectangle exactly.
        float penX = min(worldPos.x - impostorCB.streamMin.x, impostorCB.streamMax.x - worldPos.x);
        float penZ = min(worldPos.z - impostorCB.streamMin.y, impostorCB.streamMax.y - worldPos.z);
        float pen = min(penX, penZ);

        // Deep interior is fully covered by detail -> remove the impostor there by
        // collapsing to NaN (any triangle that references it is discarded).
        if (pen > impostorCB.cullMargin)
        {
            output.position = asfloat(uint4(0x7FC00000u, 0x7FC00000u, 0x7FC00000u, 0x7FC00000u));
            output.worldPos = worldPos;
            output.normal = float3(0.0f, 1.0f, 0.0f);
            output.uv = uv;
            return output;
        }

        // Boundary band: sink the impostor below the detail surface so the detailed pages
        // occlude it (early-Z rejects those fragments), backstopping the seam instead of a
        // gap. Sink ramps from zero just outside the boundary up to the cull distance.
        float sink = max(pen + impostorCB.sinkStart, 0.0f) * impostorCB.sinkRate;
        worldPos.y -= min(sink, impostorCB.maxSink);
    }

    uint hmWidth;
    uint hmHeight;
    heightmap.GetDimensions(hmWidth, hmHeight);
    float2 texelUV = 1.0f / float2(hmWidth, hmHeight);

    float hL = SampleH(heightmap, uv + float2(-texelUV.x, 0.0f));
    float hR = SampleH(heightmap, uv + float2( texelUV.x, 0.0f));
    float hD = SampleH(heightmap, uv + float2(0.0f, -texelUV.y));
    float hU = SampleH(heightmap, uv + float2(0.0f,  texelUV.y));

    float spacingX = max(impostorCB.worldTexelSpacingX, 0.0001f);
    float spacingZ = max(impostorCB.worldTexelSpacingZ, 0.0001f);

    float3 normal = normalize(float3(
        (hL - hR) / (2.0f * spacingX),
        1.0f,
        (hD - hU) / (2.0f * spacingZ)
    ));

    float4 viewPos = mul(frameCB.viewMatrix, float4(worldPos, 1.0f));

    output.position = mul(frameCB.projectionMatrix, viewPos);
    output.worldPos = worldPos;
    output.normal = normal;
    output.uv = uv;

    return output;
}
