#include "stdafx.h"
#include "Renderer.h"

#include "core/Math.h"

#include <chrono>

#include "rhi/RendererBackendFactory.h"

#include "imgui.h"

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
        m_constants = NSAllocator::RingAllocator(
            *m_constantBuffer, frames,
            NSRHI::kConstantBufferAlignment, NSRHI::kConstantBufferWindowBytes);

        // Per-frame geometry. 4 MB of vertices per frame is ~200k ImGui
        // verts, far past any realistic debug UI; indices get a quarter of
        // that. No tail slack — unlike a dynamic UBO descriptor, a vertex
        // or index binding takes an offset with no fixed range.
        //
        // Alignment 16: comfortably above every vertex stride in play and
        // a multiple of the 2- and 4-byte index sizes, so one number
        // serves both rings without tying them to a particular struct.
        constexpr size_t kDynamicVertexBytesPerFrame = 4 * 1024 * 1024;
        constexpr size_t kDynamicIndexBytesPerFrame = 1 * 1024 * 1024;
        constexpr size_t kGeometryAlignment = 16;

        m_dynamicVertexBuffer = m_backend->GetDevice().CreateBuffer(NSRHI::BufferDesc{
            .sizeBytes = kDynamicVertexBytesPerFrame * frames,
            .usage = NSRHI::EBufferUsage::Vertex,
            .cpuVisible = true
        });
        m_dynamicIndexBuffer = m_backend->GetDevice().CreateBuffer(NSRHI::BufferDesc{
            .sizeBytes = kDynamicIndexBytesPerFrame * frames,
            .usage = NSRHI::EBufferUsage::Index,
            .cpuVisible = true
        });
        m_dynamicVerts = NSAllocator::RingAllocator(*m_dynamicVertexBuffer, frames, kGeometryAlignment);
        m_dynamicIndices = NSAllocator::RingAllocator(*m_dynamicIndexBuffer, frames, kGeometryAlignment);

        // Texture upload staging. 8 MB/frame covers a 1024x1024 RGBA
        // atlas twice over. Alignment 512 is D3D12's
        // TEXTURE_DATA_PLACEMENT_ALIGNMENT for placed footprints — the
        // assert in DX12CommandList::CopyBufferToTexture checks it, and
        // Vulkan does not care.
        constexpr size_t kUploadBytesPerFrame = 8 * 1024 * 1024;
        constexpr size_t kPlacedFootprintAlignment = 512;

        m_uploadBuffer = m_backend->GetDevice().CreateBuffer(NSRHI::BufferDesc{
            .sizeBytes = kUploadBytesPerFrame * frames,
            .usage = NSRHI::EBufferUsage::Upload,
            .cpuVisible = true
        });
        m_uploads = NSAllocator::RingAllocator(
            *m_uploadBuffer, frames, kPlacedFootprintAlignment);
    }

    CreateFrameTargets();
    CreateCheckerCubeResources();

    // Context here rather than inside ImGuiRenderer: it is shared with
    // whatever feeds input, which is a platform concern, not a renderer
    // one. ImGuiRenderer owns only the GPU-side halves.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    // Clipboard hooks are bound by app, which owns the IInputSource —
    // see app::LoadPipeline, right after this returns.
    m_imgui.Initialize(m_backend->GetDevice(), *m_descriptorHeap, m_descriptors,
        m_uploads, m_dynamicVerts, m_dynamicIndices,
        m_backend->BackBufferFormat(), kDepthFormat);

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
    // CopySource because EndFrame blits this into the backbuffer. NOT
    // Sampled: nothing reads it yet — the tonemap pass adds that bit when
    // it arrives, and until then the driver is free to compress it in
    // ways an SRV would forbid.
    m_renderTarget = m_backend->GetDevice().CreateTexture(NSRHI::TextureDesc{
        .width = m_width,
        .height = m_height,
        .format = m_backend->BackBufferFormat(),
        .usage = NSRHI::ETextureUsage::RenderTarget | NSRHI::ETextureUsage::CopySource
    });

    // DepthStencil alone, so DX12 can add DENY_SHADER_RESOURCE. DXTerrain
    // samples its depth buffer in PostProcess.hlsl — when that pass lands
    // here this gains | Sampled, and that one bit is the whole change.
    m_depthBuffer = m_backend->GetDevice().CreateTexture(NSRHI::TextureDesc{
        .width = m_width,
        .height = m_height,
        .format = kDepthFormat,
        .usage = NSRHI::ETextureUsage::DepthStencil
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

    // Uploaded once, sampled forever — previously it got both bits by
    // accident, since the old desc handed SAMPLED | TRANSFER_DST to every
    // non-depth texture whether or not it needed them.
    m_checkerTexture = device.CreateTexture(NSRHI::TextureDesc{
        .width = kTexSize,
        .height = kTexSize,
        .format = NSRHI::EFormat::R8G8B8A8_UNORM,
        .usage = NSRHI::ETextureUsage::Sampled | NSRHI::ETextureUsage::CopyDestination
    });

    // Rows are packed to the BACKEND'S required pitch, not tightly: D3D12
    // demands each row start on a 256-byte boundary, Vulkan does not care.
    // Asking rather than assuming is the whole reason
    // TextureRowPitchAlignment() exists — and it is why the staging buffer
    // is sized from the padded pitch.
    const uint32_t bpt = NSRHI::BytesPerTexel(NSRHI::EFormat::R8G8B8A8_UNORM);
    const uint32_t pitchAlign = device.TextureRowPitchAlignment();
    const uint32_t rowPitch = (kTexSize * bpt + pitchAlign - 1) & ~(pitchAlign - 1);

    m_checkerStaging = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = static_cast<size_t>(rowPitch) * kTexSize,
        .usage = NSRHI::EBufferUsage::Upload,
        .cpuVisible = true
    });

    auto* pixels = static_cast<uint8_t*>(m_checkerStaging->Map());
    for (uint32_t y = 0; y < kTexSize; ++y)
    {
        // Row base steps by the padded pitch; the gap between the end of
        // one row's texels and the next row's start is left untouched.
        uint8_t* row = pixels + static_cast<size_t>(y) * rowPitch;
        for (uint32_t x = 0; x < kTexSize; ++x)
        {
            // Two greys, not black/white: pure black merges with the
            // clear colour at glancing angles and hides the silhouette.
            const bool light = ((x / kCellPx) + (y / kCellPx)) % 2 == 0;
            uint8_t* p = row + static_cast<size_t>(x) * bpt;
            p[0] = p[1] = p[2] = light ? 220 : 45;
            p[3] = 255;
        }
    }
    m_checkerStaging->Unmap();
    m_checkerRowPitch = rowPitch;
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


void Renderer::Shutdown()
{
    // Drain first: these resources may still be referenced by a command
    // buffer the GPU has not finished, and destroying them under it is
    // undefined behaviour. The backend's own Shutdown() waits too, but
    // that runs after these resets - too late.
    if (m_backend) m_backend->WaitForGPU();

    // After the drain, before the device goes: the pass holds textures
    // and heap slots. The context outlives it by a line because
    // ImGuiRenderer touches ImGui::GetIO() on the way out.
    m_imgui.Shutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();

    // Then release front-end-owned GPU resources, before the backend (and
    // with it the device) goes away.
    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_dynamicVertexBuffer.reset();
    m_dynamicIndexBuffer.reset();
    m_uploadBuffer.reset();
    m_checkerStaging.reset();
    m_checkerTexture.reset();
    m_renderTarget.reset();
    m_depthBuffer.reset();
    m_pipeline.reset();
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
    const uint32_t frameIndex = m_backend->FrameIndex();
    m_descriptors.BeginFrame(frameIndex);
    m_constants.BeginFrame(frameIndex);
    m_dynamicVerts.BeginFrame(frameIndex);
    m_dynamicIndices.BeginFrame(frameIndex);
    m_uploads.BeginFrame(frameIndex);

    // Closes the frame app::OnUpdate opened with NewFrame + its UI calls.
    // Render() here rather than there because everything downstream —
    // UpdateTextures, the draw walk — consumes what it produces, and
    // keeping the pair adjacent to its consumers makes the ordering
    // constraint visible instead of remote.
    ImGui::Render();

    // Texture work goes here, before BeginRendering — copies are illegal
    // inside a dynamic-rendering pass. Unlike ImGui's vendored backends,
    // which spin up a private command list and block, this rides the
    // frame's own list.
    m_imgui.UpdateTextures(*m_cmd);

    // One-shot uploads, recorded BEFORE BeginRendering — copies are
    // illegal inside a dynamic-rendering pass. Undefined as the source
    // state because the image's current contents are garbage anyway;
    // ShaderResource afterwards so this same frame can already sample it.
    if (m_checkerUploadPending)
    {
        m_cmd->TransitionTexture(m_checkerTexture.get(),
            NSRHI::EResourceState::Undefined, NSRHI::EResourceState::CopyDestination);
        // Whole texture, expressed as the region {0,0,w,h} — the same call
        // ImGui will use for a dirty sub-rect of its font atlas, differing
        // only in the numbers.
        m_cmd->CopyBufferToTexture(m_checkerTexture.get(), m_checkerStaging.get(),
            NSRHI::TextureRegion{
                .width = m_checkerTexture->Width(),
                .height = m_checkerTexture->Height()
            },
            m_checkerRowPitch);
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

    // Back to the checkerboard now that step B has proven the atlas path
    // — the cube is the control again, and the UI has to be visibly
    // distinct from it rather than sharing a texture.
    const uint32_t textureIndex = m_checkerSlot.index;

    // Static buffers again: the cube's geometry never changes, so paying
    // a per-frame copy for it was only ever the control that proved the
    // ring. ImGui is the ring's real consumer now.
    m_cmd->SetVertexBuffer(m_vertexBuffer.get(), m_vertexStride);
    m_cmd->SetIndexBuffer(m_indexBuffer.get(), false);   // false = 16-bit
    m_cmd->SetRootConstants(0, 1, &textureIndex);
    m_cmd->DrawIndexed(m_indexCount);


    // The UI last, so it overlays everything. Inside the rendering scope
    // — its geometry is ordinary draws, unlike the texture uploads which
    // had to happen before BeginRendering.
    m_imgui.Render(*m_cmd);

    // ImGui leaves a per-command scissor behind. Restore full-viewport
    // scissor so the next frame's scene draws are not clipped to
    // whatever the last UI rectangle happened to be — a stale-state bug
    // that would look like random geometry disappearing.
    m_cmd->SetScissor(NSRHI::ScissorRect{
        .left = 0, .top = 0,
        .right = static_cast<int32_t>(m_width),
        .bottom = static_cast<int32_t>(m_height)
    });
}

void Renderer::EndFrame()
{
    if (not m_cmd) return;

    m_cmd->EndRendering();
    m_cmd = nullptr;

    m_backend->EndFrame(*m_renderTarget);
}
