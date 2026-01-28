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
};
 
PSInput mainVS(float3 position : POSITION, float3 normal : NORMAL)
{
    PSInput result;

    float4 pos = float4(position, 1.0f);
    
    pos = mul(pos, world);
    pos = mul(pos, view);
    pos = mul(pos, proj);

    result.position = pos;
    
    float3 worldNormal = mul(normal, (float3x3) world);
    result.normal = normalize(worldNormal);
 
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
    float3 norm = normalize(input.normal);
    float diffuse = max(dot(norm, normalize(lightDir.xyz)), 0.0f);
    float ambient = 0.05f;
    float lighting = ambient + diffuse * 0.8f;
    float4 baseColor = float4(1.0f, 0.5f, 0.0f, 1.0f);
    return baseColor * lightColor * lighting;
}
