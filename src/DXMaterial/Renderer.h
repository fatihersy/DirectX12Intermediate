#pragma once

#include "IApp.h"

#include "Descriptor.h"
#include "Allocator.h"
#include "Pipeline.h"
#include "Material.h"

class Scene;

using FnRendererExecutionBody = std::function<void(NSRenderer::Ctx& ctx)>;

class Renderer
{
public:
    Renderer(){};
    ~Renderer();

    void Init(IDXGIFactory7* factory, ID3D12Device14* device, HWND hwnd, UINT width, UINT height);

    void Render();
    void Resize(UINT width, UINT height);
    void WaitForGPU();

    void onDestroy();

    void Execute(FnRendererExecutionBody Exec);

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

    inline ID3D12CommandQueue* ImGui_getCmdQueue() {
        return m_commandQueue.Get();
    }

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

    ConstBuffAlloc m_allocator;

    Descriptor::StaticHeap m_rtvHeap;
    Descriptor::StaticHeap m_dsvHeap;
    Descriptor::RingHeap m_srvHeap;

    FTexture m_fallbackTexture{};
    Descriptor::Handle m_fallbackTextureSRVHandle;

    CD3DX12_VIEWPORT m_viewport{};
    CD3DX12_RECT m_scissorRect{};

    void CreateSwapChain(IDXGIFactory7* factory, HWND hwnd, UINT width, UINT height);
    void CreateDepthStencil(LPCWSTR name, NSRenderer::DepthStencilCreateDescription desc);
    void CreateDefaultTexture();

    void PopulateCommandList(Scene& scene);
    void MoveToNextFrame();

    static constexpr uint32_t FALLBACK_TEXTURE_SRV_INDEX = 0;
};

