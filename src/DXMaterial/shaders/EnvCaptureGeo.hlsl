// EnvCaptureGeom.hlsl
// -----------------------------------------------------------------
// Renders direct-lit PBR geometry into a cubemap face.
// Simplified version of GeometryPassPS — direct lighting only,
// no IBL (since this IS the cubemap we're building).
// Same vertex layout and mesh constants as the main geometry pass.
// -----------------------------------------------------------------

// =====================================================================
// Constant buffers
// =====================================================================

struct CaptureConstants
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4 lightDir;
    float4 lightColor;
    float3 capturePos;
    float _pad0;
    float3 camPos;
    float _pad1;
};

struct MeshConstants
{
    float4x4 worldMatrix;
    float3x3 normalMatrix;
    float4 baseColor;
    float metallic;
    float roughness;
    float opacity;
    uint textureFlags;
};

ConstantBuffer<CaptureConstants> captureCB : register(b0);
ConstantBuffer<MeshConstants> meshCB : register(b1);

// Material textures start at t2 (t0/t1 are sky LUTs, still bound but unused here)
Texture2D textures[28] : register(t2);
SamplerState texSampler : register(s1); // Linear wrap sampler

// Texture flag constants (same as GeometryPassPS)
static const uint TEX_FLAG_BASE_COLOR = (1 << 12);
static const uint TEX_FLAG_NORMALS = (1 << 6);
static const uint TEX_FLAG_METALNESS = (1 << 15);
static const uint TEX_FLAG_DIFFUSE_ROUGHNESS = (1 << 16);
static const uint TEX_FLAG_AMBIENT_OCCLUSION = (1 << 17);
static const uint TEX_FLAG_GLTF_METALLIC_ROUGHNESS = (1 << 27);

static const uint TEX_SLOT_BASE_COLOR = 12;
static const uint TEX_SLOT_NORMALS = 6;
static const uint TEX_SLOT_METALNESS = 15;
static const uint TEX_SLOT_DIFFUSE_ROUGHNESS = 16;
static const uint TEX_SLOT_AMBIENT_OCCLUSION = 17;
static const uint TEX_SLOT_GLTF_METALLIC_ROUGHNESS = 27;

static const float PI = 3.14159265359f;

// =====================================================================
// VS — standard transform using cubemap face view/proj
// =====================================================================

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 posCS : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float3x3 TBN : TBN_MATRIX;
};

PSInput VS_EnvGeo(VSInput input)
{
    PSInput output;

    float4 worldPos = mul(meshCB.worldMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;

    float4 viewPos = mul(captureCB.viewMatrix, worldPos);
    output.posCS = mul(captureCB.projMatrix, viewPos);

    output.normal = normalize(mul(meshCB.normalMatrix, input.normal));
    output.texcoord = input.texcoord;

    float3 T = normalize(mul(meshCB.normalMatrix, input.tangent));
    float3 B = normalize(mul(meshCB.normalMatrix, input.bitangent));
    float3 N = output.normal;
    output.TBN = float3x3(T, B, N);

    return output;
}

// =====================================================================
// PBR helpers (same as GeometryPassPS — direct lighting only)
// =====================================================================

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 SampleNormalMap(float2 texcoord, float3x3 TBN)
{
    float3 tangentNormal = textures[TEX_SLOT_NORMALS].Sample(texSampler, texcoord).xyz;
    tangentNormal = tangentNormal * 2.0f - 1.0f;
    tangentNormal.z = sqrt(1.0f - saturate(dot(tangentNormal.xy, tangentNormal.xy)));
    return normalize(mul(tangentNormal, TBN));
}

// =====================================================================
// PS — direct lighting only (Cook-Torrance BRDF)
// =====================================================================

float4 PS_EnvGeo(PSInput input) : SV_Target
{
    // --- Albedo ---
    float4 albedo = meshCB.baseColor;
    if (meshCB.textureFlags & TEX_FLAG_BASE_COLOR)
        albedo *= textures[TEX_SLOT_BASE_COLOR].Sample(texSampler, input.texcoord);

    // --- Normal ---
    float3 N = normalize(input.normal);
    if (meshCB.textureFlags & TEX_FLAG_NORMALS)
        N = SampleNormalMap(input.texcoord, input.TBN);

    // --- Material ---
    float metallic = meshCB.metallic;
    float roughness = meshCB.roughness;
    float ao = 1.0f;

    if (meshCB.textureFlags & TEX_FLAG_GLTF_METALLIC_ROUGHNESS)
    {
        float3 mr = textures[TEX_SLOT_GLTF_METALLIC_ROUGHNESS].Sample(texSampler, input.texcoord).rgb;
        ao = mr.r;
        roughness *= mr.g;
        metallic *= mr.b;
    }
    else
    {
        if (meshCB.textureFlags & TEX_FLAG_AMBIENT_OCCLUSION)
            ao = textures[TEX_SLOT_AMBIENT_OCCLUSION].Sample(texSampler, input.texcoord).r;
        if (meshCB.textureFlags & TEX_FLAG_METALNESS)
            metallic *= textures[TEX_SLOT_METALNESS].Sample(texSampler, input.texcoord).r;
        if (meshCB.textureFlags & TEX_FLAG_DIFFUSE_ROUGHNESS)
            roughness *= textures[TEX_SLOT_DIFFUSE_ROUGHNESS].Sample(texSampler, input.texcoord).g;
    }

    // Clamp roughness to avoid singularities
    roughness = max(roughness, 0.04f);

    // --- Lighting ---
    float3 V = normalize(captureCB.camPos - input.worldPos);
    float3 L = normalize(-captureCB.lightDir.xyz);
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0f);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 numerator = D * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * NdotL + 0.0001f;
    float3 specular = numerator / denominator;

    float3 kD = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kD * albedo.rgb / PI;

    float3 radiance = captureCB.lightColor.rgb * captureCB.lightColor.w;
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    // Simple ambient term (no IBL here — that's what the cubemap provides)
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo.rgb * ao;

    float3 color = ambient + Lo;

    // Store LINEAR HDR — no tone mapping or gamma
    return float4(color, meshCB.opacity * albedo.a);
}
