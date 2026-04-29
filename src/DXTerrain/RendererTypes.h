#pragma once

namespace NSDescriptor
{
    struct Handle
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
        uint32_t amount{};
    };

    struct Offset
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
    };
}

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
}

namespace NSTexture
{
    enum class EType : UINT {
        EType_NONE = 0,
        EType_DIFFUSE = 1,
        EType_SPECULAR = 2,
        EType_AMBIENT = 3,
        EType_EMISSIVE = 4,
        EType_HEIGHT = 5,
        EType_NORMALS = 6,
        EType_SHININESS = 7,
        EType_OPACITY = 8,
        EType_DISPLACEMENT = 9,
        EType_LIGHTMAP = 10,
        EType_REFLECTION = 11,
        EType_BASE_COLOR = 12,
        EType_NORMAL_CAMERA = 13,
        EType_EMISSION_COLOR = 14,
        EType_METALNESS = 15,
        EType_DIFFUSE_ROUGHNESS = 16,
        EType_AMBIENT_OCCLUSION = 17,
        EType_UNKNOWN = 18,
        EType_SHEEN = 19,
        EType_CLEARCOAT = 20,
        EType_TRANSMISSION = 21,
        EType_MAYA_BASE = 22,
        EType_MAYA_SPECULAR = 23,
        EType_MAYA_SPECULAR_COLOR = 24,
        EType_MAYA_SPECULAR_ROUGHNESS = 25,
        EType_ANISOTROPY = 26,
        EType_GLTF_METALLIC_ROUGHNESS = 27,
        EType_MAX = 28,
        EType_Force32Bit = UINT_MAX
    };

    struct Texture
    {
        EType textureType = EType::EType_NONE;
        ComPtr<ID3D12Resource2> defaultBuffer;
        ComPtr<ID3D12Resource2> uploadBuffer;
        NSDescriptor::Offset srvOffset;
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        UINT width{};
        UINT height{};
        UINT RowPitch{};
    };
}

struct frameConstants
{
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 proj{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 eye{};
    uint32_t PADDING_0{};
};
static_assert(sizeof(frameConstants) % 16 == 0);
static_assert(offsetof(frameConstants, view) % 4 == 0);
static_assert(offsetof(frameConstants, proj) % 4 == 0);
static_assert(offsetof(frameConstants, lightDir) % 4 == 0);
static_assert(offsetof(frameConstants, lightColor) % 4 == 0);
static_assert(offsetof(frameConstants, eye) % 4 == 0);
static_assert(offsetof(frameConstants, PADDING_0) % 4 == 0);

struct meshConstants
{
    DirectX::XMFLOAT4X4 worldMatrix{};
    DirectX::XMFLOAT3X4 normalMatrix{};
    DirectX::XMFLOAT4 baseColor{};
    float metallic{};
    float roughness{};
    float opacity{};
    uint32_t textureFlags{};
};
static_assert(sizeof(meshConstants) % 16 == 0);
static_assert(offsetof(meshConstants, worldMatrix) % 4 == 0);
static_assert(offsetof(meshConstants, normalMatrix) % 4 == 0);
static_assert(offsetof(meshConstants, baseColor) % 4 == 0);
static_assert(offsetof(meshConstants, metallic) % 4 == 0);
static_assert(offsetof(meshConstants, roughness) % 4 == 0);
static_assert(offsetof(meshConstants, opacity) % 4 == 0);
static_assert(offsetof(meshConstants, textureFlags) % 4 == 0);

struct atmosphereConstants
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
static_assert(sizeof(atmosphereConstants) % 16 == 0);
static_assert(offsetof(atmosphereConstants, BetaR) % 4 == 0);
static_assert(offsetof(atmosphereConstants, PADDING_0) % 4 == 0);
static_assert(offsetof(atmosphereConstants, BetaMScatter) % 4 == 0);
static_assert(offsetof(atmosphereConstants, BetaMExtinct) % 4 == 0);
static_assert(offsetof(atmosphereConstants, MieG) % 4 == 0);
static_assert(offsetof(atmosphereConstants, HR) % 4 == 0);
static_assert(offsetof(atmosphereConstants, HM) % 4 == 0);
static_assert(offsetof(atmosphereConstants, Rg) % 4 == 0);
static_assert(offsetof(atmosphereConstants, Rt) % 4 == 0);
static_assert(offsetof(atmosphereConstants, SunIntensity) % 4 == 0);
static_assert(offsetof(atmosphereConstants, SunDir) % 4 == 0);
static_assert(offsetof(atmosphereConstants, PADDING_1) % 4 == 0);

inline bool operator!=(const atmosphereConstants& lhs, const atmosphereConstants& rhs) noexcept
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

struct envCaptureConstants
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
static_assert(sizeof(envCaptureConstants) % 16 == 0);
static_assert(offsetof(envCaptureConstants, view) % 4 == 0);
static_assert(offsetof(envCaptureConstants, proj) % 4 == 0);
static_assert(offsetof(envCaptureConstants, lightDir) % 4 == 0);
static_assert(offsetof(envCaptureConstants, lightColor) % 4 == 0);
static_assert(offsetof(envCaptureConstants, capturePos) % 4 == 0);
static_assert(offsetof(envCaptureConstants, camPos) % 4 == 0);

namespace NSRenderer {
    class GraphicsCommandList
    {
    public:
        GraphicsCommandList() = default;
        explicit GraphicsCommandList(ID3D12GraphicsCommandList10* cmdList) : m_cmdList(cmdList) {}

        ID3D12GraphicsCommandList* Raw() const { return m_cmdList; }

        void ResourceBarrier(UINT numBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) const {
            m_cmdList->ResourceBarrier(numBarriers, pBarriers);
        }

        void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation) const {
            m_cmdList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
        }

        void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, INT baseVertexLocation, UINT startInstanceLocation) const {
            m_cmdList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
        }

        void Dispatch(UINT threadGroupCountX, UINT threadGroupCountY, UINT threadGroupCountZ) const {
            m_cmdList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
        }

        void SetPipelineState(ID3D12PipelineState* pPipelineState) const {
            m_cmdList->SetPipelineState(pPipelineState);
        }

        void SetDescriptorHeaps(UINT numDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps) const {
            m_cmdList->SetDescriptorHeaps(numDescriptorHeaps, ppDescriptorHeaps);
        }

        void SetGraphicsRootSignature(ID3D12RootSignature* pRootSignature) const {
            m_cmdList->SetGraphicsRootSignature(pRootSignature);
        }

        void SetComputeRootSignature(ID3D12RootSignature* pRootSignature) const {
            m_cmdList->SetComputeRootSignature(pRootSignature);
        }

        void SetGraphicsRoot32BitConstant(UINT rootParameterIndex, UINT srcData, UINT destOffsetIn32BitValues) const {
            m_cmdList->SetGraphicsRoot32BitConstant(rootParameterIndex, srcData, destOffsetIn32BitValues);
        }

        void SetGraphicsRoot32BitConstants(UINT rootParameterIndex, UINT num32BitValuesToSet, const void* pSrcData, UINT destOffsetIn32BitValues) const {
            m_cmdList->SetGraphicsRoot32BitConstants(rootParameterIndex, num32BitValuesToSet, pSrcData, destOffsetIn32BitValues);
        }

        void SetGraphicsRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetGraphicsRootConstantBufferView(rootParameterIndex, bufferLocation);
        }

        void SetGraphicsRootShaderResourceView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetGraphicsRootShaderResourceView(rootParameterIndex, bufferLocation);
        }

        void SetGraphicsRootUnorderedAccessView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetGraphicsRootUnorderedAccessView(rootParameterIndex, bufferLocation);
        }

        void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) const {
            m_cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, baseDescriptor);
        }

        void SetComputeRoot32BitConstant(UINT rootParameterIndex, UINT srcData, UINT destOffsetIn32BitValues) const {
            m_cmdList->SetComputeRoot32BitConstant(rootParameterIndex, srcData, destOffsetIn32BitValues);
        }

        void SetComputeRoot32BitConstants(UINT rootParameterIndex, UINT num32BitValuesToSet, const void* pSrcData, UINT destOffsetIn32BitValues) const {
            m_cmdList->SetComputeRoot32BitConstants(rootParameterIndex, num32BitValuesToSet, pSrcData, destOffsetIn32BitValues);
        }

        void SetComputeRootConstantBufferView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetComputeRootConstantBufferView(rootParameterIndex, bufferLocation);
        }

        void SetComputeRootShaderResourceView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetComputeRootShaderResourceView(rootParameterIndex, bufferLocation);
        }

        void SetComputeRootUnorderedAccessView(UINT rootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS bufferLocation) const {
            m_cmdList->SetComputeRootUnorderedAccessView(rootParameterIndex, bufferLocation);
        }

        void SetComputeRootDescriptorTable(UINT rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor) const {
            m_cmdList->SetComputeRootDescriptorTable(rootParameterIndex, baseDescriptor);
        }

        void IASetVertexBuffers(UINT startSlot, UINT numViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) const {
            m_cmdList->IASetVertexBuffers(startSlot, numViews, pViews);
        }

        void IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) const {
            m_cmdList->IASetIndexBuffer(pView);
        }

        void RSSetViewports(UINT numViewports, const D3D12_VIEWPORT* pViewports) const {
            m_cmdList->RSSetViewports(numViewports, pViewports);
        }

        void RSSetScissorRects(UINT numRects, const D3D12_RECT* pRects) const {
            m_cmdList->RSSetScissorRects(numRects, pRects);
        }

        void CopyResource(ID3D12Resource* pDstResource, ID3D12Resource* pSrcResource) const {
            m_cmdList->CopyResource(pDstResource, pSrcResource);
        }

        void CopyBufferRegion(ID3D12Resource* pDstBuffer, UINT64 dstOffset, ID3D12Resource* pSrcBuffer, UINT64 srcOffset, UINT64 numBytes) const {
            m_cmdList->CopyBufferRegion(pDstBuffer, dstOffset, pSrcBuffer, srcOffset, numBytes);
        }

        void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT dstX, UINT dstY, UINT dstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox) const {
            m_cmdList->CopyTextureRegion(pDst, dstX, dstY, dstZ, pSrc, pSrcBox);
        }

        void ExecuteIndirect(
            ID3D12CommandSignature* pCommandSignature, UINT maxCommandCount, ID3D12Resource* pArgumentBuffer, UINT64 argumentBufferOffset, ID3D12Resource* pCountBuffer, UINT64 countBufferOffset) const
        {
            m_cmdList->ExecuteIndirect(pCommandSignature, maxCommandCount, pArgumentBuffer, argumentBufferOffset, pCountBuffer, countBufferOffset);
        }

        void BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE type, UINT index) const {
            m_cmdList->BeginQuery(pQueryHeap, type, index);
        }

        void EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE type, UINT index) const {
            m_cmdList->EndQuery(pQueryHeap, type, index);
        }

        void ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE type, UINT startIndex, UINT numQueries, ID3D12Resource* pDestinationBuffer, UINT64 alignedDestinationBufferOffset) const {
            m_cmdList->ResolveQueryData(pQueryHeap, type, startIndex, numQueries, pDestinationBuffer, alignedDestinationBufferOffset);
        }

        void OMSetStencilRef(UINT stencilRef) const {
            m_cmdList->OMSetStencilRef(stencilRef);
        }

        void OMSetBlendFactor(const FLOAT blendFactor[4]) const {
            m_cmdList->OMSetBlendFactor(blendFactor);
        }

        void OMSetRenderTargets(
            UINT numRenderTargetDescriptors,
            const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
            BOOL RTsSingleHandleToDescriptorRange,
            const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
        {
            m_cmdList->OMSetRenderTargets(numRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
        }

        void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, const FLOAT colorRGBA[4], UINT numRects, const D3D12_RECT* pRects) const {
            m_cmdList->ClearRenderTargetView(renderTargetView, colorRGBA, numRects, pRects);
        }

        void ClearDepthStencilView(
            D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
            D3D12_CLEAR_FLAGS clearFlags,
            FLOAT depth, UINT8 stencil,
            UINT numRects, const D3D12_RECT* pRects) const
        {
            m_cmdList->ClearDepthStencilView(depthStencilView, clearFlags, depth, stencil, numRects, pRects);
        }

        void IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY primitiveTopology) const {
            m_cmdList->IASetPrimitiveTopology(primitiveTopology);
        }

        //HRESULT Close() const {
        //  return m_cmdList->Close();
        //}

        //HRESULT Reset(ID3D12CommandAllocator* pAllocator, ID3D12PipelineState* pInitialState) const {
        //    return m_cmdList->Reset(pAllocator, pInitialState);
        //}

        //void SetPredication(ID3D12Resource* pBuffer, UINT64 alignedBufferOffset, D3D12_PREDICATION_OP operation) const {
        //    m_cmdList->SetPredication(pBuffer, alignedBufferOffset, operation);
        //}

        //void SOSetTargets(UINT startSlot, UINT numViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) const {
        //    m_cmdList->SOSetTargets(startSlot, numViews, pViews);
        //}

    private:
        ID3D12GraphicsCommandList10* m_cmdList = nullptr;
    };
}

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

        virtual bool Execute(NSBarrier::BarrierKey key, NSRenderer::GraphicsCommandList cmdList) = 0;

    };
}

class Blackboard;
namespace NSRenderer
{
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
        uint32_t generation{};

        static constexpr UINT PER_FACE_RESOLUTION = 128u;
        static constexpr UINT NUM_FACES = 6u;
        static constexpr UINT MIP_COUNT = 8u;
    };

    struct Model
    {
    public:
        NSModel::SceneModelKey sceneKey;
        NSModel::RegisterModelKey registerKey;
        EnvironmentCubemap m_envCubemap{};

        struct Neighbor {
            NSModel::SceneModelKey sceneKey;
            DirectX::XMFLOAT3 position;
        };
        std::vector<Neighbor> objsInFrustum;

        bool TestFlag(NSModel::ERegModelFlag flag) const {
            return m_flags.test(static_cast<uint32_t>(flag));
        }
        void SetFlag(NSModel::ERegModelFlag flag) {
            m_flags.set(static_cast<uint32_t>(flag));
        }
        void ResetFlag(NSModel::ERegModelFlag flag) {
            m_flags.reset(static_cast<uint32_t>(flag));
        }
        void FlipFlag(NSModel::ERegModelFlag flag) {
            m_flags.flip(static_cast<uint32_t>(flag));
        }

        bool isDirty{};

    private:
        std::bitset<32> m_flags{};
    };

    inline constexpr BlackboardKey kRenderer_frameIndex{ "Renderer.frameIndex" };
    inline constexpr BlackboardKey kRenderer_width{ "Renderer.width" };
    inline constexpr BlackboardKey kRenderer_height{ "Renderer.height" };
    inline constexpr BlackboardKey kRenderer_models{ "Renderer.models" };
    inline constexpr BlackboardKey kRenderer_mainRTV{ "Renderer.mainRTV" };
    inline constexpr BlackboardKey kRenderer_mainDSV{ "Renderer.mainDSV" };

    inline constexpr BlackboardKey kAtmosphere_transmitScatterSRV{ "Atmosphere.transmitScatterSRV" };
    inline constexpr BlackboardKey kAtmosphere_constants{ "Atmosphere.constants" };

    inline constexpr BlackboardKey kEnvCubemap_brdfLUTsrv{ "EnvCubemap.brdfLUTsrv" };

    using FnDescAlloc_t = std::function<NSDescriptor::Handle(uint32_t amount)>;
    using FnDescFree_t = std::function<void(NSDescriptor::Handle& handle)>;
    using FnDescOffset_t = std::function<NSDescriptor::Offset(const NSDescriptor::Handle& handle, uint32_t offset)>;
    using FnConstAlloc_t = std::function<NSAllocator::Ctx(size_t size)>;
    using FnRendererModelRegister_t = std::function<NSRenderer::Model&(std::wstring_view modelName, NSModel::SceneModelKey key, NSRenderer::GraphicsCommandList cmdList, NSModel::ERegModelFlag flag)>;
    using FnRendererModelUnload_t = std::function<void(NSModel::RegisterModelKey key)>;

    struct DepthStencilCreateDescription {
        DXGI_FORMAT format{};
        D3D12_DSV_FLAGS flags{};
        D3D12_DSV_DIMENSION dimention{};
        UINT width;
        UINT height;
        ComPtr<ID3D12Resource2>& outDSV;
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
            std::reference_wrapper<NSBarrier::IBarrierBatch> in_barrierBatch
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
            barrierBatch(in_barrierBatch)
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
        NSDescriptor::Handle fallbackSRV;
        std::reference_wrapper<NSBarrier::IBarrierBatch> barrierBatch;
    };
}

class Scene;
namespace NSRenderPass
{
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) = 0;
        virtual void OnDestroy() = 0;

        virtual void Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) = 0;
        virtual void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) = 0;

        bool IsEnabled() const { return im_isEnabled; };
        void SetIsEnabled(bool val) { im_isEnabled = val; };

    private:
        bool im_isEnabled{};
    };
}
