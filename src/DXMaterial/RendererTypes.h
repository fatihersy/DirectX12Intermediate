#pragma once

struct frameConstants
{
    DirectX::XMFLOAT4X4 viewMatrix{};
    DirectX::XMFLOAT4X4 projectionMatrix{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 camPos{};
    UINT PADDING_1{};
};
static_assert(sizeof(frameConstants) % 16 == 0);
static_assert(offsetof(frameConstants, PADDING_1) % 4 == 0);

struct meshConstants
{
    DirectX::XMFLOAT4X4 worldMatrix{};
    DirectX::XMFLOAT3X4 normalMatrix{};
    DirectX::XMFLOAT4 baseColor{};
    FLOAT metallic{};
    FLOAT roughness{};
    FLOAT opacity{};
    UINT textureFlags{};
};
static_assert(sizeof(meshConstants) % 16 == 0);
static_assert(offsetof(meshConstants, textureFlags) % 4 == 0);

struct skyDomeConstants
{
    DirectX::XMFLOAT3 BetaR{};
    FLOAT Pad0{};
    FLOAT BetaMScatter{};
    FLOAT BetaMExtinct{};
    FLOAT MieG{};
    FLOAT HR{};
    FLOAT HM{};
    FLOAT Rg{};
    FLOAT Rt{};
    FLOAT SunIntensity{};
    DirectX::XMFLOAT3 SunDir{};
    FLOAT Pad1{};
};
static_assert(sizeof(skyDomeConstants) % 16 == 0);
static_assert(offsetof(skyDomeConstants, BetaR) % 4 == 0);
static_assert(offsetof(skyDomeConstants, BetaMScatter) % 4 == 0);
static_assert(offsetof(skyDomeConstants, BetaMExtinct) % 4 == 0);
static_assert(offsetof(skyDomeConstants, MieG) % 4 == 0);
static_assert(offsetof(skyDomeConstants, HR) % 4 == 0);
static_assert(offsetof(skyDomeConstants, HM) % 4 == 0);
static_assert(offsetof(skyDomeConstants, Rg) % 4 == 0);
static_assert(offsetof(skyDomeConstants, Rt) % 4 == 0);
static_assert(offsetof(skyDomeConstants, SunIntensity) % 4 == 0);
static_assert(offsetof(skyDomeConstants, SunDir) % 4 == 0);

inline bool operator!=(const skyDomeConstants& lhs, const skyDomeConstants& rhs) noexcept
{
    if (not Float3Equals(lhs.BetaR, rhs.BetaR)) return true;
    if (lhs.BetaMScatter != rhs.BetaMScatter) return true;
    if (lhs.BetaMExtinct != rhs.BetaMExtinct) return true;
    if (lhs.MieG != rhs.MieG) return true;
    if (lhs.HR != rhs.HR) return true;
    if (lhs.HM != rhs.HM) return true;
    if (lhs.Rg != rhs.Rg) return true;
    if (lhs.Rt != rhs.Rt) return true;
    if (lhs.SunIntensity != rhs.SunIntensity) return true;
    if (not Float3Equals(lhs.SunDir, rhs.SunDir)) return true;

    return false;
}

struct envCaptureConstants
{
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 proj{};
    DirectX::XMFLOAT4 lightDir{};
    DirectX::XMFLOAT4 lightColor{};
    DirectX::XMFLOAT3 capturePos{};
    float Pad0{};
    DirectX::XMFLOAT3 camPos{};
    float Pad1{};
};
static_assert(sizeof(envCaptureConstants) % 16 == 0);
static_assert(offsetof(envCaptureConstants, view) % 4 == 0);
static_assert(offsetof(envCaptureConstants, proj) % 4 == 0);
static_assert(offsetof(envCaptureConstants, lightDir) % 4 == 0);
static_assert(offsetof(envCaptureConstants, lightColor) % 4 == 0);
static_assert(offsetof(envCaptureConstants, capturePos) % 4 == 0);
static_assert(offsetof(envCaptureConstants, camPos) % 4 == 0);

namespace Descriptor
{
    struct Handle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
        uint32_t amount{};
    };
    struct hOffset {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuAddr{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuAddr{};
        uint32_t index = UINT32_MAX;
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

        Descriptor::Handle rtvHandle;
        Descriptor::Handle dsvHandle;
        Descriptor::Handle srvHandle;
        Descriptor::Handle uavHandle;

        bool isOnGPU{};
        bool isDirty{};
        uint32_t generation{};

        static constexpr UINT PER_FACE_RESOLUTION = 128u;
        static constexpr UINT NUM_FACES = 6u;
        static constexpr UINT MIP_COUNT = 8u;
    };

    struct Model
    {
        SceneModelKey sceneKey{};
        EnvironmentCubemap m_envCubemap{};

        struct Neighbor {
            SceneModelKey sceneKey;
            DirectX::XMFLOAT3 position;
        };
        std::vector<Neighbor> objsInFrustum;

        bool isDirty{};
    };

    inline constexpr BlackboardKey kRenderer_frameIndex { "Renderer.FrameIndex" };
    inline constexpr BlackboardKey kRenderer_width      { "Renderer.Width" };
    inline constexpr BlackboardKey kRenderer_height     { "Renderer.Height" };
    inline constexpr BlackboardKey kRenderer_models     { "Renderer.Models" };

    inline constexpr BlackboardKey kRenderer_mainRTV            { "Renderer.MainRTV" };
    inline constexpr BlackboardKey kRenderer_mainDSV            { "Renderer.MainDSV" };

    inline constexpr BlackboardKey kSkyDome_transmitScatterSRVs{ "SkyDome.AtmosphereSRVs" };
    inline constexpr BlackboardKey kSkyDome_constants{ "SkyDome.AtmosphereConstants"};

    inline constexpr BlackboardKey kEnvCubemap_brdfLUTSRV { "EnvCubemap.BRDF_LUT_SRV" };

    class GraphicsCommandList
    {
    public:
        GraphicsCommandList() = default;
        explicit GraphicsCommandList(ID3D12GraphicsCommandList10* cmdList)
            : m_cmdList(cmdList) {}

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

    using FnDescAlloc_t = std::function<Descriptor::Handle(uint32_t amount)>;
    using FnDescFree_t = std::function<void(Descriptor::Handle& handle)>;
    using FnDescOffset_t = std::function<Descriptor::hOffset(const Descriptor::Handle& handle, uint32_t offset)>;
    using FnConstBuffAlloc_t = std::function<Allocator::AllocCtx(size_t size)>;
    using FnRendererModelRegister_t = std::function<RegisterModelKey(SceneModelKey key, NSRenderer::GraphicsCommandList cmdList)>;
    using FnRendererModelUnload_t = std::function<void(RegisterModelKey key)>;

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
    public:
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
            FnConstBuffAlloc_t fn_allocConstBuff,
            FnRendererModelRegister_t fn_registerModel,
            FnRendererModelUnload_t fn_unloadModel,
            Descriptor::Handle fallbackSRVoffset

        )
        : allocSRVRing(std::move(fn_allocSRVRing)),
          allocSRVStatic(std::move(fn_allocSRVStatic)),
          allocRTVStatic(std::move(fn_allocRTVStatic)),
          allocDSVStatic(std::move(fn_allocDSVStatic)),
          freeSRVStatic(std::move(fn_freeSRVStatic)),
          freeRTVStatic(std::move(fn_freeRTVStatic)),
          freeDSVStatic(std::move(fn_freeDSVStatic)),
          offsetSRV(std::move(fn_offsetSRV)),
          offsetRTV(std::move(fn_offsetRTV)),
          offsetDSV(std::move(fn_offsetDSV)),
          allocConstBuff(std::move(fn_allocConstBuff)),
          registerModel(std::move(fn_registerModel)),
          unloadModel(std::move(fn_unloadModel)),
          fallbackSRV(fallbackSRVoffset)
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
        FnConstBuffAlloc_t allocConstBuff;
        FnRendererModelRegister_t registerModel;
        FnRendererModelUnload_t unloadModel;
        Descriptor::Handle fallbackSRV;
    };
}

class Scene;
namespace RenderPass {
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void OnInit(Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) = 0;

        virtual void Execute(Scene& scene, Blackboard& blackboard, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) = 0;
        virtual void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) = 0;
        virtual void Destroy() {};

        bool IsEnabled() const { return im_isEnabled; }
        void SetEnabled(bool newVal) { im_isEnabled = newVal; }

    private:
        bool im_isEnabled{};
    };
}
