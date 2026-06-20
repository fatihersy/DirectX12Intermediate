#pragma once
#include "core/Defines.h"
#include "core/Math.h"
#include "core/EntityTypes.h"

#include "AllocatorTypes.h"
#include "DescriptorTypes.h"

namespace NSBarrier
{
    inline bool operator==(const D3D12_RESOURCE_BARRIER& lhs, const D3D12_RESOURCE_BARRIER& rhs) noexcept
    {
        if (
            lhs.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION
            and
            lhs.Flags == rhs.Flags
            and
            lhs.Transition.pResource == rhs.Transition.pResource
            and
            lhs.Transition.Subresource == rhs.Transition.Subresource
            and
            lhs.Transition.StateBefore == rhs.Transition.StateBefore
            and
            lhs.Transition.StateAfter == rhs.Transition.StateAfter
        )
        { return true; }

        if (
            lhs.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING
            and
            lhs.Flags == rhs.Flags
            and
            lhs.Aliasing.pResourceBefore == rhs.Aliasing.pResourceBefore
            and
            lhs.Aliasing.pResourceAfter == rhs.Aliasing.pResourceAfter
        )
        { return true; }

        if(
            lhs.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV
            and
            lhs.Flags == rhs.Flags
            and
            lhs.UAV.pResource == rhs.UAV.pResource
        )
        { return true; }

        return false;
    }

    struct BarrierKey
    {
        std::string_view name;
        constexpr explicit BarrierKey(std::string_view n) : name(n) {}
    };

    inline constexpr BarrierKey kApp_beginModelLoad{ "App.beginModelLoad" };
    inline constexpr BarrierKey kApp_endModelLoad  { "App.endModelLoad" };

    inline constexpr BarrierKey kTerrain_terrainOnInit{ "Terrain.terrainOnInit" };
}

struct FrameConstants
{
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 proj{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 eye{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(FrameConstants) % 16 == 0);
static_assert(offsetof(FrameConstants, view) % 4 == 0);
static_assert(offsetof(FrameConstants, proj) % 4 == 0);
static_assert(offsetof(FrameConstants, lightDir) % 4 == 0);
static_assert(offsetof(FrameConstants, lightColor) % 4 == 0);
static_assert(offsetof(FrameConstants, eye) % 4 == 0);
static_assert(offsetof(FrameConstants, PADDING_0) % 4 == 0);

struct MeshConstants
{
    DirectX::XMFLOAT4X4 worldMatrix{};
    DirectX::XMFLOAT3X4 normalMatrix{};
    DirectX::XMFLOAT4 baseColor{};
    float metallic{};
    float roughness{};
    float opacity{};
    uint32_t textureFlags{};
};
static_assert(sizeof(MeshConstants) % 16 == 0);
static_assert(offsetof(MeshConstants, worldMatrix) % 4 == 0);
static_assert(offsetof(MeshConstants, normalMatrix) % 4 == 0);
static_assert(offsetof(MeshConstants, baseColor) % 4 == 0);
static_assert(offsetof(MeshConstants, metallic) % 4 == 0);
static_assert(offsetof(MeshConstants, roughness) % 4 == 0);
static_assert(offsetof(MeshConstants, opacity) % 4 == 0);
static_assert(offsetof(MeshConstants, textureFlags) % 4 == 0);

struct FrameConstantsZPrepass
{
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 proj{};
    DirectX::XMFLOAT3 eye{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(FrameConstantsZPrepass) % 16 == 0);
static_assert(offsetof(FrameConstantsZPrepass, view) % 4 == 0);
static_assert(offsetof(FrameConstantsZPrepass, proj) % 4 == 0);
static_assert(offsetof(FrameConstantsZPrepass, PADDING_0) % 4 == 0);

struct MeshConstantsZPrepass
{
    DirectX::XMFLOAT4X4 worldMatrix{};
};
static_assert(sizeof(MeshConstantsZPrepass) % 16 == 0);
static_assert(offsetof(MeshConstantsZPrepass, worldMatrix) % 4 == 0);

struct AtmosphereConstants
{
    DirectX::XMFLOAT3 BetaR{};
    float PADDING_0{};
    float BetaMScatter{};
    float BetaMExtinct{};
    float MieG{};
    float HR{};
    float HM{};
    float Rg{};
    float Rt{};
    float SunIntensity{};
    DirectX::XMFLOAT3 SunDir{};
    float PADDING_1{};
};
static_assert(sizeof(AtmosphereConstants) % 16 == 0);
static_assert(offsetof(AtmosphereConstants, BetaR) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, PADDING_0) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, BetaMScatter) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, BetaMExtinct) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, MieG) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, HR) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, HM) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, Rg) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, Rt) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, SunIntensity) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, SunDir) % 4 == 0);
static_assert(offsetof(AtmosphereConstants, PADDING_1) % 4 == 0);

inline bool operator!=(const AtmosphereConstants& lhs, const AtmosphereConstants& rhs) noexcept
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
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 proj{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 capturePos{};
    float PADDING_0{};
    DirectX::XMFLOAT3 camPos{};
    float PADDING_1{};
};
static_assert(sizeof(EnvCaptureConstants) % 16 == 0);
static_assert(offsetof(EnvCaptureConstants, view) % 4 == 0);
static_assert(offsetof(EnvCaptureConstants, proj) % 4 == 0);
static_assert(offsetof(EnvCaptureConstants, lightDir) % 4 == 0);
static_assert(offsetof(EnvCaptureConstants, lightColor) % 4 == 0);
static_assert(offsetof(EnvCaptureConstants, capturePos) % 4 == 0);
static_assert(offsetof(EnvCaptureConstants, camPos) % 4 == 0);

struct TerrainConstants
{
    DirectX::XMFLOAT4X4 worldMatrix{};
    float maxHeight{};
    float worldTexelSpacingX{};
    float worldTexelSpacingZ{};
    float tessFactorScale{};
    float textureTilingFactor{};
    uint32_t PADDING_0[3]{};
    DirectX::XMFLOAT2 chunkUVOffset{};
    DirectX::XMFLOAT2 chunkUVScale{};
    uint32_t heightmapSrvIndex{};
    uint32_t terrainDiffuseSrvIndex{};
    uint32_t pageHalo{};
    uint32_t PADDING_1{};
    uint32_t splatSrvIndices[4]{};
};
static_assert(sizeof(TerrainConstants) % 16 == 0);
static_assert(offsetof(TerrainConstants, worldMatrix) % 4 == 0);
static_assert(offsetof(TerrainConstants, maxHeight) % 4 == 0);
static_assert(offsetof(TerrainConstants, worldTexelSpacingX) % 4 == 0);
static_assert(offsetof(TerrainConstants, worldTexelSpacingZ) % 4 == 0);
static_assert(offsetof(TerrainConstants, tessFactorScale) % 4 == 0);
static_assert(offsetof(TerrainConstants, textureTilingFactor) % 4 == 0);
static_assert(offsetof(TerrainConstants, PADDING_0) % 4 == 0);
static_assert(offsetof(TerrainConstants, chunkUVOffset) % 4 == 0);
static_assert(offsetof(TerrainConstants, chunkUVScale) % 4 == 0);
static_assert(offsetof(TerrainConstants, heightmapSrvIndex) % 4 == 0);
static_assert(offsetof(TerrainConstants, terrainDiffuseSrvIndex) % 4 == 0);
static_assert(offsetof(TerrainConstants, PADDING_1) % 4 == 0);
static_assert(offsetof(TerrainConstants, splatSrvIndices) % 4 == 0);

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
        MAX            = 1 << 1,
        Force32Bit = UINT32_MAX
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

        NSDescriptor::Handle rtvHandle;
        NSDescriptor::Handle dsvHandle;
        NSDescriptor::Handle srvHandle;
        NSDescriptor::Handle uavHandle;

        bool isOnGPU{};
        bool isDirty{};
        uint64_t generation{};

        static constexpr UINT PER_FACE_RESOLUTION = 128u;
        static constexpr UINT NUM_FACES = 6u;
        static constexpr UINT MIP_COUNT = 8u;
    };

    enum class ERegModelFlag : uint32_t {
        UNDEFINED             = 0,
        UNSEEN_TO_ENV_CAPTURE = 1 << 0,
        MAX                   = 1 << 1,
        Force32Bit = UINT32_MAX,
    };

    struct Model
    {
    public:
        ObserverKey m_sceneKey;
        EntityKey<NSRenderer::Model> m_id;
        EnvironmentCubemap m_envCubemap{};

        struct Neighbor {
            EntityID id;
            DirectX::XMFLOAT3 position;
        };
        std::vector<Neighbor> objsInFrustum;

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

    inline constexpr BlackboardKey kAtmosphere_transmitScatterSRV{ "Atmosphere.transmitScatterSRV" };
    inline constexpr BlackboardKey kAtmosphere_constants         { "Atmosphere.constants" };

    inline constexpr BlackboardKey kEnvCubemap_brdfLUTsrv{ "EnvCubemap.brdfLUTsrv" };

    using FnDescAlloc_t = std::function<NSDescriptor::Handle(uint32_t amount)>;
    using FnDescFree_t = std::function<void(NSDescriptor::Handle& handle)>;
    using FnDescOffset_t = std::function<NSDescriptor::Offset(const NSDescriptor::Handle& handle, uint32_t offset)>;
    using FnConstAlloc_t = std::function<NSAllocator::Ctx(size_t size)>;
    using FnRendererModelRegister_t = std::function<std::shared_ptr<NSRenderer::Model>(std::wstring_view modelName, ObserverKey key, NSDX12::GraphicsCommandList cmdList, ERegModelFlag flag)>;
    using FnRendererModelUnload_t = std::function<void(ObserverKey key)>;

    using FnRendererFlagHasLeastOne_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagHasLeastAll_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagHasExact_t = std::function<bool(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagEmpty_t = std::function<bool()>;
    using FnRendererFlagSet_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagUnSet_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;
    using FnRendererFlagToggle_t = std::function<void(Flag<NSRenderer::ERendererFlag> flag)>;

    struct DepthStencilCreateDescription {
        DXGI_FORMAT format{};
        D3D12_DSV_FLAGS flags{};
        D3D12_DSV_DIMENSION dimension{};
        UINT width;
        UINT height;
        ComPtr<ID3D12Resource2>& outDSV;
    };

    struct RendererDescription
    {
        HWND wnd = nullptr;
        UINT width{};
        UINT height{};
        uint32_t streamingDistance{};
    };

    struct Ctx
    {
        Ctx(
            FnDescAlloc_t fn_allocSRVRing,
            FnDescAlloc_t fn_allocSRVStatic,
            FnDescAlloc_t fn_allocRTVStatic,
            FnDescAlloc_t fn_allocDSVStatic,
            FnDescFree_t fn_freeSRVStatic,
            FnDescFree_t fn_freeRTVStatic,
            FnDescFree_t fn_freeDSVStatic,
            FnDescOffset_t fn_offsetSRV,
            FnDescOffset_t fn_offsetRTV,
            FnDescOffset_t fn_offsetDSV,
            FnConstAlloc_t fn_constAlloc,
            FnRendererModelRegister_t fn_registerModel,
            FnRendererModelUnload_t fn_unloadModel,
            NSDescriptor::Handle in_fallbackSRV,
            std::reference_wrapper<NSBarrier::IBarrierBatch> in_barrierBatch,
            FnRendererFlagHasLeastOne_t fn_RendererFlagHasLeastOne,
            FnRendererFlagHasLeastAll_t fn_RendererFlagHasLeastAll,
            FnRendererFlagHasExact_t fn_RendererFlagHasExact,
            FnRendererFlagEmpty_t fn_RendererFlagEmpty,
            FnRendererFlagSet_t fn_RendererFlagSet,
            FnRendererFlagUnSet_t fn_RendererFlagUnSet,
            FnRendererFlagToggle_t fn_RendererFlagToggle,
            RendererDescription inRendererDesc
        )
        :   allocSRVRing(std::move(fn_allocSRVRing)),
            allocSRVStatic(std::move(fn_allocSRVStatic)),
            allocRTVStatic(std::move(fn_allocRTVStatic)),
            allocDSVStatic(std::move(fn_allocDSVStatic)),
            freeSRVStatic(std::move(fn_freeSRVStatic)),
            freeRTVStatic(std::move(fn_freeRTVStatic)),
            freeDSVStatic(std::move(fn_freeDSVStatic)),
            offsetSRV(std::move(fn_offsetSRV)),
            offsetRTV(std::move(fn_offsetRTV)),
            offsetDSV(std::move(fn_offsetDSV)),
            constAlloc(std::move(fn_constAlloc)),
            registerModel(std::move(fn_registerModel)),
            unloadModel(std::move(fn_unloadModel)),
            fallbackSRV(in_fallbackSRV),
            barrierBatch(in_barrierBatch),
            rendererFlagHasLeastOne(fn_RendererFlagHasLeastOne),
            rendererFlagHasLeastAll(fn_RendererFlagHasLeastAll),
            rendererFlagHasExact(fn_RendererFlagHasExact),
            rendererFlagEmpty(fn_RendererFlagEmpty),
            rendererFlagSet(fn_RendererFlagSet),
            rendererFlagUnSet(fn_RendererFlagUnSet),
            rendererFlagToggle(fn_RendererFlagToggle),
            rendererDesc(inRendererDesc)
        {};

        FnDescAlloc_t allocSRVRing;
        FnDescAlloc_t allocSRVStatic;
        FnDescAlloc_t allocRTVStatic;
        FnDescAlloc_t allocDSVStatic;
        FnDescFree_t freeSRVStatic;
        FnDescFree_t freeRTVStatic;
        FnDescFree_t freeDSVStatic;
        FnDescOffset_t offsetSRV;
        FnDescOffset_t offsetRTV;
        FnDescOffset_t offsetDSV;
        FnConstAlloc_t constAlloc;
        FnRendererModelRegister_t registerModel;
        FnRendererModelUnload_t unloadModel;
        const NSDescriptor::Handle fallbackSRV;
        std::reference_wrapper<NSBarrier::IBarrierBatch> barrierBatch;
        FnRendererFlagHasLeastOne_t rendererFlagHasLeastOne;
        FnRendererFlagHasLeastAll_t rendererFlagHasLeastAll;
        FnRendererFlagHasExact_t rendererFlagHasExact;
        FnRendererFlagEmpty_t rendererFlagEmpty;
        FnRendererFlagSet_t rendererFlagSet;
        FnRendererFlagUnSet_t rendererFlagUnSet;
        FnRendererFlagToggle_t rendererFlagToggle;
        RendererDescription rendererDesc;
    };
}

namespace NSDebug
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct Line
    {
        Vertex a;
        Vertex b;
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
        PASSID_ATMOSPHERE,
        PASSID_ENVIRONMENTCUBEMAP,
        PASSID_GEOMETRY,
        PASSID_TERRAIN,
        PASSID_DEBUG,
        PASSID_MAX,
    };

    struct DebugUtils {
        std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)> ImGuiSrvDescriptorAllocFn;
        std::function<void(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle)> ImGuiSrvDescriptorFreeFn;
        std::vector<NSDebug::Line> m_debugLines;
        D3D12_GPU_DESCRIPTOR_HANDLE debugPassImageSrv;
        uint32_t debugPassImageWidth{};
        uint32_t debugPassImageHeight{};
        bool isActive{};

    };

    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void OnDestroy(NSRenderer::Ctx rendererCtx) = 0;

        virtual void Execute(NSScene::IScene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSDX12::GraphicsCommandList cmdList) = 0;
        virtual void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) = 0;

        bool IsEnabled() const { return im_isEnabled; };
        void SetIsEnabled(bool val) { im_isEnabled = val; };
    protected:
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
        { T::ID } -> std::convertible_to<RenderPassID>;
    };
}
