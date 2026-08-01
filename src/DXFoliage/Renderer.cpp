#include "stdafx.h"
#include "Renderer.h"

#include "rhi/RendererBackendFactory.h"

#include <cstring>

namespace
{
    // Matches the pipeline's vertex attributes below: POSITION (3 floats)
    // at offset 0, COLOR (4 floats) at offset 12. Declared locally because
    // it's temporary demo content — the equivalent in RendererTypes.h
    // (NSDebug::Vertex) lives in a header that still carries DX12 types.
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

    CreateTriangleResources();

    return true;
}

void Renderer::CreateTriangleResources()
{
    NSRHI::IDevice& device = m_backend->GetDevice();

    // Empty layout — these shaders bind no resources at all.
        // Two 32-bit values: a float2 screen-space offset the vertex shader
    // adds. Small enough to be a push constant on Vulkan and a root
    // constant on DX12, which is what this layout describes.
    m_pipelineLayout = device.CreatePipelineLayout(NSRHI::PipelineLayoutDesc{
        .num32BitRootConstants = 2
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
        .depthTargetFormat = NSRHI::EFormat::Unknown,
        .depthTestEnabled = false,
        .depthWriteEnabled = false,
        .layout = m_pipelineLayout.get()
    });

    const float aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

    const DemoVertex triangleVertices[]{
        { { 0.00f,  0.25f * aspectRatio, 0.f }, { 1.f, 0.f, 0.f, 1.f } },
        { { 0.25f, -0.25f * aspectRatio, 0.f }, { 0.f, 1.f, 0.f, 1.f } },
        { {-0.25f, -0.25f * aspectRatio, 0.f }, { 0.f, 0.f, 1.f, 1.f } },
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
    const uint16_t triangleIndices[]{ 0, 1, 2 };
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
    m_pipeline.reset();
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

    if (m_backend) m_backend->Resize(width, height);
}

void Renderer::BeginFrame()
{
    if (not m_backend) return;

    m_cmd = &m_backend->BeginFrame();

    // The backend already transitioned the backbuffer to a render target;
    // the front-end decides what to render into it.
    NSRHI::RenderingAttachment color{};
    color.target = &m_backend->CurrentBackBuffer();
    color.clear = true;
    color.clearColor = { 0.f, 0.f, 0.f, 0.f };

    m_cmd->BeginRendering({ color }, nullptr);

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

    // TEMP-PUSH: a constant, visible nudge to the right. If push constants
    // are not reaching the shader the triangle stays centred.
    const float offset[2]{ 0.4f, 0.0f };  // TEMP-PUSH
    m_cmd->SetRootConstants(0, 2, offset);  // TEMP-PUSH
    m_cmd->SetVertexBuffer(m_vertexBuffer.get(), m_vertexStride);
    m_cmd->SetIndexBuffer(m_indexBuffer.get(), false);   // false = 16-bit
    m_cmd->DrawIndexed(m_indexCount);
}

void Renderer::EndFrame()
{
    if (not m_cmd) return;

    m_cmd->EndRendering();
    m_cmd = nullptr;

    m_backend->EndFrame();
}
