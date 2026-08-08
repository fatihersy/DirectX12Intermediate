#include "stdafx.h"
#include "ImGuiRenderer.h"

#include "Logger.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
    // ImGui hands us RGBA32 (four unsigned bytes) unless asked otherwise,
    // and that maps straight onto our default colour format. Alpha8 would
    // need an R8_UNORM EFormat, which does not exist yet and has no other
    // caller — the assert below is the reminder rather than a guess at
    // what that path should look like.
    constexpr NSRHI::EFormat kAtlasFormat = NSRHI::EFormat::R8G8B8A8_UNORM;
}

ImGuiRenderer::~ImGuiRenderer()
{
    Shutdown();
}

bool ImGuiRenderer::Initialize(NSRHI::IDevice& device,
                           NSRHI::IDescriptorHeap& heap,
                           NSDescriptor::RingHeap& descriptors,
                           NSAllocator::RingAllocator& uploads,
                           NSAllocator::RingAllocator& vertices,
                           NSAllocator::RingAllocator& indices,
                           NSRHI::EFormat colorTargetFormat,
                           NSRHI::EFormat depthTargetFormat)
{
    m_device = &device;
    m_heap = &heap;
    m_descriptors = &descriptors;
    m_uploads = &uploads;
    m_vertices = &vertices;
    m_indices = &indices;

    // 17 dwords: float4x4 projection + the texture index, pushed per
    // draw command. No constant-buffer slots — everything ImGui needs
    // per command fits in push constants, which is cheaper than a ring
    // allocation per command.
    m_layout = device.CreatePipelineLayout(NSRHI::PipelineLayoutDesc{
        .num32BitRootConstants = 17,
        .usesBindlessDescriptorTable = true,
        .bindlessHeap = &heap
    });

    m_pipeline = device.CreateGraphicsPipeline(NSRHI::GraphicsPipelineDesc{
        .vertexShader = { L"imgui.hlsl", L"mainVS", NSRHI::EShaderStage::Vertex },
        .pixelShader = { L"imgui.hlsl", L"mainPS", NSRHI::EShaderStage::Pixel },
        // Exactly ImDrawVert's layout: 20 bytes, colour as packed RGBA8
        // that the hardware expands to 0..1 — the first non-float vertex
        // attribute this renderer has used.
        .vertexAttributes = {
            { "POSITION", NSRHI::EFormat::R32G32_FLOAT,   0 },
            { "TEXCOORD", NSRHI::EFormat::R32G32_FLOAT,   8 },
            { "COLOR",    NSRHI::EFormat::R8G8B8A8_UNORM, 16 }
        },
        .vertexStrideBytes = sizeof(ImDrawVert),
        .topology = NSRHI::EPrimitiveTopology::TriangleList,
        .colorTargetFormats = { colorTargetFormat },
        .depthTargetFormat = depthTargetFormat,
        // No depth at all: the UI is an overlay, drawn last, and must not
        // be occluded by or write into the scene's depth.
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .blendMode = NSRHI::EBlendMode::AlphaBlend,
        // ImGui's triangle winding is not guaranteed consistent, and
        // every official backend disables culling for exactly this
        // reason. With Back culling roughly half the UI vanishes.
        .cullMode = NSRHI::ECullMode::None,
        .layout = m_layout.get()
    });

    // Tells ImGui it may create and update textures at runtime rather
    // than baking one immutable atlas up front. Without it ImGui falls
    // back to its pre-1.92 behaviour (Dear ImGui 1.92 introduced the
    // ImTextureData lifecycle; we pin 1.92.5 in conanfile.py) and never
    // populates ImDrawData::Textures, which is what UpdateTextures reads.
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetIO().BackendRendererName = "DXFoliage-RHI";

    return true;
}

void ImGuiRenderer::Shutdown()
{
    // The caller has already drained the GPU (Renderer::Shutdown waits
    // before releasing anything front-end owned), so these are safe to
    // drop without deferred-destroy machinery.
    for (auto& [index, backed] : m_textures)
    {
        if (backed.slot.IsValid() and m_descriptors) m_descriptors->FreeStatic(backed.slot);
    }
    m_textures.clear();
    m_fontAtlasSlot = {};

    // Pipeline before layout: the pipeline was built against it.
    m_pipeline.reset();
    m_layout.reset();

    m_device = nullptr;
    m_heap = nullptr;
    m_descriptors = nullptr;
    m_uploads = nullptr;
}

void ImGuiRenderer::UpdateTextures(NSRHI::ICommandList& cmd)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (not drawData or not drawData->Textures) return;

    for (ImTextureData* tex : *drawData->Textures)
    {
        switch (tex->Status)
        {
            case ImTextureStatus_WantCreate:
                CreateTexture(cmd, tex);
                break;

            case ImTextureStatus_WantUpdates:
                // ImGui's own backends upload the single UpdateRect
                // bounding box rather than walking Updates[] — cheaper in
                // calls, slightly more pixels. Same choice here; the
                // per-rect loop is a one-line change if it ever matters.
                UploadRegion(cmd, tex,
                    static_cast<uint32_t>(tex->UpdateRect.x), static_cast<uint32_t>(tex->UpdateRect.y),
                    static_cast<uint32_t>(tex->UpdateRect.w), static_cast<uint32_t>(tex->UpdateRect.h));
                tex->SetStatus(ImTextureStatus_OK);
                break;

            case ImTextureStatus_WantDestroy:
                DestroyTexture(tex);
                break;

            case ImTextureStatus_OK:
            case ImTextureStatus_Destroyed:
            default:
                break;
        }
    }
}

void ImGuiRenderer::Render(NSRHI::ICommandList& cmd)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (not drawData or drawData->CmdListsCount == 0) return;

    // Framebuffer pixels, not logical units — DisplaySize is logical and
    // FramebufferScale converts. They differ on a HiDPI display, which is
    // exactly the conflation the plan warns about for swapchains.
    const float fbWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
    const float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

    // One-shot: several early returns below look identical from outside
    // (nothing on screen, no validation complaint). Logging the inputs
    // once says which one, instead of costing a build/run cycle to guess.
    static bool loggedOnce = false;
    if (not loggedOnce)
    {
        loggedOnce = true;
        g_FInfo("ImGuiRenderer: cmdLists=%d vtx=%d idx=%d fb=%.0fx%.0f scale=%.2f,%.2f",
            drawData->CmdListsCount, drawData->TotalVtxCount, drawData->TotalIdxCount,
            fbWidth, fbHeight, drawData->FramebufferScale.x, drawData->FramebufferScale.y);
    }

    if (fbWidth <= 0.0f or fbHeight <= 0.0f) return;

    // Every command list's geometry goes into ONE ring allocation each,
    // concatenated, so the buffers are bound once and each command draws
    // its slice via firstIndex/vertexOffset. The alternative — binding
    // per command list — costs a bind per window for no gain.
    const size_t totalVtxBytes = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
    const size_t totalIdxBytes = static_cast<size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);
    if (totalVtxBytes == 0 or totalIdxBytes == 0) return;

    NSAllocator::Ctx vtxAlloc = m_vertices->Allocate(totalVtxBytes);
    NSAllocator::Ctx idxAlloc = m_indices->Allocate(totalIdxBytes);
    if (not vtxAlloc or not idxAlloc)
    {
        g_FError("ImGuiRenderer: geometry ring exhausted (%zu vtx / %zu idx bytes)",
            totalVtxBytes, totalIdxBytes);
        return;
    }

    auto* vtxDst = static_cast<ImDrawVert*>(vtxAlloc.cpuAddr);
    auto* idxDst = static_cast<ImDrawIdx*>(idxAlloc.cpuAddr);
    // Running totals, because each command's offsets are relative to the
    // whole concatenated stream rather than its own command list.
    uint32_t vtxBase = 0;
    uint32_t idxBase = 0;

    struct DrawSlice
    {
        uint32_t elemCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t textureIndex;
        NSRHI::ScissorRect scissor;
    };
    std::vector<DrawSlice> slices;

    for (int n = 0; n < drawData->CmdListsCount; ++n)
    {
        const ImDrawList* list = drawData->CmdLists[n];
        std::memcpy(vtxDst + vtxBase, list->VtxBuffer.Data,
            static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
        std::memcpy(idxDst + idxBase, list->IdxBuffer.Data,
            static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx));

        for (const ImDrawCmd& drawCmd : list->CmdBuffer)
        {
            // A user callback draws itself; we have no caller for that
            // yet, and silently skipping is better than mis-drawing.
            if (drawCmd.UserCallback != nullptr) continue;
            if (drawCmd.ElemCount == 0) continue;

            // Clip rects arrive in ImGui's coordinate space, offset by
            // DisplayPos (non-zero only with multi-viewport, which this
            // master-branch build does not have) and in logical units.
            ImVec2 clipMin(
                (drawCmd.ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                (drawCmd.ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.y);
            ImVec2 clipMax(
                (drawCmd.ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.x,
                (drawCmd.ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y);

            // Clamp before the cast: a negative scissor is a validation
            // error on both APIs, and ImGui does emit rects that start
            // off-screen when a window is dragged past the edge.
            clipMin.x = std::max(clipMin.x, 0.0f);
            clipMin.y = std::max(clipMin.y, 0.0f);
            clipMax.x = std::min(clipMax.x, fbWidth);
            clipMax.y = std::min(clipMax.y, fbHeight);
            if (clipMax.x <= clipMin.x or clipMax.y <= clipMin.y) continue;

            slices.push_back(DrawSlice{
                .elemCount = drawCmd.ElemCount,
                .firstIndex = idxBase + drawCmd.IdxOffset,
                .vertexOffset = static_cast<int32_t>(vtxBase + drawCmd.VtxOffset),
                .textureIndex = static_cast<uint32_t>(drawCmd.GetTexID()),
                .scissor = NSRHI::ScissorRect{
                    .left = static_cast<int32_t>(clipMin.x),
                    .top = static_cast<int32_t>(clipMin.y),
                    .right = static_cast<int32_t>(clipMax.x),
                    .bottom = static_cast<int32_t>(clipMax.y)
                }
            });
        }

        vtxBase += static_cast<uint32_t>(list->VtxBuffer.Size);
        idxBase += static_cast<uint32_t>(list->IdxBuffer.Size);
    }

    if (slices.empty()) return;

    // Orthographic, screen-space, row-vector to match core/Math.h's
    // convention (the shader does mul(matrix, vector)). Maps
    // [0..fbWidth] x [0..fbHeight] with Y DOWN onto clip space with Y up
    // — hence the negated Y scale and the +1 translate.
    const float L = drawData->DisplayPos.x;
    const float T = drawData->DisplayPos.y;
    const float R = L + drawData->DisplaySize.x;
    const float B = T + drawData->DisplaySize.y;

    struct ImGuiPush
    {
        float projection[4][4];
        uint32_t textureIndex;
    } push{};
    // Translation in ROW 3, not column 3. This is a row-vector matrix
    // uploaded untransposed; HLSL's column-major packing transposes it on
    // arrival, and the shader's mul(matrix, vector) compensates — the
    // convention documented in core/Math.h, and the same one ImGui's own
    // backends use. Getting it backwards puts the translation into the
    // shader-side matrix's bottom ROW, which makes w vary per vertex and
    // collapses every triangle to a point: invisible, and perfectly valid
    // as far as the validation layer is concerned.
    push.projection[0][0] = 2.0f / (R - L);
    push.projection[1][1] = 2.0f / (T - B);
    push.projection[2][2] = 0.5f;
    push.projection[3][3] = 1.0f;
    push.projection[3][0] = (R + L) / (L - R);
    push.projection[3][1] = (T + B) / (B - T);
    push.projection[3][2] = 0.5f;

    cmd.SetPipeline(m_pipeline.get());
    cmd.SetVertexBuffer(m_vertices->Buffer(), sizeof(ImDrawVert),
        vtxAlloc.offsetBytes, totalVtxBytes);
    cmd.SetIndexBuffer(m_indices->Buffer(), sizeof(ImDrawIdx) == 4,
        idxAlloc.offsetBytes, totalIdxBytes);

    for (const DrawSlice& slice : slices)
    {
        cmd.SetScissor(slice.scissor);
        push.textureIndex = slice.textureIndex;
        cmd.SetRootConstants(0, 17, &push);
        cmd.DrawIndexed(slice.elemCount, 1, slice.firstIndex, slice.vertexOffset);
    }
}

void ImGuiRenderer::CreateTexture(NSRHI::ICommandList& cmd, ImTextureData* tex)
{
    ASSERT(tex->Format == ImTextureFormat_RGBA32,
        "Only RGBA32 atlases are handled; Alpha8 needs an R8_UNORM EFormat");

    // A static slot: an atlas outlives every frame, which is exactly what
    // the static region is for (see the in-flight contract in
    // Descriptor.h — static slots are write-once until freed at a drain).
    NSRHI::DescriptorHandle slot = m_descriptors->AllocateStatic();
    if (not slot.IsValid())
    {
        g_FError("ImGuiRenderer: descriptor heap static region exhausted");
        return;
    }

    BackedTexture backed{};
    backed.slot = slot;
    backed.texture = m_device->CreateTexture(NSRHI::TextureDesc{
        .width = static_cast<uint32_t>(tex->Width),
        .height = static_cast<uint32_t>(tex->Height),
        .format = kAtlasFormat,
        .usage = NSRHI::ETextureUsage::Sampled | NSRHI::ETextureUsage::CopyDestination
    });

    m_device->CreateShaderResourceView(*m_heap, m_heap->At(slot.index), backed.texture.get());

    const uint32_t index = slot.index;
    m_textures[index] = std::move(backed);

    // ImTextureID is a plain ImU64, so the descriptor index goes straight
    // in with no wrapper — the whole point of bindless. ImDrawCmd will
    // hand this number back at draw time and it becomes a root constant.
    tex->SetTexID(static_cast<ImTextureID>(index));

    // Full-texture upload for a create; ImGui's backends do the same, and
    // it doubles as clearing the parts no glyph has claimed yet.
    UploadRegion(cmd, tex, 0, 0, static_cast<uint32_t>(tex->Width), static_cast<uint32_t>(tex->Height));
    tex->SetStatus(ImTextureStatus_OK);

    if (not m_fontAtlasSlot.IsValid()) m_fontAtlasSlot = slot;

    g_FInfo("ImGuiRenderer: atlas %dx%d created in descriptor slot %u",
        tex->Width, tex->Height, index);
}

void ImGuiRenderer::UploadRegion(NSRHI::ICommandList& cmd, ImTextureData* tex,
                             uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (w == 0 or h == 0) return;

    auto found = m_textures.find(static_cast<uint32_t>(tex->GetTexID()));
    if (found == m_textures.end()) return;
    NSRHI::ITexture* target = found->second.texture.get();

    const uint32_t bpt = NSRHI::BytesPerTexel(kAtlasFormat);
    const uint32_t pitchAlign = m_device->TextureRowPitchAlignment();
    const uint32_t rowPitch = (w * bpt + pitchAlign - 1) & ~(pitchAlign - 1);

    NSAllocator::Ctx staging = m_uploads->Allocate(static_cast<size_t>(rowPitch) * h);
    if (not staging)
    {
        g_FError("ImGuiRenderer: upload ring exhausted for a %ux%u region", w, h);
        return;
    }

    // Row by row, always — the source pixels are strided by the FULL
    // atlas width, so even a tightly-packed destination cannot be one
    // memcpy. ImGui's own backends have the identical loop; only the
    // destination pitch differs between them, which is precisely what
    // TextureRowPitchAlignment() abstracts.
    auto* dst = static_cast<uint8_t*>(staging.cpuAddr);
    for (uint32_t row = 0; row < h; ++row)
    {
        std::memcpy(dst + static_cast<size_t>(row) * rowPitch,
            tex->GetPixelsAt(static_cast<int>(x), static_cast<int>(y + row)),
            static_cast<size_t>(w) * bpt);
    }

    // Undefined as the before-state on create (contents are garbage
    // anyway); ShaderResource after, so the same frame can sample it.
    cmd.TransitionTexture(target,
        NSRHI::EResourceState::Undefined, NSRHI::EResourceState::CopyDestination);
    cmd.CopyBufferToTexture(target, m_uploads->Buffer(),
        NSRHI::TextureRegion{ .x = x, .y = y, .width = w, .height = h },
        rowPitch, staging.offsetBytes);
    cmd.TransitionTexture(target,
        NSRHI::EResourceState::CopyDestination, NSRHI::EResourceState::ShaderResource);
}

void ImGuiRenderer::DestroyTexture(ImTextureData* tex)
{
    auto found = m_textures.find(static_cast<uint32_t>(tex->GetTexID()));
    if (found == m_textures.end()) return;

    // KNOWN GAP: this releases immediately, while frames using the
    // texture may still be in flight. Safe today only because nothing
    // destroys a texture mid-run — ImGui asks for this when a font is
    // rebuilt or an atlas page retires, neither of which happens yet.
    // The fix is a deferred-destroy queue keyed on the frame fence
    // (gap #8 in DESCRIPTORS.md); do it before dynamic fonts land.
    m_descriptors->FreeStatic(found->second.slot);
    m_textures.erase(found);

    tex->SetTexID(ImTextureID_Invalid);
    tex->SetStatus(ImTextureStatus_Destroyed);
}
