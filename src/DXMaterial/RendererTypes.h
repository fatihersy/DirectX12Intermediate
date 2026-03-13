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
    FLOAT PadR{};
    FLOAT BetaMScatter{};
    FLOAT BetaMExtinct{};
    FLOAT MieG{};
    FLOAT Pad0{};
    FLOAT HR{};
    FLOAT HM{};
    FLOAT Rg{};
    FLOAT Rt{};
    FLOAT SunIntensity{};
    DirectX::XMFLOAT3 SunDir{};
};
static_assert(sizeof(skyDomeConstants) % 16 == 0);
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

namespace NSRenderer
{
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

        //void OMSetRenderTargets(
        //    UINT numRenderTargetDescriptors,
        //    const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
        //    BOOL RTsSingleHandleToDescriptorRange,
        //    const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) const
        //{
        //     m_cmdList->OMSetRenderTargets(numRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
        //}

        //void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, const FLOAT colorRGBA[4], UINT numRects, const D3D12_RECT* pRects) const {
        //    m_cmdList->ClearRenderTargetView(renderTargetView, colorRGBA, numRects, pRects);
        //}

        //void ClearDepthStencilView(
        //    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        //    D3D12_CLEAR_FLAGS clearFlags,
        //    FLOAT depth, UINT8 stencil,
        //    UINT numRects, const D3D12_RECT* pRects) const
        //{
        //    m_cmdList->ClearDepthStencilView(depthStencilView, clearFlags, depth, stencil, numRects, pRects);
        //}

        //void IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY primitiveTopology) const {
        //    m_cmdList->IASetPrimitiveTopology(primitiveTopology);
        //}

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

    using AllocSRVRing_t = std::function<Descriptor::Handle(uint32_t amount)>;
    using AllocSRVStatic_t = std::function<Descriptor::Handle(uint32_t amount)>;
    using FreeSRVStatic_t = std::function<void(Descriptor::Handle handle)>;
    using OffsetSRV_t = std::function<Descriptor::hOffset(const Descriptor::Handle& handle, uint32_t offset)>;
    using AllocConstBuff_t = std::function<Allocator::AllocCtx(size_t size)>;

    struct DepthStencilCreateDescription {
        DXGI_FORMAT format{};
        D3D12_DSV_FLAGS flags{};
        D3D12_DSV_DIMENSION dimention{};
        UINT width;
        UINT height;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        ComPtr<ID3D12Resource2>& outDSV;
    };

    struct Ctx
    {
    public:
        Ctx(
            Descriptor::Handle fallbackSRV,
            AllocSRVRing_t pfn_allocSRVRing,
            AllocSRVStatic_t pfn_allocSRVStatic,
            FreeSRVStatic_t pfn_freeSRVStatic,
            OffsetSRV_t pfn_offsetSRV,
            AllocConstBuff_t pfn_allocConstBuff
        )
        : fallbackSRV(fallbackSRV),
          allocSRVRing(std::move(pfn_allocSRVRing)),
          allocSRVStatic(std::move(pfn_allocSRVStatic)),
          freeSRVStatic(std::move(pfn_freeSRVStatic)),
          offsetSRV(std::move(pfn_offsetSRV)),
          allocConstBuff(std::move(pfn_allocConstBuff))
        {};

        const Descriptor::Handle fallbackSRV;

        AllocSRVRing_t allocSRVRing;
        AllocSRVStatic_t allocSRVStatic;
        FreeSRVStatic_t freeSRVStatic;
        OffsetSRV_t offsetSRV;
        AllocConstBuff_t allocConstBuff;
    };
}

class Scene;
namespace RenderPass {


    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void Execute(Scene& scene, NSRenderer::Ctx rendererCtx, NSRenderer::GraphicsCommandList cmdList) = 0;
        virtual void OnResize(uint32_t width, uint32_t height, NSRenderer::Ctx rendererCtx) = 0;
        virtual void Destroy() {};

        bool IsEnabled() const { return im_isEnabled; }
        void SetEnabled(bool newVal) { im_isEnabled = newVal; }

    private:
        bool im_isEnabled{};
    };
}
