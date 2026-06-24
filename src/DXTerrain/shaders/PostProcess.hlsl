// Full-screen post-process: atmosphere-LUT aerial-perspective fog + tonemap, resolving the linear-HDR
// scene-color target to the LDR backbuffer.

#include "Atmosphere.hlsli" // AtmosCB (b2) + AerialPerspective + LUT sampling

cbuffer PostConstants : register(b0)
{
    float4x4 invViewProj;
    float3 camPos;
    uint sceneColorSrvIndex;
    uint depthSrvIndex;
    uint transmittanceSrvIndex;
    uint scatteringSrvIndex;
    uint PADDING_0;
};

SamplerState linearClampSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// Fullscreen triangle from the vertex id; no vertex buffer.
VSOutput VS_FullScreen(uint id : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PS_Post(VSOutput input) : SV_TARGET
{
    Texture2D<float4> sceneColorTex = ResourceDescriptorHeap[sceneColorSrvIndex];
    float3 color = sceneColorTex.Sample(linearClampSampler, input.uv).rgb;

    Texture2D<float> depthTex = ResourceDescriptorHeap[depthSrvIndex];
    float depth = depthTex.Load(int3(input.position.xy, 0));

    // Reverse-Z: cleared to 0 = far/background. Geometry writes depth > 0. Sky pixels keep 0 and are
    // already the atmosphere, so they get tonemap only (no double-counted fog).
    if (depth > 0.0f)
    {
        // Reconstruct world position from depth.
        float2 ndc = input.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
        float4 clip = float4(ndc, depth, 1.0f);
        float4 world = mul(invViewProj, clip);
        world /= world.w;

        Texture2D<float4> transLUT = ResourceDescriptorHeap[transmittanceSrvIndex];
        Texture3D<float4> scatterLUT = ResourceDescriptorHeap[scatteringSrvIndex];

        color = AerialPerspective(color, camPos, world.xyz, normalize(SunDir), transLUT, scatterLUT, linearClampSampler);
    }

    // Unified tonemap (relocated here from the surface/sky shaders).
    color = color / (color + 1.0f);
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, 1.0f);
}
