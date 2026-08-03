#include "stdafx.h"
#include "Renderer.h"

#include "core/Math.h"

#include <chrono>

#include "rhi/RendererBackendFactory.h"

#include <cstring>

namespace
{
    // D32 rather than D24S8: no stencil is needed, and a 32-bit float
    // depth buffer is universally supported where D24S8 is not.
    constexpr NSRHI::EFormat kDepthFormat = NSRHI::EFormat::D32_FLOAT;

    // Matches the pipeline's vertex attributes below: POSITION (3 floats)
    // at offset 0, COLOR (4 floats) at offset 12. Declared locally because
    // it's temporary demo content, replaced when real meshes arrive.
    struct DemoVertex
    {
        float position[3];
        float color[4];
    };

    // Checker cube vertex: POSITION (3 floats) at 0, TEXCOORD (2 floats)
    // at 12, matching checker.hlsl's input signature and the pipeline's
    // vertex attributes.
    struct CheckerVertex
    {
        float position[3];
        float uv[2];
    };
}

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

bool Renderer::Initialize(NSPlatform::IWindow& window, uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;

    m_backend = NSRHI::CreateRendererBackendFromArgs();
    if (not m_backend)
    {
        return false;
    }

    if (not m_backend->Initialize(window, width, height))
    {
        return false;
    }

    // THE descriptor heap, before anything that allocates a slot from it.
    // Sizes are guesses with headroom, not measurements: 256 transient
    // slots per frame covers ImGui's per-frame texture set several times
    // over, 512 statics covers every texture DXTerrain binds. The driver
    // limit here is ~1M, so generosity costs nothing but descriptors.
    {
        constexpr uint32_t kRingSlotsPerFrame = 256;
        constexpr uint32_t kStaticSlots = 512;
        const uint32_t frames = m_backend->FramesInFlight();

        m_descriptorHeap = m_backend->GetDevice().CreateDescriptorHeap(NSRHI::DescriptorHeapDesc{
            .type = NSRHI::EDescriptorHeapType::ShaderResource,
            .capacity = kRingSlotsPerFrame * frames + kStaticSlots,
        });
        m_descriptors = NSDescriptor::RingHeap(
            *m_descriptorHeap, kRingSlotsPerFrame, frames, kStaticSlots);

        // Per-draw constants. 4 MB/frame ≈ 16k draws at one 256-aligned
        // allocation each — DXTerrain reserves 1 GB/frame for the same
        // job, which is ~4 million draws of headroom; this is the same
        // design at a size the overflow assert can actually police. The
        // window is tail slack for the last allocation's descriptor
        // range (see kConstantBufferWindowBytes).
        constexpr size_t kConstantBytesPerFrame = 4 * 1024 * 1024;
        m_constantBuffer = m_backend->GetDevice().CreateBuffer(NSRHI::BufferDesc{
            .sizeBytes = kConstantBytesPerFrame * frames + NSRHI::kConstantBufferWindowBytes,
            .usage = NSRHI::EBufferUsage::Constant,
            .cpuVisible = true
        });
        m_constants = NSAllocator::ConstantAllocator(*m_constantBuffer, frames);
    }

    CreateFrameTargets();
    CreateCheckerCubeResources();
    CreateBlendProofResources();  // TEMP-BLEND

    return true;
}

void Renderer::CreateFrameTargets()
{
    // Released first: the replacements are created at the new size, and
    // holding both sets briefly would waste two full-screen allocations.
    m_renderTarget.reset();
    m_depthBuffer.reset();

    // Matches the backbuffer format so the backend's blit needs no
    // conversion. Once the scene goes HDR this becomes a float format and
    // a tonemapping pass has to produce a display-format image instead.
    m_renderTarget = m_backend->GetDevice().CreateTexture(NSRHI::TextureDesc{
        .width = m_width,
        .height = m_height,
        .format = m_backend->BackBufferFormat(),
        .isRenderTarget = true
    });

    m_depthBuffer = m_backend->GetDevice().CreateTexture(NSRHI::TextureDesc{
        .width = m_width,
        .height = m_height,
        .format = kDepthFormat,
        .isDepthStencil = true
    });
}

// The bindless texture path's first consumer, end to end: a 4x4x4 cube
// whose checkerboard cell is exactly 1 world unit — the texture doubles
// as a ruler, four cells per edge = four units. Everything the last
// sessions built fires for the first time here: heap slot from
// AllocateStatic, descriptor written by CreateShaderResourceView, index
// pushed as a root constant, shader reads g_textures[index].
void Renderer::CreateCheckerCubeResources()
{
    NSRHI::IDevice& device = m_backend->GetDevice();

    // -- Texture: 256x256, 4x4 cells of 64px each. Big cells rather than
    // a 4x4-PIXEL texture because the sampler is linear: bilinear blends
    // one texel wide, and at 4 pixels stretched over a whole face that
    // "edge" would be a quarter of the face. At 64px cells it is ~1
    // screen pixel — crisp squares, no point sampler needed.
    constexpr uint32_t kTexSize = 256;
    constexpr uint32_t kCellPx = kTexSize / 4;

    m_checkerTexture = device.CreateTexture(NSRHI::TextureDesc{
        .width = kTexSize,
        .height = kTexSize,
        .format = NSRHI::EFormat::R8G8B8A8_UNORM
    });

    m_checkerStaging = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = kTexSize * kTexSize * 4,
        .usage = NSRHI::EBufferUsage::Upload,
        .cpuVisible = true
    });

    auto* pixels = static_cast<uint8_t*>(m_checkerStaging->Map());
    for (uint32_t y = 0; y < kTexSize; ++y)
    {
        for (uint32_t x = 0; x < kTexSize; ++x)
        {
            // Two greys, not black/white: pure black merges with the
            // clear colour at glancing angles and hides the silhouette.
            const bool light = ((x / kCellPx) + (y / kCellPx)) % 2 == 0;
            uint8_t* p = pixels + (y * kTexSize + x) * 4;
            p[0] = p[1] = p[2] = light ? 220 : 45;
            p[3] = 255;
        }
    }
    m_checkerStaging->Unmap();
    // The copy itself is recorded in BeginFrame — uploads are command-list
    // work and no command list exists during Initialize.
    m_checkerUploadPending = true;

    // A static slot: this texture lives as long as the renderer, exactly
    // what the static region is for. Writing the descriptor while the
    // image is still UNDEFINED is fine — a descriptor write touches the
    // heap, not the image; the layout only has to be right when a draw
    // actually samples it, and BeginFrame's barrier runs first.
    m_checkerSlot = m_descriptors.AllocateStatic();
    ASSERT(m_checkerSlot.IsValid(), "Descriptor heap static region exhausted at startup");
    device.CreateShaderResourceView(
        *m_descriptorHeap, m_descriptorHeap->At(m_checkerSlot.index), m_checkerTexture.get());

    // 1 root constant (the texture index); the transform moved to
    // constant slot 0, allocated per draw from the ConstantAllocator.
    // This split is deliberate proof-of-work: the same spinning
    // checkerboard now depends on the whole constant path (allocate →
    // write → SetConstantBuffer → dynamic offset / root CBV) with zero
    // new visual variables — the known-good-control method.
    m_texPipelineLayout = device.CreatePipelineLayout(NSRHI::PipelineLayoutDesc{
        .num32BitRootConstants = 1,
        .usesBindlessDescriptorTable = true,
        .bindlessHeap = m_descriptorHeap.get(),
        .numConstantBufferSlots = 1,
        .constantBuffer = m_constantBuffer.get()
    });

    m_vertexStride = sizeof(CheckerVertex);

    m_pipeline = device.CreateGraphicsPipeline(NSRHI::GraphicsPipelineDesc{
        .vertexShader = { L"checker.hlsl", L"mainVS", NSRHI::EShaderStage::Vertex },
        .pixelShader = { L"checker.hlsl", L"mainPS", NSRHI::EShaderStage::Pixel },
        .vertexAttributes = {
            { "POSITION", NSRHI::EFormat::R32G32B32_FLOAT, 0 },
            { "TEXCOORD", NSRHI::EFormat::R32G32_FLOAT, 12 }
        },
        .vertexStrideBytes = m_vertexStride,
        .topology = NSRHI::EPrimitiveTopology::TriangleList,
        // Ask the backend rather than assuming: the swapchain picks
        // whatever the surface supports (B8G8R8A8 on this compositor,
        // R8G8B8A8 elsewhere) and a mismatch here is a validation error.
        .colorTargetFormats = { m_backend->BackBufferFormat() },
        .depthTargetFormat = kDepthFormat,
        .depthTestEnabled = true,
        .depthWriteEnabled = true,
        .layout = m_texPipelineLayout.get()
    });

    // 24 vertices, not 8: shared corners cannot carry per-face UVs, and
    // every face wants the full 0..1 checker. Positions are ±2 — the
    // 4-unit cube this function exists to prove. Each face's corner
    // order and both triangles are lifted verbatim from the proven
    // shared-corner cube (see git history), so the winding survives:
    // clockwise from outside, matching FRONT_FACE_CLOCKWISE + back cull.
    const CheckerVertex cubeVertices[]{
        // back (-Z)
        { { -2.f, -2.f, -2.f }, { 0.f, 0.f } },
        { { -2.f,  2.f, -2.f }, { 0.f, 1.f } },
        { {  2.f,  2.f, -2.f }, { 1.f, 1.f } },
        { {  2.f, -2.f, -2.f }, { 1.f, 0.f } },
        // front (+Z)
        { { -2.f, -2.f,  2.f }, { 0.f, 0.f } },
        { {  2.f, -2.f,  2.f }, { 1.f, 0.f } },
        { {  2.f,  2.f,  2.f }, { 1.f, 1.f } },
        { { -2.f,  2.f,  2.f }, { 0.f, 1.f } },
        // left (-X)
        { { -2.f, -2.f, -2.f }, { 0.f, 0.f } },
        { { -2.f, -2.f,  2.f }, { 1.f, 0.f } },
        { { -2.f,  2.f,  2.f }, { 1.f, 1.f } },
        { { -2.f,  2.f, -2.f }, { 0.f, 1.f } },
        // right (+X)
        { {  2.f, -2.f, -2.f }, { 0.f, 0.f } },
        { {  2.f,  2.f, -2.f }, { 0.f, 1.f } },
        { {  2.f,  2.f,  2.f }, { 1.f, 1.f } },
        { {  2.f, -2.f,  2.f }, { 1.f, 0.f } },
        // top (+Y)
        { { -2.f,  2.f, -2.f }, { 0.f, 0.f } },
        { { -2.f,  2.f,  2.f }, { 0.f, 1.f } },
        { {  2.f,  2.f,  2.f }, { 1.f, 1.f } },
        { {  2.f,  2.f, -2.f }, { 1.f, 0.f } },
        // bottom (-Y)
        { { -2.f, -2.f, -2.f }, { 0.f, 0.f } },
        { {  2.f, -2.f, -2.f }, { 1.f, 0.f } },
        { {  2.f, -2.f,  2.f }, { 1.f, 1.f } },
        { { -2.f, -2.f,  2.f }, { 0.f, 1.f } },
    };

    m_vertexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(cubeVertices),
        .usage = NSRHI::EBufferUsage::Vertex,
        .cpuVisible = true
    });

    void* mapped = m_vertexBuffer->Map();
    std::memcpy(mapped, cubeVertices, sizeof(cubeVertices));
    m_vertexBuffer->Unmap();

    // Every face is (b, b+1, b+2)(b, b+2, b+3) over its four vertices —
    // the per-face orders above were chosen to make that uniform.
    uint16_t cubeIndices[36]{};
    for (uint16_t face = 0; face < 6; ++face)
    {
        const uint16_t b = face * 4;
        uint16_t* tri = cubeIndices + face * 6;
        tri[0] = b; tri[1] = b + 1; tri[2] = b + 2;
        tri[3] = b; tri[4] = b + 2; tri[5] = b + 3;
    }
    m_indexCount = static_cast<uint32_t>(std::size(cubeIndices));

    m_indexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(cubeIndices),
        .usage = NSRHI::EBufferUsage::Index,
        .cpuVisible = true
    });

    void* mappedIndices = m_indexBuffer->Map();
    std::memcpy(mappedIndices, cubeIndices, sizeof(cubeIndices));
    m_indexBuffer->Unmap();
}

// TEMP-BLEND: the observable control for EBlendMode. ImGui cannot render a
// single widget without alpha blending, so this proves the state actually
// reaches the GPU before anything depends on it - the whole of this
// function goes when the ImGui pass lands.
void Renderer::CreateBlendProofResources()
{
    NSRHI::IDevice& device = m_backend->GetDevice();

    // The quad's own layout — one float4x4 (16 dwords), no descriptor
    // heap. Lived with the old vertex-colour cube until the checker cube
    // replaced that pipeline; the quad still draws with the original
    // vert/pixel.hlsl pair, so the plain layout moves here with it.
    m_pipelineLayout = device.CreatePipelineLayout(NSRHI::PipelineLayoutDesc{
        .num32BitRootConstants = 16
    });

    // Same shaders and layout as the cubes: the ONLY difference from
    // m_pipeline is blendMode and the depth state. One variable per step -
    // if the quad appears blended, blending is what changed.
    //
    // Depth test off so the quad always draws on top regardless of where
    // the cubes are, and depth write off so it leaves the buffer alone.
    // Transparent geometry that writes depth occludes whatever is drawn
    // after it, which is a different bug entirely and not the one under
    // test here.
    m_blendPipeline = device.CreateGraphicsPipeline(NSRHI::GraphicsPipelineDesc{
        .vertexShader = { L"vert.hlsl", L"mainVS", NSRHI::EShaderStage::Vertex },
        .pixelShader = { L"pixel.hlsl", L"mainPS", NSRHI::EShaderStage::Pixel },
        .vertexAttributes = {
            { "POSITION", NSRHI::EFormat::R32G32B32_FLOAT, 0 },
            { "COLOR", NSRHI::EFormat::R32G32B32A32_FLOAT, 12 }
        },
        // Its OWN stride — m_vertexStride now belongs to the checker
        // cube (20 bytes); this quad still uses DemoVertex (28).
        .vertexStrideBytes = sizeof(DemoVertex),
        .topology = NSRHI::EPrimitiveTopology::TriangleList,
        .colorTargetFormats = { m_backend->BackBufferFormat() },
        .depthTargetFormat = kDepthFormat,
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .blendMode = NSRHI::EBlendMode::AlphaBlend,
        .layout = m_pipelineLayout.get()
    });

    // Coordinates are already clip space: DrawScene pushes an identity
    // matrix for this draw, so no model/view/projection is involved and
    // the quad cannot be mispositioned by a matrix bug. z = 0.5 is inside
    // the 0..1 depth range, though nothing tests it here.
    //
    // Alpha 0.5 with a strong green: over a black clear it reads as dark
    // green, and over a cube face it visibly tints rather than replaces.
    const DemoVertex quadVertices[]{
        { { -0.6f, -0.6f, 0.5f }, { 0.f, 1.f, 0.f, 0.5f } },
        { {  0.6f, -0.6f, 0.5f }, { 0.f, 1.f, 0.f, 0.5f } },
        { {  0.6f,  0.6f, 0.5f }, { 0.f, 1.f, 0.f, 0.5f } },
        { { -0.6f,  0.6f, 0.5f }, { 0.f, 1.f, 0.f, 0.5f } },
    };

    m_quadVertexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(quadVertices),
        .usage = NSRHI::EBufferUsage::Vertex,
        .cpuVisible = true
    });
    void* mappedQuad = m_quadVertexBuffer->Map();
    std::memcpy(mappedQuad, quadVertices, sizeof(quadVertices));
    m_quadVertexBuffer->Unmap();

    // BOTH windings on purpose. Culling is on (VK_CULL_MODE_BACK_BIT /
    // FRONT_FACE_CLOCKWISE) and this quad skips the projection entirely,
    // so getting the winding backwards would make it vanish - which looks
    // exactly like blending having done nothing. Two mirrored copies mean
    // the quad is visible either way, and exactly one of each mirrored
    // pair survives culling, so it is still drawn once and the 0.5 alpha
    // is not applied twice. Removes culling as a variable from a test
    // that is about blending.
    const uint16_t quadIndices[]{
        0, 3, 2,  0, 2, 1,   // one winding
        0, 2, 3,  0, 1, 2,   // the mirror; the culled half of each pair
    };
    m_quadIndexCount = static_cast<uint32_t>(std::size(quadIndices));

    m_quadIndexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(quadIndices),
        .usage = NSRHI::EBufferUsage::Index,
        .cpuVisible = true
    });
    void* mappedQuadIndices = m_quadIndexBuffer->Map();
    std::memcpy(mappedQuadIndices, quadIndices, sizeof(quadIndices));
    m_quadIndexBuffer->Unmap();
}

void Renderer::Shutdown()
{
    // Drain first: these resources may still be referenced by a command
    // buffer the GPU has not finished, and destroying them under it is
    // undefined behaviour. The backend's own Shutdown() waits too, but
    // that runs after these resets - too late.
    if (m_backend) m_backend->WaitForGPU();

    // Then release front-end-owned GPU resources, before the backend (and
    // with it the device) goes away.
    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_quadVertexBuffer.reset();  // TEMP-BLEND
    m_quadIndexBuffer.reset();  // TEMP-BLEND
    m_checkerStaging.reset();
    m_checkerTexture.reset();
    m_renderTarget.reset();
    m_depthBuffer.reset();
    m_pipeline.reset();
    m_blendPipeline.reset();  // TEMP-BLEND
    // Layouts after the pipelines built against them; the heap and the
    // constant buffer after the layouts that referenced them (the Vulkan
    // layout's set 1 points at the constant buffer).
    m_pipelineLayout.reset();
    m_texPipelineLayout.reset();
    m_descriptorHeap.reset();
    m_constantBuffer.reset();

    if (m_backend)
    {
        m_backend->Shutdown();
        m_backend.reset();
    }
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 or height == 0) return;

    m_width = width;
    m_height = height;

    if (m_backend)
    {
        m_backend->Resize(width, height);
        // The backbuffer changed size, so both targets must follow -
        // BeginRendering requires every attachment to share an extent.
        CreateFrameTargets();
    }
}

void Renderer::BeginFrame()
{
    if (not m_backend) return;

    m_cmd = &m_backend->BeginFrame();

    // Reclaim this frame's ring ranges — descriptors and constants share
    // the in-flight contract. Safe here and only here: the backend's
    // BeginFrame just waited on this frame slot's fence, so the previous
    // submission that read them has retired.
    m_descriptors.BeginFrame(m_backend->FrameIndex());
    m_constants.BeginFrame(m_backend->FrameIndex());

    // One-shot uploads, recorded BEFORE BeginRendering — copies are
    // illegal inside a dynamic-rendering pass. Undefined as the source
    // state because the image's current contents are garbage anyway;
    // ShaderResource afterwards so this same frame can already sample it.
    if (m_checkerUploadPending)
    {
        m_cmd->TransitionTexture(m_checkerTexture.get(),
            NSRHI::EResourceState::Undefined, NSRHI::EResourceState::CopyDestination);
        m_cmd->CopyBufferToTexture(m_checkerTexture.get(), m_checkerStaging.get());
        m_cmd->TransitionTexture(m_checkerTexture.get(),
            NSRHI::EResourceState::CopyDestination, NSRHI::EResourceState::ShaderResource);
        m_checkerUploadPending = false;
    }

    // Our own target, not a swapchain image: the backend blits this
    // across at EndFrame. Needs the same Undefined -> RenderTarget
    // transition the backend used to do for the backbuffer.
    m_cmd->TransitionTexture(m_renderTarget.get(),
        NSRHI::EResourceState::Undefined, NSRHI::EResourceState::RenderTarget);

    NSRHI::RenderingAttachment color{};
    color.target = m_renderTarget.get();
    color.clear = true;
    color.clearColor = { 0.f, 0.f, 0.f, 0.f };

    // The backend transitions the backbuffer it owns; the depth buffer is
    // ours, so we transition it. From Undefined rather than DepthWrite:
    // the contents are cleared below anyway, and declaring them undefined
    // lets the driver discard rather than preserve them.
    m_cmd->TransitionTexture(m_depthBuffer.get(),
        NSRHI::EResourceState::Undefined, NSRHI::EResourceState::DepthWrite);

    NSRHI::DepthAttachment depth{};
    depth.target = m_depthBuffer.get();
    depth.clear = true;
    // 1.0 is the far plane under the 0..1 depth range PerspectiveFovLH
    // produces, so clearing to it means "nothing has been drawn yet".
    depth.clearDepth = 1.0f;

    m_cmd->BeginRendering({ color }, &depth);

    m_cmd->SetViewport(NSRHI::Viewport{
        .x = 0.f, .y = 0.f,
        .width = static_cast<float>(m_width),
        .height = static_cast<float>(m_height)
    });
    m_cmd->SetScissor(NSRHI::ScissorRect{
        .left = 0, .top = 0,
        .right = static_cast<int32_t>(m_width),
        .bottom = static_cast<int32_t>(m_height)
    });
}

void Renderer::DrawScene(std::shared_ptr<NSScene::IScene>)
{
    if (not m_cmd) return;

    // The heap once per frame, before any pipeline that indexes it. On
    // Vulkan the actual vkCmdBindDescriptorSets is deferred inside until
    // SetPipeline supplies a layout; on DX12 this is the real
    // SetDescriptorHeaps call. Recorded per frame because the command
    // list is reset per frame — nothing persists across the reset.
    m_cmd->SetDescriptorHeap(m_descriptorHeap.get());

    m_cmd->SetPipeline(m_pipeline.get());

    // An animated rotation: the spin shows all six faces in turn, and
    // proves the matrix is rebuilt and re-pushed every frame rather than
    // baked once at startup.
    static const auto start = std::chrono::steady_clock::now();
    const float seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - start).count();

    const NSMath::Float4x4 model = NSMath::Multiply(
        NSMath::RotationX(seconds * 0.6f), NSMath::RotationY(seconds));
    // Eye at -8, not the old -2.5: the cube is 4 units across now and the
    // rotating diagonal sweeps ~3.5 — the old camera would be inside it.
    const NSMath::Float4x4 view = NSMath::LookAtLH(
        { 0.f, 0.f, -8.f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });
    const NSMath::Float4x4 proj = NSMath::PerspectiveFovLH(
        1.0472f,  // 60 degrees
        static_cast<float>(m_width) / static_cast<float>(m_height),
        0.1f, 100.f);

    // The transform goes through the constant ring — write through the
    // mapped pointer, hand the OFFSET to the command list. Mirrors
    // DXTerrain's constAlloc + As<T>() + SetGraphicsRootConstantBufferView
    // triple exactly, one draw's worth per frame.
    NSAllocator::Ctx drawCB = m_constants.Allocate(sizeof(NSMath::Float4x4));
    drawCB.As<NSMath::Float4x4>() =
        NSMath::Multiply(model, NSMath::Multiply(view, proj));
    m_cmd->SetConstantBuffer(0, drawCB.offsetBytes);

    // Only the bindless slot index still travels as a push constant.
    const uint32_t textureIndex = m_checkerSlot.index;

    m_cmd->SetVertexBuffer(m_vertexBuffer.get(), m_vertexStride);
    m_cmd->SetIndexBuffer(m_indexBuffer.get(), false);   // false = 16-bit
    m_cmd->SetRootConstants(0, 1, &textureIndex);
    m_cmd->DrawIndexed(m_indexCount);

    // TEMP-BLEND: drawn LAST and over the cubes, which are the known-good
    // control. Identity transform - the quad's vertices are already clip
    // space, so nothing here depends on the matrix path being right.
    const NSMath::Float4x4 identity = NSMath::Float4x4::Identity();  // TEMP-BLEND
    m_cmd->SetPipeline(m_blendPipeline.get());  // TEMP-BLEND
    m_cmd->SetVertexBuffer(m_quadVertexBuffer.get(), sizeof(DemoVertex));  // TEMP-BLEND
    m_cmd->SetIndexBuffer(m_quadIndexBuffer.get(), false);  // TEMP-BLEND
    m_cmd->SetRootConstants(0, 16, &identity);  // TEMP-BLEND
    m_cmd->DrawIndexed(m_quadIndexCount);  // TEMP-BLEND
}

void Renderer::EndFrame()
{
    if (not m_cmd) return;

    m_cmd->EndRendering();
    m_cmd = nullptr;

    m_backend->EndFrame(*m_renderTarget);
}
