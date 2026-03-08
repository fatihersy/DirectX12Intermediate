#pragma once

#include "IApp.h"

#include "Descriptor.h"
#include "Allocator.h"
#include "Pipeline.h"
#include "Scene.h"

class Renderer
{
public:
    Renderer(){};
    Renderer(IDXGIFactory7* factory, ID3D12Device14* device,HWND hwnd, UINT width, UINT height);
    ~Renderer();

    void Render();
    void Resize(UINT width, UINT height);
    void WaitForGPU();

    void onDestroy();

    inline CBAllocator& GetCBAllocator() { return this->m_allocator; }
    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            this->m_rtvHeap.GetCpuStart(),
            this->m_swapchain->GetCurrentBackBufferIndex(),
            this->m_rtvHeap.GetDescriptorSize()
        );
    }
    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrentDSV() const {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(this->m_dsvHeap.GetCpuStart());
    }
    inline ID3D12CommandQueue* GetCmdQueue() const { return this->m_commandQueue.Get(); };
    inline ID3D12GraphicsCommandList10* GetCmdList() const { return this->m_commandList.Get(); };

    inline Descriptor::Handle AllocSRVRing(uint32_t amount = 1u) {
        return m_srvHeap.AllocateRing(amount);
    };
    inline Descriptor::Handle AllocSRVStatic(uint32_t amount = 1u) {
        return m_srvHeap.AllocateStatic(amount);
    };
    inline void FreeSRVStatic(Descriptor::Handle handle) {
        m_srvHeap.FreeStatic(handle);
    };
    inline Descriptor::hOffset OffsetSRV(const Descriptor::Handle& handle, uint32_t offset) const {
        return m_srvHeap.Offset(handle, offset);
    }
    inline const Descriptor::Handle GetFallbackSRV() const {
        Descriptor::Handle handle{};
        m_srvHeap.At(FALLBACK_TEXTURE_SRV_INDEX, handle);
        return handle;
    };

private:
    ID3D12Device14* m_device = nullptr;
    IDXGIFactory7* m_factory = nullptr;

    ComPtr<IDXGISwapChain4> m_swapchain;
    ComPtr<ID3D12Resource2> m_renderTarget[IApp::c_frameCount];
    ComPtr<ID3D12Resource2> m_depthStencil;

    ComPtr<ID3D12CommandAllocator> m_commandAllocators[IApp::c_frameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    ComPtr<ID3D12Fence1> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceGeneration{};

    CBAllocator m_allocator;

    Descriptor::StaticHeap m_rtvHeap;
    Descriptor::StaticHeap m_dsvHeap;
    Descriptor::RingHeap m_srvHeap;

    FTexture m_fallbackTexture{};
    Descriptor::Handle m_fallbackTextureSRVHandle;

    CD3DX12_VIEWPORT m_viewport{};
    CD3DX12_RECT m_scissorRect{};

    void CreateSwapChain(IDXGIFactory7* factory, HWND hwnd, UINT width, UINT height);
    void CreateDepthStencil(LPCWSTR name, RendererTypes::DepthStencilCreateDescription desc);
    void CreateDefaultTexture();

    void PopulateCommandList(Scene& scene);
    void MoveToNextFrame();

    static constexpr uint32_t FALLBACK_TEXTURE_SRV_INDEX = 0;
};

