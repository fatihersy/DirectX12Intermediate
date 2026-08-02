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
    float4x4 transform;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

PSInput mainVS(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    // mul(matrix, vector), NOT mul(vector, matrix). NSMath builds
    // row-vector matrices and uploads them untransposed; HLSL packs
    // float4x4 column-major by default, which transposes them on arrival,
    // and this multiplication order compensates. Both halves or neither -
    // see the convention note in core/Math.h. DXTerrain and DXMaterial
    // already do exactly this.
    result.position = mul(pushConstants.transform, position);
    result.color = color;

    return result;
}
