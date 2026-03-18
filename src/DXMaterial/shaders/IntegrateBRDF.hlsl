// IntegrateBRDF.hlsl
// -----------------------------------------------------------------
// Compute shader that generates the BRDF integration LUT for the
// split-sum approximation of specular IBL.
//
// Output is a 2D texture indexed by (NdotV, roughness).
// R channel = scale factor for F0
// G channel = bias term (added to F0 * scale)
//
// This only needs to be computed once; the result is view-independent.
// -----------------------------------------------------------------

RWTexture2D<float2> OutBRDF : register(u0);

cbuffer BRDFIntegrationCB : register(b0)
{
    uint Resolution;   // Output texture resolution (e.g. 256)
    uint NumSamples;   // Integration samples (e.g. 1024)
    uint Pad0;
    uint Pad1;
};

static const float PI = 3.14159265359f;

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    return normalize(T * H.x + B * H.y + N * H.z);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    // Use k = (roughness^2) / 2 for IBL (different from direct lighting)
    float a = roughness;
    float k = (a * a) / 2.0f;

    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Resolution || DTid.y >= Resolution)
        return;

    float NdotV = (float(DTid.x) + 0.5f) / float(Resolution);
    float roughness = (float(DTid.y) + 0.5f) / float(Resolution);

    // Clamp NdotV away from 0 to avoid numerical issues
    NdotV = max(NdotV, 0.001f);

    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV); // sin
    V.y = 0.0f;
    V.z = NdotV; // cos

    float3 N = float3(0.0f, 0.0f, 1.0f);

    float A = 0.0f; // Scale
    float B = 0.0f; // Bias

    for (uint i = 0u; i < NumSamples; i++)
    {
        float2 Xi = Hammersley(i, NumSamples);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(NumSamples);
    B /= float(NumSamples);

    OutBRDF[DTid.xy] = float2(A, B);
}