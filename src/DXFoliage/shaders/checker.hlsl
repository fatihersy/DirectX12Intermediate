struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// The transform now arrives through constant slot 0 - a per-draw
// allocation from NSAllocator::ConstantAllocator, reached as
// register(b1) on DX12 (a root CBV at baseVA + offset) and as
// [[vk::binding(0, 1)]] on Vulkan (a dynamic-UBO descriptor whose offset
// changes per draw). Slot N = b{N+1}; b0 stays the push-constant block.
// Same dual-annotation trick as the heap declarations below.
struct DrawConstants
{
    float4x4 transform;
};
[[vk::binding(0, 1)]] ConstantBuffer<DrawConstants> drawCB : register(b1);

// Only the texture's slot in the bindless heap remains pushed - the
// index IS the binding, which is the bindless model. 1 dword, matching
// the pipeline layout's num32BitRootConstants.
struct PushConstants
{
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
    result.position = mul(drawCB.transform, float4(position, 1.0f));
    result.uv = uv;

    return result;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    // The index came from a push constant, so it is uniform across the
    // draw - no NonUniformResourceIndex needed until something indexes
    // per-pixel (material IDs will, one day).
    float4 s = g_textures[pushConstants.textureIndex].Sample(g_sampler, input.uv);

    // Modulate by alpha. A NO-OP for the checkerboard (alpha is 255
    // everywhere, so rgb*1 == rgb), but it is what makes a font atlas
    // legible: ImGui's atlas is white RGB with the glyph shape carried
    // ENTIRELY in alpha, so sampling it raw would paint a solid white
    // cube and prove nothing. This pass is temporary scaffolding for
    // step B - the real ImGui shader multiplies by the vertex colour,
    // which carries alpha of its own.
    return float4(s.rgb * s.a, 1.0f);
}
