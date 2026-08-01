struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// [[vk::push_constant]] makes this a SPIR-V push constant block rather
// than a descriptor-bound uniform buffer. DXC ignores the vk:: namespace
// on the DX12 path, where it becomes an ordinary cbuffer backed by root
// constants - so one declaration serves both backends.
struct PushConstants
{
    float2 offset;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

PSInput mainVS(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position;
    result.position.xy += pushConstants.offset;
    result.color = color;

    return result;
}
