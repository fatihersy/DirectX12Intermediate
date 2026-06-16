
namespace NSDX12
{
    struct SHADER_RESOURCE_VIEW_DESC
    {
        D3D12_SRV_DIMENSION ViewDimension;
        UINT Shader4ComponentMapping;
        union
        {
            D3D12_BUFFER_SRV Buffer;
            D3D12_TEX1D_SRV Texture1D;
            D3D12_TEX1D_ARRAY_SRV Texture1DArray;
            D3D12_TEX2D_SRV Texture2D;
            D3D12_TEX2D_ARRAY_SRV Texture2DArray;
            D3D12_TEX2DMS_SRV Texture2DMS;
            D3D12_TEX2DMS_ARRAY_SRV Texture2DMSArray;
            D3D12_TEX3D_SRV Texture3D;
            D3D12_TEXCUBE_SRV TextureCube;
            D3D12_TEXCUBE_ARRAY_SRV TextureCubeArray;
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV RaytracingAccelerationStructure;
            D3D12_BUFFER_SRV_BYTE_OFFSET BufferByteOffset;
        };
    };

    class GraphicsCommandList
    {
    public:
        GraphicsCommandList() = default;
        explicit GraphicsCommandList(ID3D12GraphicsCommandList10* cmdList) : m_cmdList(cmdList) {}

        [[__deprecated__("Please do not use raw command list")]]
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

        // Additionals --------------------------------------------------------

        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            _In_range_(0,D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
            _In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource) UINT NumSubresources,
            UINT64 RequiredSize,
            _In_reads_(NumSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
            _In_reads_(NumSubresources) const UINT* pNumRows,
            _In_reads_(NumSubresources) const UINT64* pRowSizesInBytes,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
        }

        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            _In_range_(0,D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
            _In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource) UINT NumSubresources,
            UINT64 RequiredSize,
            _In_reads_(NumSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
            _In_reads_(NumSubresources) const UINT* pNumRows,
            _In_reads_(NumSubresources) const UINT64* pRowSizesInBytes,
            _In_ const void* pResourceData,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_INFO* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources, RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pResourceData, pSrcData);
        }

        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            UINT64 IntermediateOffset,
            _In_range_(0,D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
            _In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource) UINT NumSubresources,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, IntermediateOffset, FirstSubresource, NumSubresources, pSrcData);
        }

        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            UINT64 IntermediateOffset,
            _In_range_(0,D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
            _In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource) UINT NumSubresources,
            _In_ const void* pResourceData,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_INFO* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, IntermediateOffset, FirstSubresource, NumSubresources, pResourceData, pSrcData);
        }

        template <UINT MaxSubresources>
        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            UINT64 IntermediateOffset,
            _In_range_(0,MaxSubresources) UINT FirstSubresource,
            _In_range_(1,MaxSubresources-FirstSubresource) UINT NumSubresources,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, IntermediateOffset, FirstSubresource, NumSubresources, pSrcData);
        }

        template <UINT MaxSubresources>
        inline UINT64 UpdateSubresources(
            _In_ ID3D12Resource* pDestinationResource,
            _In_ ID3D12Resource* pIntermediate,
            UINT64 IntermediateOffset,
            _In_range_(0,MaxSubresources) UINT FirstSubresource,
            _In_range_(1,MaxSubresources-FirstSubresource) UINT NumSubresources,
            _In_ const void* pResourceData,
            _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_INFO* pSrcData) noexcept
        {
            ASSERT(m_cmdList);

            return ::UpdateSubresources(m_cmdList, pDestinationResource, pIntermediate, IntermediateOffset, FirstSubresource, NumSubresources, pResourceData, pSrcData);
        }
    private:
        ID3D12GraphicsCommandList10* m_cmdList = nullptr;
    };



}
