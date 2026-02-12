#pragma once

class IApp
{
public:
    IApp(unsigned int width, unsigned int height, std::wstring title);
    virtual ~IApp();

    static IApp* GetInstance() {
        assert(s_instance != nullptr);
        return s_instance;
    };

    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual void OnInit() = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(UINT width, UINT height) = 0;
    virtual void ToggleFullScreen() = 0;

    inline ComPtr<ID3D12DescriptorHeap>& GetModelSrvHeap() { return im_modelSrvHeap; }
    inline ComPtr<ID3D12DescriptorHeap>& GetImguiSrvHeap() { return im_imGuiSrvHeap; }
    inline UINT GetModelSrvDescriptorSize() const { return im_modelSrvDescriptorSize; }

    void modelSrvAlloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle, int allocAmount = 1);
    void modelSrvFree(D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle);

    std::wstring m_title;
    UINT m_width;
    UINT m_height;
    RECT m_defaultWindowedRECT;
    bool m_isFullscreen;
    CD3DX12_CPU_DESCRIPTOR_HANDLE im_fallbackTextureCpuHandle{};

    protected:
        static IApp* s_instance;

        ComPtr<ID3D12DescriptorHeap> im_imGuiSrvHeap;
        std::vector<INT> im_freeImGuiSRVindices;
        UINT im_imGuiSrvDescriptorSize{};

        ComPtr<ID3D12DescriptorHeap> im_modelSrvHeap;
        std::vector<INT> im_freeModelSRVindices;
        UINT im_modelSrvDescriptorSize{};

        ComPtr<ID3D12DescriptorHeap> im_fallbackTexSrvHeap;
        UINT im_fallbackSrvDescriptorSize{};
};
