cbuffer FrameCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSIn
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSIn VSMain(VSIn input)
{
    PSIn output;
    float4 viewPos = mul(view, float4(input.position, 1.0f));
    output.position = mul(proj, viewPos);
    output.color = input.color;
    return output;
}

float4 PSMain(PSIn input) : SV_TARGET
{
    return input.color;
}
