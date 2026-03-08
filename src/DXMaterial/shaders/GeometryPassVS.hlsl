// Vertex shader for PBR setup with normal mapping.
// Transforms vertices to clip space, computes world normals/TBN for PS.
// Supports inverse-transpose for correct normal handling under non-uniform scale.

struct VSInput
{
    float3 position : POSITION; // Local-space position.
    float3 normal : NORMAL; // Local-space normal.
    float3 tangent : TANGENT; // Local-space tangent (for TBN).
    float3 bitangent : BITANGENT; // Local-space bitangent (for TBN; may be recomputed).
    float2 texcoord : TEXCOORD0; // UV coordinates.
};

struct PSInput
{
    float4 position : SV_POSITION; // Clip-space position (for rasterization).
    float3 worldPos : WORLD_POSITION; // World-space position (for PS lighting).
    float3 normal : NORMAL; // World-space normal (for PS).
    float2 texcoord : TEXCOORD0; // UVs (passthrough).
    float3x3 TBN : TBN_MATRIX; // Tangent-to-world matrix (for normal mapping).
};

struct FrameConstants
{
    float4x4 viewMatrix; // World -> view transform.
    float4x4 projectionMatrix; // View -> clip transform.
    float4 lightDir; // Directional light (unused in VS).
    float4 lightColor; // Light color (unused in VS).
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

PSInput mainVS(VSInput input)
{
    PSInput output;

    // === POSITION TRANSFORM ===
    // Local -> world.
    float4 localPos = float4(input.position, 1.0f);
    float4 worldPos = mul(meshCB.worldMatrix, localPos); // FIXED: matrix * vec
    output.worldPos = worldPos.xyz;

    // World -> view -> clip (NDC).
    float4 viewPos = mul(frameCB.viewMatrix, worldPos); // FIXED
    output.position = mul(frameCB.projectionMatrix, viewPos); // FIXED

    // === NORMAL TRANSFORM ===
    // Use inverse-transpose matrix for scale/shear invariance.
    float3 localNormal = input.normal;
    float3 worldNormal = normalize(mul(meshCB.normalMatrix, localNormal)); // FIXED
    output.normal = worldNormal;

    // === UV PASSTHROUGH ===
    output.texcoord = input.texcoord;

    // === TBN MATRIX FOR NORMAL MAPPING ===
    // Transform tangent and bitangent using same normal matrix.
    float3 localTangent = input.tangent;
    float3 T = normalize(mul(meshCB.normalMatrix, localTangent)); // FIXED
    float3 localBitangent = input.bitangent;
    float3 B = normalize(mul(meshCB.normalMatrix, localBitangent)); // FIXED
    float3 N = worldNormal;

    // Gram-Schmidt orthogonalization (ensures T/B perp to N; recomputes B for consistency).
    T = normalize(T - dot(T, N) * N);
    B = cross(N, T); // Right-handed; if mirrored, use B = -cross(N, T);
    B = normalize(B);

    // TBN matrix: Columns = T, B, N (transforms tangent-space vectors to world).
    output.TBN = float3x3(T, B, N);

    return output;
}
