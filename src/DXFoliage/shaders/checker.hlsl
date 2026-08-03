struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// One matrix plus the texture's slot in the bindless heap. The index IS
// the binding - the CPU never binds this texture to a slot per draw, it
// just says which element of the heap to read. 17 dwords total, matching
// the pipeline layout's num32BitRootConstants.
struct PushConstants
{
    float4x4 transform;
    uint textureIndex;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

// The bindless heap, seen from the shader. Both annotations on purpose:
// [[vk::binding(N, 0)]] pins these to the set/binding layout that
// VulkanDescriptorHeap creates, register(t0, space1) matches the
// unbounded SRV range DX12PipelineLayout declares, and register(s0) its
// static sampler. DXC reads whichever namespace fits the target and
// ignores the other - one declaration, both backends, the same trick as
// [[vk::push_constant]] above.
[[vk::binding(0, 0)]] Texture2D g_textures[] : register(t0, space1);
[[vk::binding(1, 0)]] SamplerState g_sampler : register(s0);

PSInput mainVS(float3 position : POSITION, float2 uv : TEXCOORD0)
{
    PSInput result;

    // mul(matrix, vector) - see the convention note in core/Math.h.
    result.position = mul(pushConstants.transform, float4(position, 1.0f));
    result.uv = uv;

    return result;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    // The index came from a push constant, so it is uniform across the
    // draw - no NonUniformResourceIndex needed until something indexes
    // per-pixel (material IDs will, one day).
    return g_textures[pushConstants.textureIndex].Sample(g_sampler, input.uv);
}
