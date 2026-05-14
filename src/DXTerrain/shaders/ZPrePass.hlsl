struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

struct FrameConstants
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 lightDir;
    float4 lightColor;
    float3 camPos;
    uint _padding1;
};

struct MeshConstants
{
    float4x4 worldMatrix;
    float3x4 normalMatrix;
    float4 baseColor;
    float metallic;
    float roughness;
    float opacity;
    uint textureFlags;
};

ConstantBuffer<FrameConstants> frameCB : register(b0);
ConstantBuffer<MeshConstants> meshCB : register(b1);

VSOutput mainVS(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(meshCB.worldMatrix, float4(input.position, 1.0f));
    float4 viewPos = mul(frameCB.viewMatrix, worldPos);
    output.position = mul(frameCB.projectionMatrix, viewPos);
    return output;
}
