// Pixel shader for PBR metallic-roughness rendering.
// Supports normal mapping, texture flags for conditional sampling,
// and a single directional light. Outputs color with alpha for blending.

struct PSInput
{
    float4 position : SV_POSITION; // Clip-space position (post-projection).
    float3 worldPos : WORLD_POSITION; // World-space position for view/light calcs.
    float3 normal : NORMAL; // World-space normal (from VS).
    float2 texcoord : TEXCOORD0; // UV coordinates for texture sampling.
    float3x3 TBN : TBN_MATRIX; // Tangent-Bitangent-Normal matrix for normal mapping.
};

struct FrameConstants
{
    float4x4 viewMatrix; // View matrix (world -> view).
    float4x4 projectionMatrix; // Projection matrix (unused in PS, for completeness).
    float4 lightDir; // Directional light direction (world-space, points toward light; negate for incoming).
    float4 lightColor; // Light color/intensity (RGB + alpha as intensity).
    float3 camPos;
    uint _padding1;
};

struct MeshConstants
{
    float4x4 worldMatrix; // Local -> world transform.
    float3x3 normalMatrix; // Inverse-transpose of worldMatrix (for normals/TBN; ADD THIS IN C++).
    float4 baseColor; // Albedo tint.
    float metallic; // Metallic factor.
    float roughness; // Roughness factor.
    float opacity; // Opacity.
    uint textureFlags; // Bitfield for textures.
    // Note: Update sizeof(meshConstants) in C++ to ~144 bytes (add 12 for float3x3).
};

ConstantBuffer<FrameConstants> frameCB : register(b0); // Per-frame constants.
ConstantBuffer<MeshConstants> meshCB : register(b1); // Per-mesh constants.

Texture2D textures[28] : register(t0); // Texture array: slot = FTextureType enum value.
SamplerState texSampler : register(s0); // Linear sampler for textures.

// Texture flag constants (match FTextureType enum bits).
static const uint TEX_FLAG_NONE                    = ( 1 << 0 );
static const uint TEX_FLAG_DIFFUSE                 = ( 1 << 1 );
static const uint TEX_FLAG_NORMALS                 = ( 1 << 6 );
static const uint TEX_FLAG_BASE_COLOR              = ( 1 << 12);
static const uint TEX_FLAG_NORMAL_CAMERA           = ( 1 << 13);
static const uint TEX_FLAG_METALNESS               = ( 1 << 15);
static const uint TEX_FLAG_DIFFUSE_ROUGHNESS       = ( 1 << 16);
static const uint TEX_FLAG_AMBIENT_OCCLUSION       = ( 1 << 17);
static const uint TEX_FLAG_GLTF_METALLIC_ROUGHNESS = ( 1 << 27);

// Texture flag constants (match FTextureType enum bits).
static const uint TEX_SLOT_NONE                    = 0;
static const uint TEX_SLOT_DIFFUSE                 = 1;
static const uint TEX_SLOT_NORMALS                 = 6;
static const uint TEX_SLOT_BASE_COLOR              = 12;
static const uint TEX_SLOT_NORMAL_CAMERA           = 13;
static const uint TEX_SLOT_METALNESS               = 15;
static const uint TEX_SLOT_DIFFUSE_ROUGHNESS       = 16;
static const uint TEX_SLOT_AMBIENT_OCCLUSION       = 17;
static const uint TEX_SLOT_GLTF_METALLIC_ROUGHNESS = 27;

// Math constants.
static const float PI = 3.14159265359f;
static const float3 F0_DIELECTRIC = float3(0.04f, 0.04f, 0.04f); // Base reflectance for non-metals.

// Normal Distribution Function (GGX/Trowbridge-Reitz): Microfacet normal alignment.
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001f); // Avoid divide-by-zero.
}

// Geometry Function (Schlick-GGX): Visibility term (shadowing/masking of microfacets).
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f; // Remapped roughness for direct lighting.

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / max(denom, 0.0000001f);
}

// Combined Geometry (Smith): GGX for both view and light directions.
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel (Schlick approx.): Reflectance at grazing angles.
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// Sample and unpack normal map (assumes directX format: RG = tangent normal, B=1).
float3 SampleNormalMap(float2 uv, float3x3 TBN)
{
    // Sample normal texture (slot 6 = NORMALS).
    float3 tangentNormal = textures[TEX_SLOT_NORMALS].Sample(texSampler, uv).xyz;

    // Unpack from [0,1] to [-1,1] (DirectX normal maps store X/Y in RG, Z reconstructed as sqrt(1 - X^2 - Y^2) but here assume pre-unpacked).
    tangentNormal = tangentNormal * 2.0f - 1.0f;
    tangentNormal.z = sqrt(1.0f - saturate(dot(tangentNormal.xy, tangentNormal.xy))); // Reconstruct Z for accuracy.

    // Transform to world space via TBN.
    return normalize(mul(tangentNormal, TBN));
}

// ===========================
// MAIN PIXEL SHADER
// ===========================
float4 mainPS(PSInput input) : SV_TARGET
{
    // === BASE COLOR (ALBEDO) ===
    float4 albedo = meshCB.baseColor; // Default to constant color.
    if (meshCB.textureFlags & TEX_FLAG_BASE_COLOR)
    {
        // Sample base color texture (slot 12) and multiply with constant (allows tinting).
        albedo *= textures[TEX_SLOT_BASE_COLOR].Sample(texSampler, input.texcoord);
    }
    // Note: For pure texture override, use: albedo = textures[...].Sample(...);

    // === NORMAL SAMPLING ===
    float3 N = normalize(input.normal); // Fallback to interpolated vertex normal.
    if (meshCB.textureFlags & TEX_FLAG_NORMALS)
    {
        N = SampleNormalMap(input.texcoord, input.TBN); // Use normal map if present.
    }
    // Optional: Support TEX_NORMAL_CAMERA (slot 13) for object-space normals if needed.

    // === MATERIAL PROPERTIES (METALLIC, ROUGHNESS, AO) ===
    float metallic = meshCB.metallic;
    float roughness = meshCB.roughness;
    float ao = 1.0f; // Ambient occlusion multiplier [0,1].

    // Priority: Handle packed glTF metallic-roughness first (common workflow).
    if (meshCB.textureFlags & TEX_FLAG_GLTF_METALLIC_ROUGHNESS)
    {
        float3 mrSample = textures[TEX_SLOT_GLTF_METALLIC_ROUGHNESS].Sample(texSampler, input.texcoord).rgb;
        ao = mrSample.r; // Red channel: AO.
        roughness *= mrSample.g; // Green: Roughness.
        metallic *= mrSample.b; // Blue: Metallic.
    }
    else
    {
        // Fallback to separate maps.
        if (meshCB.textureFlags & TEX_FLAG_AMBIENT_OCCLUSION)
        {
            ao = textures[TEX_SLOT_AMBIENT_OCCLUSION].Sample(texSampler, input.texcoord).r;
        }
        if (meshCB.textureFlags & TEX_FLAG_METALNESS)
        {
            metallic *= textures[TEX_SLOT_METALNESS].Sample(texSampler, input.texcoord).r; // Grayscale metallic.
        }
        if (meshCB.textureFlags & TEX_FLAG_DIFFUSE_ROUGHNESS)
        {
            roughness *= textures[TEX_SLOT_DIFFUSE_ROUGHNESS].Sample(texSampler, input.texcoord).g; // Green channel for roughness.
        }
        // Extend as needed for other types (e.g., SHININESS as 1.0 - roughness).
    }

    // Opacity for transparency (modulate by albedo alpha).
    float opacity = meshCB.opacity * albedo.a;

    // === LIGHTING SETUP ===
    // Extract camera position from view matrix (assuming look-at style; row-major).
    // This is a hack—ideally pass camPos as a constant. Assumes view[3] = -camPos * rotation.

    // View direction (surface to camera).
    float3 V = normalize(frameCB.camPos - input.worldPos);

    // Incoming light direction (negate stored dir if it points toward light).
    float3 L = normalize(-frameCB.lightDir.xyz);
    float3 H = normalize(V + L); // Half-vector for specular.

    // === PBR BRDF CALCULATION ===
    // Base reflectance (F0): Lerp from dielectric to albedo for metals.
    float3 F0 = lerp(F0_DIELECTRIC, albedo.rgb, metallic);

    // BRDF terms.
    float NDF = DistributionGGX(N, H, roughness); // Microfacet distribution.
    float G = GeometrySmith(N, V, L, roughness); // Geometry/visibility.
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0); // Fresnel reflectance.

    // Specular component (Cook-Torrance).
    float3 numerator = NDF * G * F;
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float denominator = 4.0f * NdotV * NdotL + 0.001f; // Avoid denom=0.
    float3 specular = numerator / denominator;

    // Energy conservation: Diffuse + Specular = 1 (modulated by metallic).
    float3 kS = F; // Specular strength.
    float3 kD = 1.0f - kS; // Diffuse strength.
    kD *= 1.0f - metallic; // No diffuse for metals.

    // Incoming radiance (light color * intensity).
    float3 radiance = frameCB.lightColor.rgb * frameCB.lightColor.a;

    // Direct lighting: (diffuse + specular) * light * NdotL.
    float3 diffuse = kD * albedo.rgb / PI; // Lambertian diffuse (divide by PI for energy conservation).
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    // Simple ambient (approximates IBL; multiply by AO for occlusion).
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo.rgb * ao; // Low ambient; tune for scene.

    // Total color: Ambient + direct lights.
    float3 color = ambient + Lo;

    // === POST-PROCESSING ===
    // Reinhard tone mapping (simple HDR compression).
    color = color / (color + 1.0f);

    // Gamma correction (sRGB output; assumes linear input from textures).
    color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));

    return float4(color, opacity);
}
