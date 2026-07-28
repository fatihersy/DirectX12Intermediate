#pragma once
#include "core/Defines.h"
#include "core/Math.h"
#include "core/EntityTypes.h"

namespace NSBarrier
{
    inline bool operator==(const D3D12_RESOURCE_BARRIER& lhs, const D3D12_RESOURCE_BARRIER& rhs) noexcept
    {
        switch (lhs.Type)
        {
            case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
            {
                return
                lhs.Flags == rhs.Flags
                and
                lhs.Transition.pResource == rhs.Transition.pResource
                and
                lhs.Transition.Subresource == rhs.Transition.Subresource
                and
                lhs.Transition.StateBefore == rhs.Transition.StateBefore
                and
                lhs.Transition.StateAfter == rhs.Transition.StateAfter;
            }
            case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
            {
                return
                lhs.Flags == rhs.Flags
                and
                lhs.Aliasing.pResourceBefore == rhs.Aliasing.pResourceBefore
                and
                lhs.Aliasing.pResourceAfter == rhs.Aliasing.pResourceAfter;
            }
            case D3D12_RESOURCE_BARRIER_TYPE_UAV:
            {
                return
                lhs.Flags == rhs.Flags
                and
                lhs.UAV.pResource == rhs.UAV.pResource;
            }

            default:
            return false;
        }
    }

    struct BarrierKey
    {
        std::string_view name;
        constexpr explicit BarrierKey(std::string_view n) : name(n) {}
    };

    inline constexpr BarrierKey kApp_beginModelLoad {"App.beginModelLoad"};
    inline constexpr BarrierKey kApp_endModelLoad {"App.endModelLoad"};
    inline constexpr BarrierKey kTerrain_OnInit {"App.OnInit"};
};

struct FrameConstants
{
    NSMath::Float4x4 view{};
    NSMath::Float4x4 proj{};
    NSMath::Float4 lightDir{};
    NSMath::Float4 lightColor{};
    NSMath::Float3 eye{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(FrameConstants) % 16 == 0);
static_assert(offsetof(FrameConstants, view) % 16 == 0);
static_assert(offsetof(FrameConstants, proj) % 16 == 0);
static_assert(offsetof(FrameConstants, lightDir) % 16 == 0);
static_assert(offsetof(FrameConstants, lightColor) % 16 == 0);
static_assert(offsetof(FrameConstants, eye      ) / 16 == (offsetof(FrameConstants, eye      ) + sizeof(FrameConstants::eye      ) - 1) / 16);
static_assert(offsetof(FrameConstants, PADDING_0) / 16 == (offsetof(FrameConstants, PADDING_0) + sizeof(FrameConstants::PADDING_0) - 1) / 16);

struct PostConstants
{
    NSMath::Float4x4 invViewProj{};
    NSMath::Float3 camPos{};
    uint32_t sceneColorSrvIndex{};
    uint32_t depthSrvIndex{};
    uint32_t transmittanceSrvIndex{};
    uint32_t scatteringSrvIndex{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(PostConstants) % 16 == 0);
static_assert(offsetof(PostConstants, invViewProj) % 16 == 0);
static_assert(offsetof(PostConstants, camPos               ) / 16 == (offsetof(PostConstants, camPos               ) + sizeof(PostConstants::camPos               ) - 1) / 16);
static_assert(offsetof(PostConstants, sceneColorSrvIndex   ) / 16 == (offsetof(PostConstants, sceneColorSrvIndex   ) + sizeof(PostConstants::sceneColorSrvIndex   ) - 1) / 16);
static_assert(offsetof(PostConstants, depthSrvIndex        ) / 16 == (offsetof(PostConstants, depthSrvIndex        ) + sizeof(PostConstants::depthSrvIndex        ) - 1) / 16);
static_assert(offsetof(PostConstants, transmittanceSrvIndex) / 16 == (offsetof(PostConstants, transmittanceSrvIndex) + sizeof(PostConstants::transmittanceSrvIndex) - 1) / 16);
static_assert(offsetof(PostConstants, scatteringSrvIndex   ) / 16 == (offsetof(PostConstants, scatteringSrvIndex   ) + sizeof(PostConstants::scatteringSrvIndex   ) - 1) / 16);
static_assert(offsetof(PostConstants, PADDING_0            ) / 16 == (offsetof(PostConstants, PADDING_0            ) + sizeof(PostConstants::PADDING_0            ) - 1) / 16);

struct MeshConstants
{
    NSMath::Float4x4 worldMatrix{};
    NSMath::Float3x4 normalMatrix{};
    NSMath::Float4 baseColor{};
    float metallic{};
    float roughtness{};
    float opacity{};
    uint32_t textureFlags{};
};
static_assert(sizeof(MeshConstants) % 16 == 0);
static_assert(offsetof(MeshConstants, worldMatrix) % 16 == 0);
static_assert(offsetof(MeshConstants, normalMatrix) % 16 == 0);
static_assert(offsetof(MeshConstants, baseColor) % 16 == 0);
static_assert(offsetof(MeshConstants, metallic    ) / 16 == (offsetof(MeshConstants, metallic    ) + sizeof(MeshConstants::metallic    ) - 1) / 16);
static_assert(offsetof(MeshConstants, roughtness  ) / 16 == (offsetof(MeshConstants, roughtness  ) + sizeof(MeshConstants::roughtness  ) - 1) / 16);
static_assert(offsetof(MeshConstants, opacity     ) / 16 == (offsetof(MeshConstants, opacity     ) + sizeof(MeshConstants::opacity     ) - 1) / 16);
static_assert(offsetof(MeshConstants, textureFlags) / 16 == (offsetof(MeshConstants, textureFlags) + sizeof(MeshConstants::textureFlags) - 1) / 16);

struct FrameConstantsZPrepass
{
    NSMath::Float4x4 view{};
    NSMath::Float4x4 proj{};
    NSMath::Float3 eye{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(FrameConstantsZPrepass) % 16 == 0);
static_assert(offsetof(FrameConstantsZPrepass, view) % 16 == 0);
static_assert(offsetof(FrameConstantsZPrepass, proj) % 16 == 0);
static_assert(offsetof(FrameConstantsZPrepass, eye      ) / 16 == (offsetof(FrameConstantsZPrepass, eye      ) + sizeof(FrameConstantsZPrepass::eye      ) - 1) / 16);
static_assert(offsetof(FrameConstantsZPrepass, PADDING_0) / 16 == (offsetof(FrameConstantsZPrepass, PADDING_0) + sizeof(FrameConstantsZPrepass::PADDING_0) - 1) / 16);

struct MeshConstantsZPrepass
{
    NSMath::Float4x4 worldMatrix{};
};
static_assert(sizeof(MeshConstantsZPrepass) % 16 == 0);
static_assert(offsetof(MeshConstantsZPrepass, worldMatrix) % 16 == 0);

struct AtmosphereConstants
{
    NSMath::Float3 BetaR{};
    float PADDING_0{};
    float BetaMScatter{};
    float BetaMExtinct{};
    float MieG{};
    float HR{};
    float HM{};
    float Rg{};
    float Rt{};
    float SunIntensity{};
    NSMath::Float3 SunDir{};
    float PADDING_1{};
};
static_assert(sizeof(AtmosphereConstants) % 16 == 0);
static_assert(offsetof(AtmosphereConstants, BetaR       ) / 16 == (offsetof(AtmosphereConstants, BetaR       ) + sizeof(AtmosphereConstants::BetaR       ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, PADDING_0   ) / 16 == (offsetof(AtmosphereConstants, PADDING_0   ) + sizeof(AtmosphereConstants::PADDING_0   ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, BetaMScatter) / 16 == (offsetof(AtmosphereConstants, BetaMScatter) + sizeof(AtmosphereConstants::BetaMScatter) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, BetaMExtinct) / 16 == (offsetof(AtmosphereConstants, BetaMExtinct) + sizeof(AtmosphereConstants::BetaMExtinct) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, MieG        ) / 16 == (offsetof(AtmosphereConstants, MieG        ) + sizeof(AtmosphereConstants::MieG        ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, HR          ) / 16 == (offsetof(AtmosphereConstants, HR          ) + sizeof(AtmosphereConstants::HR          ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, HM          ) / 16 == (offsetof(AtmosphereConstants, HM          ) + sizeof(AtmosphereConstants::HM          ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, Rg          ) / 16 == (offsetof(AtmosphereConstants, Rg          ) + sizeof(AtmosphereConstants::Rg          ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, Rt          ) / 16 == (offsetof(AtmosphereConstants, Rt          ) + sizeof(AtmosphereConstants::Rt          ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, SunIntensity) / 16 == (offsetof(AtmosphereConstants, SunIntensity) + sizeof(AtmosphereConstants::SunIntensity) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, SunDir      ) / 16 == (offsetof(AtmosphereConstants, SunDir      ) + sizeof(AtmosphereConstants::SunDir      ) - 1) / 16);
static_assert(offsetof(AtmosphereConstants, PADDING_1   ) / 16 == (offsetof(AtmosphereConstants, PADDING_1   ) + sizeof(AtmosphereConstants::PADDING_1   ) - 1) / 16);

inline bool operator!=(const AtmosphereConstants& lhs, AtmosphereConstants& rhs) noexcept
{
    if (not NSMath::Float3Equals(lhs.BetaR, rhs.BetaR)) return true;
    if (lhs.BetaMScatter != rhs.BetaMScatter) return true;
    if (lhs.BetaMExtinct != rhs.BetaMExtinct) return true;
    if (lhs.MieG         != rhs.MieG) return true;
    if (lhs.HR           != rhs.HR) return true;
    if (lhs.HM           != rhs.HM) return true;
    if (lhs.Rg           != rhs.Rg) return true;
    if (lhs.Rt           != rhs.Rt) return true;
    if (lhs.SunIntensity != rhs.SunIntensity) return true;
    if (not NSMath::Float3Equals(lhs.SunDir, rhs.SunDir)) return true;

    return false;
}

struct EnvCaptureConstants
{
    NSMath::Float4x4 view{};
    NSMath::Float4x4 proj{};
    NSMath::Float4 lightDir{};
    NSMath::Float4 lightColor{};
    NSMath::Float3 capturePos{};
    float PADDING_0{};
    NSMath::Float3 camPos{};
    float PADDING_1{};
};
static_assert(sizeof(EnvCaptureConstants) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, view) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, proj) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, lightDir) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, lightColor) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, capturePos) / 16 == (offsetof(EnvCaptureConstants, capturePos) + sizeof(EnvCaptureConstants::capturePos) - 1) / 16);
static_assert(offsetof(EnvCaptureConstants, PADDING_0 ) / 16 == (offsetof(EnvCaptureConstants, PADDING_0 ) + sizeof(EnvCaptureConstants::PADDING_0 ) - 1) / 16);
static_assert(offsetof(EnvCaptureConstants, camPos    ) / 16 == (offsetof(EnvCaptureConstants, camPos    ) + sizeof(EnvCaptureConstants::camPos    ) - 1) / 16);
static_assert(offsetof(EnvCaptureConstants, PADDING_1 ) / 16 == (offsetof(EnvCaptureConstants, PADDING_1 ) + sizeof(EnvCaptureConstants::PADDING_1 ) - 1) / 16);

struct TerrainConstants
{
    NSMath::Float4x4 worldMatrix{};
    float maxHeight{};
    float worldTexelSpacingX{};
    float worldTexelSpacingZ{};
    float tessFactorScale{};
    float textureTilingFactor{};
    uint32_t PADDING_0[3]{};
    NSMath::Float2 chunkUVOffset{};
    NSMath::Float2 chunkUVScale{};
    uint32_t heightmapSrvIndex{};
    uint32_t terrainDiffuseSrvIndex{};
    uint32_t pageHalo{};
    uint32_t PADDING_1{};
    uint32_t splatSrvIndices[4]{};
    uint32_t impostorHeightmapSrvIndex{};
    uint32_t impostorDiffuseSrvIndex{};
    float morphNear{};
    float morphFar{};
};
static_assert(sizeof(TerrainConstants) % 16 == 0);
static_assert(offsetof(TerrainConstants, worldMatrix) % 16 == 0);
static_assert(offsetof(TerrainConstants, maxHeight                ) / 16 == (offsetof(TerrainConstants, maxHeight                ) + sizeof(TerrainConstants::maxHeight                ) - 1) / 16);
static_assert(offsetof(TerrainConstants, worldTexelSpacingX       ) / 16 == (offsetof(TerrainConstants, worldTexelSpacingX       ) + sizeof(TerrainConstants::worldTexelSpacingX       ) - 1) / 16);
static_assert(offsetof(TerrainConstants, worldTexelSpacingZ       ) / 16 == (offsetof(TerrainConstants, worldTexelSpacingZ       ) + sizeof(TerrainConstants::worldTexelSpacingZ       ) - 1) / 16);
static_assert(offsetof(TerrainConstants, tessFactorScale          ) / 16 == (offsetof(TerrainConstants, tessFactorScale          ) + sizeof(TerrainConstants::tessFactorScale          ) - 1) / 16);
static_assert(offsetof(TerrainConstants, textureTilingFactor      ) / 16 == (offsetof(TerrainConstants, textureTilingFactor      ) + sizeof(TerrainConstants::textureTilingFactor      ) - 1) / 16);
static_assert(offsetof(TerrainConstants, PADDING_0                ) / 16 == (offsetof(TerrainConstants, PADDING_0                ) + sizeof(TerrainConstants::PADDING_0                ) - 1) / 16);
static_assert(offsetof(TerrainConstants, chunkUVOffset            ) / 16 == (offsetof(TerrainConstants, chunkUVOffset            ) + sizeof(TerrainConstants::chunkUVOffset            ) - 1) / 16);
static_assert(offsetof(TerrainConstants, chunkUVScale             ) / 16 == (offsetof(TerrainConstants, chunkUVScale             ) + sizeof(TerrainConstants::chunkUVScale             ) - 1) / 16);
static_assert(offsetof(TerrainConstants, heightmapSrvIndex        ) / 16 == (offsetof(TerrainConstants, heightmapSrvIndex        ) + sizeof(TerrainConstants::heightmapSrvIndex        ) - 1) / 16);
static_assert(offsetof(TerrainConstants, terrainDiffuseSrvIndex   ) / 16 == (offsetof(TerrainConstants, terrainDiffuseSrvIndex   ) + sizeof(TerrainConstants::terrainDiffuseSrvIndex   ) - 1) / 16);
static_assert(offsetof(TerrainConstants, pageHalo                 ) / 16 == (offsetof(TerrainConstants, pageHalo                 ) + sizeof(TerrainConstants::pageHalo                 ) - 1) / 16);
static_assert(offsetof(TerrainConstants, PADDING_1                ) / 16 == (offsetof(TerrainConstants, PADDING_1                ) + sizeof(TerrainConstants::PADDING_1                ) - 1) / 16);
static_assert(offsetof(TerrainConstants, splatSrvIndices          ) / 16 == (offsetof(TerrainConstants, splatSrvIndices          ) + sizeof(TerrainConstants::splatSrvIndices          ) - 1) / 16);
static_assert(offsetof(TerrainConstants, impostorHeightmapSrvIndex) / 16 == (offsetof(TerrainConstants, impostorHeightmapSrvIndex) + sizeof(TerrainConstants::impostorHeightmapSrvIndex) - 1) / 16);
static_assert(offsetof(TerrainConstants, impostorDiffuseSrvIndex  ) / 16 == (offsetof(TerrainConstants, impostorDiffuseSrvIndex  ) + sizeof(TerrainConstants::impostorDiffuseSrvIndex  ) - 1) / 16);
static_assert(offsetof(TerrainConstants, morphNear                ) / 16 == (offsetof(TerrainConstants, morphNear                ) + sizeof(TerrainConstants::morphNear                ) - 1) / 16);
static_assert(offsetof(TerrainConstants, morphFar                 ) / 16 == (offsetof(TerrainConstants, morphFar                 ) + sizeof(TerrainConstants::morphFar                 ) - 1) / 16);

struct ImpostorConstants
{
    NSMath::Float4x4 worldMatrix{};
    float maxHeight{};
    float worldTexelSpacingX{};
    float worldTexelSpacingZ{};
    float cullMargin{};
    NSMath::Float2 streamMin{};
    NSMath::Float2 streamMax{};
    uint32_t heightmapSrvIndex{};
    uint32_t diffuseSrvIndex{};
    uint32_t streamValid{};
    float sinkStart{};
    float sinkRate{};
    float maxSink{};
    float PADDING_0[2];
};
static_assert(sizeof(ImpostorConstants) % 16 == 0);
static_assert(offsetof(ImpostorConstants, worldMatrix) % 16 == 0);
static_assert(offsetof(ImpostorConstants, maxHeight         ) / 16 == (offsetof(ImpostorConstants, maxHeight         ) + sizeof(ImpostorConstants::maxHeight         ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, worldTexelSpacingX) / 16 == (offsetof(ImpostorConstants, worldTexelSpacingX) + sizeof(ImpostorConstants::worldTexelSpacingX) - 1) / 16);
static_assert(offsetof(ImpostorConstants, worldTexelSpacingZ) / 16 == (offsetof(ImpostorConstants, worldTexelSpacingZ) + sizeof(ImpostorConstants::worldTexelSpacingZ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, cullMargin        ) / 16 == (offsetof(ImpostorConstants, cullMargin        ) + sizeof(ImpostorConstants::cullMargin        ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, streamMin         ) / 16 == (offsetof(ImpostorConstants, streamMin         ) + sizeof(ImpostorConstants::streamMin         ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, streamMax         ) / 16 == (offsetof(ImpostorConstants, streamMax         ) + sizeof(ImpostorConstants::streamMax         ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, heightmapSrvIndex ) / 16 == (offsetof(ImpostorConstants, heightmapSrvIndex ) + sizeof(ImpostorConstants::heightmapSrvIndex ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, diffuseSrvIndex   ) / 16 == (offsetof(ImpostorConstants, diffuseSrvIndex   ) + sizeof(ImpostorConstants::diffuseSrvIndex   ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, streamValid       ) / 16 == (offsetof(ImpostorConstants, streamValid       ) + sizeof(ImpostorConstants::streamValid       ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, sinkStart         ) / 16 == (offsetof(ImpostorConstants, sinkStart         ) + sizeof(ImpostorConstants::sinkStart         ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, sinkRate          ) / 16 == (offsetof(ImpostorConstants, sinkRate          ) + sizeof(ImpostorConstants::sinkRate          ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, maxSink           ) / 16 == (offsetof(ImpostorConstants, maxSink           ) + sizeof(ImpostorConstants::maxSink           ) - 1) / 16);
static_assert(offsetof(ImpostorConstants, PADDING_0         ) / 16 == (offsetof(ImpostorConstants, PADDING_0         ) + sizeof(ImpostorConstants::PADDING_0         ) - 1) / 16);

namespace NSBarrier
{
    class IBarrierBatch
    {
    public:
        virtual ~IBarrierBatch() = default;

        virtual void Add(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) = 0;
        virtual void Add(NSBarrier::BarrierKey key, std::vector<CD3DX12_RESOURCE_BARRIER>& barriers) = 0;

        virtual void Flush() = 0;
        virtual bool Remove(NSBarrier::BarrierKey key, CD3DX12_RESOURCE_BARRIER barrier) = 0;
        virtual void Clear(NSBarrier::BarrierKey key) = 0;

        virtual bool Execute(NSBarrier::BarrierKey key, NSDX12::GraphicsCommandList cmdList) = 0;
    };
}

class Blackboard;
namespace NSRenderer
{
    enum class ERendererFlag : uint32_t
    {
        UNDEFINED      = 0,
        MODE_WIREFRAME = 1 << 0,
        MAX            = 1 << 0,
        Force32Bit = UINT32_MAX,
    };

    struct BlackboardKey
    {
        const char* name;
        constexpr explicit BlackboardKey(const char* n) : name(n) {}
    };

    struct EnvironmentCubemap
    {
        ComPtr<ID3D12Resource2> cubemapTexture;
        ComPtr<ID3D12Resource2> cubemapDepth;
    };

    enum class ERegModelFlag : uint32_t
    {
        UNDEFINED = 0,
        UNSEEN_TO_ENV_CAPTURE = 1 << 0,
        MAX                   = 1 << 1,
        Force32Bit = UINT32_MAX
    };

    struct Model
    {
    public:
        ObserverKey m_sceneKey;
        EntityKey<NSRenderer::Model> m_id;
        EnvironmentCubemap m_envCubemap{};

        struct Neighbor
        {
            EntityID id;
            NSMath::Float3 position;
        };

        bool isDirty{};

        Flag<ERegModelFlag> m_flags;
    private:
    };

    inline constexpr BlackboardKey kRenderer_frameIndex{ "Renderer.frameIndex" };
    inline constexpr BlackboardKey kRenderer_width     { "Renderer.width" };
    inline constexpr BlackboardKey kRenderer_height    { "Renderer.height" };
    inline constexpr BlackboardKey kRenderer_models    { "Renderer.models" };
    inline constexpr BlackboardKey kRenderer_mainRTV   { "Renderer.mainRTV" };
    inline constexpr BlackboardKey kRenderer_mainDSV   { "Renderer.mainDSV" };
    inline constexpr BlackboardKey kRenderer_terrain   { "Renderer.terrain" };

    inline constexpr BlackboardKey kAtmosphere_transmitScatterSRV{ "Atmosphere." };
    inline constexpr BlackboardKey kAtmosphere_scatteringSRV     { "Atmosphere.scatteringSRV" };
    inline constexpr BlackboardKey kAtmosphere_constants         { "Atmosphere.constants" };

    inline constexpr BlackboardKey kEnvCubemap_brdfLUTsrv        { "EnvCubemap.brdfLUTsrv" };

    using FnRendererModelRegister_t = std::function<std::shared_ptr<NSRenderer::Model>(std::wstring_view name, ObserverKey key, NSDX12::GraphicsCommandList cmdList, ERegModelFlag flags)>;
    using FnRendererModelUnload_t = std::function<void(ObserverKey)>;

    using FnRendererFlagHasLeastOne_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagHasLeastAll_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagHasExact_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagEmpty_t = std::function<bool()>;
    using FnRendererFlagSet_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagUnSet_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagToggle_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;

    struct DepthStencilCreateDescription
    {
        DXGI_FORMAT format{};
        D3D12_DSV_FLAGS flags{};
        D3D12_DSV_DIMENSION dimension{};
        uint32_t width;
        uint32_t height;
        ComPtr<ID3D12Resource2>& outDSV;
    };

    struct RendererDescription
    {
        HWND wnd = nullptr;
        uint32_t width{};
        uint32_t height{};
        uint32_t streamingDistance{};
    };

    struct Ctx
    {
        Ctx(
            FnRendererModelRegister_t fn_registerModel,
            FnRendererModelUnload_t fn_unloadModel,
            MemberRef<NSBarrier::IBarrierBatch> in_barrierBatch,
            FnRendererFlagHasLeastOne_t fn_RendererFlagHasLeastOne,
            FnRendererFlagHasLeastAll_t fn_RendererFlagHasLeastAll,
            FnRendererFlagHasExact_t fn_RendererFlagHasExact,
            FnRendererFlagEmpty_t fn_RendererFlagEmpty,
            FnRendererFlagSet_t fn_RendererFlagSet,
            FnRendererFlagUnSet_t fn_RendererFlagUnSet,
            FnRendererFlagToggle_t fn_RendererFlagToggle,
            RendererDescription inRendererDesc
        )
            :
            registerModel(std::move(fn_registerModel)),
            unloadModel(std::move(fn_unloadModel)),
            barrierBatch(in_barrierBatch),
            RendererFlagHasLeastOne(fn_RendererFlagHasLeastOne),
            RendererFlagHasLeastAll(fn_RendererFlagHasLeastAll),
            RendererFlagHasExact(fn_RendererFlagHasExact),
            RendererFlagEmpty(fn_RendererFlagEmpty),
            RendererFlagSet(fn_RendererFlagSet),
            RendererFlagUnSet(fn_RendererFlagUnSet),
            RendererFlagToggle(fn_RendererFlagToggle),
            rendererDesc(inRendererDesc)
        {}

        FnRendererModelRegister_t registerModel;
        FnRendererModelUnload_t unloadModel;
        MemberRef<NSBarrier::IBarrierBatch> barrierBatch;
        FnRendererFlagHasLeastOne_t RendererFlagHasLeastOne;
        FnRendererFlagHasLeastAll_t RendererFlagHasLeastAll;
        FnRendererFlagHasExact_t RendererFlagHasExact;
        FnRendererFlagEmpty_t RendererFlagEmpty;
        FnRendererFlagSet_t RendererFlagSet;
        FnRendererFlagUnSet_t RendererFlagUnSet;
        FnRendererFlagToggle_t RendererFlagToggle;
        RendererDescription rendererDesc;
    };
}

namespace NSDebug
{
    struct Vertex
    {
        NSMath::Float3 position;
        NSMath::Float4 color;
    };

    struct Line
    {
        Vertex a;
    };
}

namespace NSScene
{
    class IScene;
}

namespace NSRenderPass
{
    enum class RenderPassID
    {
        PASSID_Z = 0u,
        PASSID_ATMOSSPHERE,
        PASSID_ENVIRONMENTCUBEMAP,
        PASSID_GEOMETRY,
        PASSID_TERRAIN,
        PASSID_DEBUG,
        PASSID_MAX,
    };

    struct DebugUtils
    {
        std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)> ImGuiSrvDescriptorAllocFn;
        std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)> ImGuiSrvDescriptorFreeFn;
        std::vector<NSDebug::Line> m_debugLines;
        D3D12_GPU_DESCRIPTOR_HANDLE debugPassImageSrv;
        uint32_t debugPassImageWidth{};
        uint32_t debugPassImageHeight{};
        bool isActive{};

        bool m_drawWorldGrid{ true };
        int m_gridMajorSpacing{ 1 };
        float m_gripHeight{ 550.f };
    };

    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void OnDestroy(NSRenderer::Ctx rendererCtx) = 0;

        virtual void Execute(std::shared_ptr<NSScene::IScene> scene, MemberRef<Blackboard> blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList) = 0;
        virtual void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) = 0;

        bool IsEnabled() const { return im_isEnabled; };
        void SetIsEnabled(bool val) { im_isEnabled = val; };

    private:
        bool im_isEnabled{};
    };

    template<RenderPassID Id>
    class RenderPass : public IRenderPass
    {
        public:
            static constexpr RenderPassID ID = Id;
    };

    template<typename T>
    concept IsRenderPass = std::derived_from<T, IRenderPass> && requires
    {
        { T::ID} -> std::convertible_to<RenderPassID>;
    };
}
