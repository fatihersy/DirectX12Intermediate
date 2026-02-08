#pragma once

#include "IApp.h"
#include "Model.h"
#include "StepTimer.h"

#include "directxtk12/Keyboard.h"
#include "directxtk12/Mouse.h"

class app : public IApp
{
public:
    app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow);
    ~app();

    void Run();

    void OnInit() override;
    void OnDestroy() override;
    void OnUpdate() override;
    void OnRender() override;

private:
    ComPtr<IWICImagingFactory2> m_wicFactory;
    static const UINT FrameCount = 2;

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain4> m_swapchain;
    ComPtr<ID3D12Device14> m_device;
    ComPtr<ID3D12Resource2> m_renderTarget[FrameCount];
    ComPtr<ID3D12Resource2> m_depthStencil;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12PipelineState> m_pipeline;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    ComPtr<ID3D12Resource2> m_fallbackTexture;
    ComPtr<ID3D12Resource2> m_fallbackTextureUpload;

    Model m_model;

    ComPtr<ID3D12Resource2> m_perFrameConstants;
    UINT m_rtvDescriptorSize;
    UINT m_srvDescriptorSize;
    D3D12_GPU_VIRTUAL_ADDRESS m_constantDataGpuVirtualAddr;
    PaddedConstantBuffer* m_constantDataCpuAddr;

    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence1> m_fence;
    UINT64 m_fenceGeneration;

    UINT m_maxObjects;

    void PopulateCommandList();
    void WaitForGPU();
    void MoveToNextFrame();
    void LoadPipeline();
    void LoadAssets();
    void UpdateKeyBindings();
    void UpdateMouseBindings();
    void UpdateCamera();

    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projectionMatrix;

    DirectX::XMVECTOR m_camEye;
    DirectX::XMVECTOR m_camFwd;
    DirectX::XMVECTOR m_camUp;
    FLOAT m_camYaw;
    FLOAT m_camPitch;
    FLOAT m_camSpeed;
    FLOAT m_lookSensitivity;
    DirectX::XMVECTOR m_lightDir;
    DirectX::XMVECTOR m_lightColor;

    float m_aspectRatio;
    std::wstring m_assetsPath;
    std::wstring m_executablePath;

    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    StepTimer m_timer;

    inline std::wstring GetAssetFullPath(const LPCWSTR assetName) const {
        return m_assetsPath + assetName;
    }
};

