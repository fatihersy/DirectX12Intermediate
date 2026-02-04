cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 proj;
    float4 lightDir;
    float4 lightColor;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

Texture2D diffuseTexture : register(t0);
SamplerState linearSampler : register(s0);
 
PSInput mainVS(float3 position : POSITION, float3 normal : NORMAL, float2 texcoord : TEXCOORD0)
{
    PSInput result;

    float4 pos = float4(position, 1.0f);
    
    pos = mul(pos, world);
    pos = mul(pos, view);
    pos = mul(pos, proj);

    result.position = pos;
    
    float3 worldNormal = mul(normal, (float3x3) world);
    result.normal = normalize(worldNormal);
    result.texcoord = texcoord;
 
    return result;
}
 
float4 mainPS(PSInput input) : SV_TARGET
{
    // ========================================
    // DEBUG OPTION 1: Solid color
    // ========================================
    // Uncomment this to see if cube renders at all (should be bright red)
    // return float4(1.0f, 0.0f, 0.0f, 1.0f);
    
    // ========================================
    // DEBUG OPTION 2: Visualize normals as colors
    // ========================================
    // Uncomment this to see if normals are correct
    //Normals map to RGB: X→Red, Y→Green, Z→Blue
    //return float4(input.normal * 0.5f + 0.5f, 1.0f);
    
    // ========================================
    // DEBUG OPTION 3: Visualize lighting only
    // ========================================
    // Uncomment to see just the lighting intensity
    //float3 norm = normalize(input.normal);
    //float diffuse = max(dot(norm, normalize(lightDir.xyz)), 0.0f);
    //float ambient = 0.2f;
    //float lighting = ambient + diffuse * 0.8f;
    //return float4(lighting, lighting, lighting, 1.0f);
    
    // ========================================
    // NORMAL RENDERING: Full lighting
    // ========================================
    float3 normal = normalize(input.normal);
    float4 texColor = diffuseTexture.Sample(linearSampler, input.texcoord);
    float NdotL = max(0.0f, dot(normal, -normalize(lightDir.xyz)));
    float ambient = 0.05f; // Define ambient here (adjust as needed)
    float lighting = ambient + NdotL * 0.95f;
    return texColor * lighting * lightColor;
}
