#pragma once

#include <IApp.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

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

    void OnKeyDown(UINT8 key) override;
    void OnKeyUp(UINT8 key) override;
private:
    static const UINT FrameCount = 2;

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
    };

    struct ConstantBuffer
    {
        DirectX::XMFLOAT4X4 worldMatrix;
        DirectX::XMFLOAT4X4 viewMatrix;
        DirectX::XMFLOAT4X4 projectionMatrix;
        DirectX::XMFLOAT4 lightDir;
        DirectX::XMFLOAT4 lightColor;
    };
    static_assert(sizeof(ConstantBuffer) == 224);

    union PaddedConstantBuffer
    {
        ConstantBuffer constant;
        uint8_t bytes[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };
    static_assert(sizeof(PaddedConstantBuffer) == D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT * 1);

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
    ComPtr<ID3D12PipelineState> m_pipeline;
    ComPtr<ID3D12GraphicsCommandList10> m_commandList;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource2> m_vertexBufferGPU;
    //D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    //ComPtr<ID3D12Resource2> m_indexBufferGPU;

    ComPtr<ID3D12Resource2> m_perFrameConstants;
    UINT m_rtvDescriptorSize;
    D3D12_GPU_VIRTUAL_ADDRESS m_constantDataGpuVirtualAddr;
    PaddedConstantBuffer* m_constantDataCpuAddr;

    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence1> m_fence;
    UINT64 m_fenceGeneration;

    UINT c_maxObjects = 1;

    void PopulateCommandList();
    void WaitForGPU();
    void MoveToNextFrame();
    void LoadPipeline();
    void LoadAssets();

    float m_angle;
    DirectX::XMMATRIX m_worldMatrix;
    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projectionMatrix;
    DirectX::XMVECTOR m_lightDir;
    DirectX::XMVECTOR m_lightColor;

    float m_aspectRatio;
    std::wstring m_assetsPath;

    inline std::wstring GetAssetFullPath(LPCWSTR assetName) {
        return m_assetsPath + assetName;
    }
};

