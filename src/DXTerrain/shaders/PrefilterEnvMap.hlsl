// PrefilterEnvMap.hlsl
// -----------------------------------------------------------------
// Compute shader that pre-filters an environment cubemap for specular
// IBL using GGX importance sampling. Each mip level corresponds to a
// roughness value: mip 0 = mirror (roughness 0), last mip = fully
// rough (roughness 1).
//
// Dispatch once per mip level (1..maxMip). Mip 0 is the original
// captured cubemap and is left untouched.
// -----------------------------------------------------------------

cbuffer PrefilterCB : register(b0)
{
    float Roughness;   // Roughness for this mip level
    uint FaceSize;     // Output face resolution for this mip
    uint NumSamples;   // Importance samples (e.g. 1024)
    uint Pad0;
};

TextureCube<float4> EnvMap : register(t0);
SamplerState LinearClamp  : register(s0);

RWTexture2DArray<float4> OutMip : register(u0);

static const float PI = 3.14159265359f;

// Van der Corput radical inverse (base 2)
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX importance sampling: given random Xi, produce a half-vector H
// in tangent space for the given roughness.
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // Spherical to cartesian (tangent space)
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent-space to world-space
    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    return normalize(T * H.x + B * H.y + N * H.z);
}

// Convert thread ID + face index to a world-space direction
float3 CubeDirection(uint face, float2 uv)
{
    // Map [0,1] UV to [-1,1]
    float u = uv.x * 2.0f - 1.0f;
    float v = uv.y * 2.0f - 1.0f;

    // DX cubemap face layout (left-handed)
    float3 dir;
    switch (face)
    {
        case 0: dir = float3( 1.0f,   -v,   -u); break; // +X
        case 1: dir = float3(-1.0f,   -v,    u); break; // -X
        case 2: dir = float3(   u,  1.0f,    v); break; // +Y
        case 3: dir = float3(   u, -1.0f,   -v); break; // -Y
        case 4: dir = float3(   u,   -v,  1.0f); break; // +Z
        case 5: dir = float3(  -u,   -v, -1.0f); break; // -Z
        default: dir = float3(0, 0, 1); break;
    }
    return normalize(dir);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= FaceSize || DTid.y >= FaceSize)
        return;

    uint face = DTid.z; // 0..5

    float2 uv = (float2(DTid.xy) + 0.5f) / float(FaceSize);
    float3 N = CubeDirection(face, uv);
    float3 R = N;
    float3 V = R;

    float totalWeight = 0.0f;
    float3 prefilteredColor = 0.0f;

    for (uint i = 0u; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, N, Roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = dot(N, L);
        if (NdotL > 0.0f)
        {
            prefilteredColor += EnvMap.SampleLevel(LinearClamp, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.001f);

    OutMip[DTid] = float4(prefilteredColor, 1.0f);
}