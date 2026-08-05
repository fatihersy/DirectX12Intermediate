struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

// Per draw COMMAND, not per frame: ImGui switches texture between
// commands (font atlas, then any user image), and in the bindless model
// that switch costs one integer rather than a descriptor rebind. The
// projection sits alongside it because both are tiny and change at the
// same rate or slower - 20 dwords total, well inside Vulkan's guaranteed
// 128-byte push-constant budget.
struct PushConstants
{
    float4x4 projection;
    uint textureIndex;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

// The same bindless declarations as checker.hlsl - one heap, shared by
// every pass. See VulkanDescriptorHeap for why the texture array and the
// sampler are separate bindings rather than a combined image sampler.
[[vk::binding(0, 0)]] Texture2D g_textures[] : register(t0, space1);
[[vk::binding(1, 0)]] SamplerState g_sampler : register(s0);

// Matches ImDrawVert exactly: pos float2 @0, uv float2 @8, col RGBA8 @16
// (20 bytes). COLOR arrives as R8G8B8A8_UNORM, so the hardware expands
// it to 0..1 floats - ImGui packs it as ABGR in a ImU32, which is the
// same byte order as RGBA8 little-endian, so no swizzle is needed.
PSInput mainVS(float2 position : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR)
{
    PSInput result;

    // mul(matrix, vector) - see the convention note in core/Math.h.
    result.position = mul(pushConstants.projection, float4(position, 0.0f, 1.0f));
    result.color = color;
    result.uv = uv;

    return result;
}

float4 mainPS(PSInput input) : SV_TARGET
{
    // Vertex colour MODULATES the atlas. This is what carries glyph
    // colour, window tinting and fading: the atlas itself is white with
    // the shape in alpha, so text colour lives entirely in the vertex
    // stream. Alpha multiplies too, which is why the pipeline needs
    // straight (non-premultiplied) alpha blending.
    return input.color * g_textures[pushConstants.textureIndex].Sample(g_sampler, input.uv);
}
