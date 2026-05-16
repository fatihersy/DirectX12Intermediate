struct VSInput
{
    float3 position : POSITION;
    #ifndef DEPTH_PREPASS
        float3 normal : NORMAL;
        float3 tangent : TANGENT;
        float3 bitangent : BITANGENT;
        float2 texcoord : TEXCOORD0;
    #endif
};

struct VSOutput
{
    float4 position : SV_POSITION;

    #ifndef DEPTH_PREPASS
        float3 worldPos : WORLD_POSITION;
        float3 normal : NORMAL;
        float2 texcoord : TEXCOORD0;
        float3x3 TBN : TBN_MATRIX;
    #endif
};

#ifndef DEPTH_PREPASS
    struct FrameConstants
    {
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float4 lightDir;
        float4 lightColor;
        float3 camPos;
        uint PADDING_0;
    };
#else
    struct FrameConstants
    {
        float4x4 viewMatrix;
        float4x4 projectionMatrix;
        float3 camPos;
        uint _padding0;
    };
#endif

#ifndef DEPTH_PREPASS
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
#else
    struct MeshConstants
    {
        float4x4 worldMatrix;
    };
#endif

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<MeshConstants> meshCB : register(b1);

VSOutput mainVS(VSInput input)
{
    #ifndef DEPTH_PREPASS
        VSOutput output;

        float4 localPos = float4(input.position, 1.0f);
        float4 worldPos = mul(meshCB.worldMatrix, localPos);
        output.worldPos = worldPos.xyz;

        float4 viewPos = mul(frameCB.viewMatrix, worldPos);
        output.position = mul(frameCB.projectionMatrix, viewPos);

        float3 localNormal = input.normal;
        float3 worldNormal = normalize(mul(meshCB.normalMatrix, localNormal));
        output.normal = worldNormal;
        output.texcoord = input.texcoord;

        float3 localTangent = input.tangent;
        float3 T = normalize(mul(meshCB.normalMatrix, localTangent));
        float3 localBitangent = input.bitangent;
        float3 B = normalize(mul(meshCB.normalMatrix, localBitangent));
        float3 N = worldNormal;

        T = normalize(T - dot(T, N) * N);
        B = cross(N, T);
        B = normalize(B);

        output.TBN = float3x3(T, B, N);

        return output;

    #else
        VSOutput output;
        float4 worldPos = mul(meshCB.worldMatrix, float4(input.position, 1.0f));
        float4 viewPos = mul(frameCB.viewMatrix, worldPos);
        output.position = mul(frameCB.projectionMatrix, viewPos);
        return output;
    #endif
}
