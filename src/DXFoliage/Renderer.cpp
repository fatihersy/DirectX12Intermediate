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

    CreateFrameTargets();
    CreateTriangleResources();
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

void Renderer::CreateTriangleResources()
{
    NSRHI::IDevice& device = m_backend->GetDevice();

    // Empty layout — these shaders bind no resources at all.
        // Two 32-bit values: a float2 screen-space offset the vertex shader
    // adds. Small enough to be a push constant on Vulkan and a root
    // constant on DX12, which is what this layout describes.
    m_pipelineLayout = device.CreatePipelineLayout(NSRHI::PipelineLayoutDesc{
        // A full float4x4: 16 values, 64 bytes. Vulkan only guarantees
        // 128 bytes of push-constant space, so one matrix fits comfortably
        // but a separate model/view/projection trio (192) would not - that
        // will need a uniform buffer.
        .num32BitRootConstants = 16
    });

    m_vertexStride = sizeof(DemoVertex);

    m_pipeline = device.CreateGraphicsPipeline(NSRHI::GraphicsPipelineDesc{
        .vertexShader = { L"vert.hlsl", L"mainVS", NSRHI::EShaderStage::Vertex },
        .pixelShader = { L"pixel.hlsl", L"mainPS", NSRHI::EShaderStage::Pixel },
        .vertexAttributes = {
            { "POSITION", NSRHI::EFormat::R32G32B32_FLOAT, 0 },
            { "COLOR", NSRHI::EFormat::R32G32B32A32_FLOAT, 12 }
        },
        .vertexStrideBytes = m_vertexStride,
        .topology = NSRHI::EPrimitiveTopology::TriangleList,
        // Ask the backend rather than assuming: the swapchain picks
        // whatever the surface supports (B8G8R8A8 on this compositor,
        // R8G8B8A8 elsewhere) and a mismatch here is a validation error.
        .colorTargetFormats = { m_backend->BackBufferFormat() },
        // Must match the depth texture's format: Vulkan bakes attachment
        // formats into the pipeline, so a mismatch is a validation error
        // rather than a silent artefact.
        .depthTargetFormat = kDepthFormat,
        .depthTestEnabled = true,
        .depthWriteEnabled = true,
        .layout = m_pipelineLayout.get()
    });

    // A unit cube centred on the origin. Eight corners, each a distinct
    // colour so faces are told apart at a glance - which is what makes a
    // missing depth test visible rather than merely suspected.
    //
    // World-space units, with no aspect correction baked in: that belongs
    // in the projection matrix, and applying it here too would squash the
    // geometry horizontally.
    const DemoVertex triangleVertices[]{
        { { -0.5f, -0.5f, -0.5f }, { 0.f, 0.f, 0.f, 1.f } },  // 0
        { {  0.5f, -0.5f, -0.5f }, { 1.f, 0.f, 0.f, 1.f } },  // 1
        { {  0.5f,  0.5f, -0.5f }, { 1.f, 1.f, 0.f, 1.f } },  // 2
        { { -0.5f,  0.5f, -0.5f }, { 0.f, 1.f, 0.f, 1.f } },  // 3
        { { -0.5f, -0.5f,  0.5f }, { 0.f, 0.f, 1.f, 1.f } },  // 4
        { {  0.5f, -0.5f,  0.5f }, { 1.f, 0.f, 1.f, 1.f } },  // 5
        { {  0.5f,  0.5f,  0.5f }, { 1.f, 1.f, 1.f, 1.f } },  // 6
        { { -0.5f,  0.5f,  0.5f }, { 0.f, 1.f, 1.f, 1.f } },  // 7
    };

    m_vertexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(triangleVertices),
        .usage = NSRHI::EBufferUsage::Vertex,
        .cpuVisible = true
    });

    void* mapped = m_vertexBuffer->Map();
    std::memcpy(mapped, triangleVertices, sizeof(triangleVertices));
    m_vertexBuffer->Unmap();

    // Indexed drawing, exercising SetIndexBuffer/DrawIndexed. For one
    // triangle this is pure overhead - the point is that the mechanism is
    // proven on known-good geometry before a cube depends on it. 16-bit
    // because a cube needs 36 indices, nowhere near the 65535 limit.
    // 12 triangles, wound clockwise when viewed from OUTSIDE - matching
    // the pipeline's FRONT_FACE_CLOCKWISE, so every outward face survives
    // culling and every inward one is discarded. Get a face backwards and
    // it simply vanishes, which is a quick way to spot a mistake.
    const uint16_t triangleIndices[]{
        0, 2, 1,  0, 3, 2,   // back   (-Z)
        4, 5, 6,  4, 6, 7,   // front  (+Z)
        0, 4, 7,  0, 7, 3,   // left   (-X)
        1, 2, 6,  1, 6, 5,   // right  (+X)
        3, 7, 6,  3, 6, 2,   // top    (+Y)
        0, 1, 5,  0, 5, 4,   // bottom (-Y)
    };
    m_indexCount = static_cast<uint32_t>(std::size(triangleIndices));

    m_indexBuffer = device.CreateBuffer(NSRHI::BufferDesc{
        .sizeBytes = sizeof(triangleIndices),
        .usage = NSRHI::EBufferUsage::Index,
        .cpuVisible = true
    });

    void* mappedIndices = m_indexBuffer->Map();
    std::memcpy(mappedIndices, triangleIndices, sizeof(triangleIndices));
    m_indexBuffer->Unmap();
}

// TEMP-BLEND: the observable control for EBlendMode. ImGui cannot render a
// single widget without alpha blending, so this proves the state actually
// reaches the GPU before anything depends on it - the whole of this
// function goes when the ImGui pass lands.
void Renderer::CreateBlendProofResources()
{
    NSRHI::IDevice& device = m_backend->GetDevice();

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
        .vertexStrideBytes = m_vertexStride,
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
    m_renderTarget.reset();
    m_depthBuffer.reset();
    m_pipeline.reset();
    m_blendPipeline.reset();  // TEMP-BLEND
    // Layout last: both pipelines were built against it.
    m_pipelineLayout.reset();

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

    m_cmd->SetPipeline(m_pipeline.get());

    // TEMP-MTX: an animated rotation. A spin proves more than a static
    // angle - it shows the matrix is rebuilt and re-pushed every frame,
    // not baked once at startup.
    static const auto tempStart = std::chrono::steady_clock::now();  // TEMP-MTX
    const float tempSeconds = std::chrono::duration<float>(  // TEMP-MTX
        std::chrono::steady_clock::now() - tempStart).count();  // TEMP-MTX

    // Rotating about Y rather than Z: a Z spin looks identical under
    // orthographic and perspective, whereas turning edge-on foreshortens
    // only if the projection is doing its job.
    const NSMath::Float4x4 model = NSMath::Multiply(  // TEMP-MTX
        NSMath::RotationX(tempSeconds * 0.6f), NSMath::RotationY(tempSeconds));  // TEMP-MTX
    const NSMath::Float4x4 view = NSMath::LookAtLH(  // TEMP-MTX
        { 0.f, 0.f, -2.5f }, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });  // TEMP-MTX
    const NSMath::Float4x4 proj = NSMath::PerspectiveFovLH(  // TEMP-MTX
        1.0472f,  // 60 degrees  // TEMP-MTX
        static_cast<float>(m_width) / static_cast<float>(m_height),  // TEMP-MTX
        0.1f, 100.f);  // TEMP-MTX

    // TWO cubes, and the far one drawn SECOND. A single convex object
    // needs no depth test - back-face culling alone resolves it, which is
    // why one cube looked correct with depth disabled. Two overlapping
    // objects is the case depth actually exists for: without it, draw
    // ORDER decides what you see; with it, DISTANCE does.
    const NSMath::Float4x4 viewProj = NSMath::Multiply(view, proj);  // TEMP-MTX

    // Camera sits at -Z looking toward +Z, so smaller z is nearer.
    const NSMath::Float4x4 nearCube = NSMath::Multiply(  // TEMP-MTX
        NSMath::Multiply(model, NSMath::Translation(-0.35f, 0.f, -0.6f)), viewProj);  // TEMP-MTX
    const NSMath::Float4x4 farCube = NSMath::Multiply(  // TEMP-MTX
        NSMath::Multiply(model, NSMath::Translation( 0.35f, 0.f,  0.6f)), viewProj);  // TEMP-MTX

    // Buffers first: both draws share the same geometry, so they are
    // bound once and only the push constant changes between them.
    m_cmd->SetVertexBuffer(m_vertexBuffer.get(), m_vertexStride);
    m_cmd->SetIndexBuffer(m_indexBuffer.get(), false);   // false = 16-bit

    m_cmd->SetRootConstants(0, 16, &nearCube);  // TEMP-MTX
    m_cmd->DrawIndexed(m_indexCount);  // TEMP-MTX

    // Farther away, drawn later. Without depth it wins purely by being
    // last, and visibly overlaps the nearer cube.
    m_cmd->SetRootConstants(0, 16, &farCube);  // TEMP-MTX
    m_cmd->DrawIndexed(m_indexCount);  // TEMP-MTX

    // TEMP-BLEND: drawn LAST and over the cubes, which are the known-good
    // control. Identity transform - the quad's vertices are already clip
    // space, so nothing here depends on the matrix path being right.
    const NSMath::Float4x4 identity = NSMath::Float4x4::Identity();  // TEMP-BLEND
    m_cmd->SetPipeline(m_blendPipeline.get());  // TEMP-BLEND
    m_cmd->SetVertexBuffer(m_quadVertexBuffer.get(), m_vertexStride);  // TEMP-BLEND
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
