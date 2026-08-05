#pragma once

#include "IPipelineLayout.h"
#include "RHITypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// The compiled shader program plus the fixed-function state around it
// (what shape the vertex data is, whether depth testing is on, ...) — the
// neutral equivalent of a D3D12 Pipeline State Object / a VkPipeline.
namespace NSRHI
{
    enum class EShaderStage : uint8_t
    {
        Vertex,
        Pixel,
    };

    // A reference to shader *source*, not compiled bytecode — the backend
    // compiles it to its own target (DXIL on DX12, SPIR-V on Vulkan),
    // deriving the profile from `stage`. This keeps the front-end
    // API-agnostic (it never knows DXIL vs SPIR-V exists) and is the same
    // seam the DXIL/SPIR-V dual-target task builds on.
    // (filename/entryPoint are wide to match the existing DXC path; the
    // string type may need revisiting when the Linux DXC path lands.)
    struct ShaderSource
    {
        const wchar_t* filename{};    // e.g. L"vert.hlsl"
        const wchar_t* entryPoint{};  // e.g. L"mainVS"
        EShaderStage stage{ EShaderStage::Vertex };

        bool IsValid() const { return filename != nullptr; }
    };

    enum class EPrimitiveTopology : uint8_t
    {
        TriangleList,
        LineList,
    };

    // Deliberately a small enum rather than a full source/dest/op matrix.
    // Every blending site that actually exists wants the same thing:
    // DXTerrain enables blending in 2 of its 11 pipelines and both are
    // straight alpha blending, and ImGui needs exactly this too. A general
    // blend-factor struct here would be an interface designed against no
    // caller — add a mode when something needs one.
    //
    // (DXTerrain's geometry pass differs in one detail: it uses a
    // destination *alpha* factor of ZERO where this uses InvSrcAlpha. That
    // only matters when the render target's alpha channel is later read,
    // which nothing does yet. Revisit when the HDR/tonemap path lands.)
    enum class EBlendMode : uint8_t
    {
        Opaque,
        AlphaBlend,
    };

    // Back-face culling was hardcoded until ImGui needed None. Not an
    // ImGui accommodation though — DXTerrain uses all three, and None
    // MORE than Back (5 sites vs 2), because two-sided geometry is the
    // norm for foliage: a leaf card has to survive being seen from
    // behind. Front exists for the same reason it does there: rendering
    // the inside of a skybox or cubemap capture.
    enum class ECullMode : uint8_t
    {
        Back,   // the default everything used before this existed
        None,
        Front,
    };

    // One entry per vertex input, matching an HLSL semantic
    // (e.g. "POSITION", "COLOR") to where that data lives in the vertex
    // buffer. DXC assigns SPIR-V input locations in declaration order,
    // which already matches the D3D12 input layout order used today, so
    // no reordering is needed between the two backends.
    struct VertexAttribute
    {
        const char* semanticName{};
        EFormat format{};
        uint32_t offsetBytes{};
    };

    struct GraphicsPipelineDesc
    {
        // Shader source — the backend compiles these to its target.
        // pixelShader is optional (IsValid() == false means none).
        ShaderSource vertexShader;
        ShaderSource pixelShader;

        std::vector<VertexAttribute> vertexAttributes;
        uint32_t vertexStrideBytes{};

        EPrimitiveTopology topology{ EPrimitiveTopology::TriangleList };

        // The formats of whatever BeginRendering() will target when this
        // pipeline is bound — needed up front on Vulkan (dynamic
        // rendering still requires attachment formats at pipeline-creation
        // time), and matches the RTVFormats/DSVFormat fields DX12's PSO
        // desc already requires today.
        std::vector<EFormat> colorTargetFormats;
        EFormat depthTargetFormat{ EFormat::Unknown };

        bool depthTestEnabled{ true };
        bool depthWriteEnabled{ true };

        EBlendMode blendMode{ EBlendMode::Opaque };
        ECullMode cullMode{ ECullMode::Back };

        IPipelineLayout* layout{ nullptr };
    };

    class IPipeline
    {
    public:
        virtual ~IPipeline() = default;
    };
}
