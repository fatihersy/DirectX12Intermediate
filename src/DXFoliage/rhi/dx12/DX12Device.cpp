#include "stdafx.h"
#include "DX12Device.h"

#include "DXSampleHelper.h"
#include "Logger.h"

#include "DX12Buffer.h"
#include "DX12DescriptorHeap.h"
#include "DX12Pipeline.h"
#include "DX12PipelineLayout.h"
#include "DX12Texture.h"

namespace NSRHIDX12
{
    DX12Device::DX12Device()
    {
        UINT dxgiFactoryFlags{};

        ComPtr<ID3D12Debug6> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

        // Seeking compatible device
        {
            ComPtr<IDXGIAdapter1> adapter;

            for (
                UINT adapterIndex{};
                SUCCEEDED(m_factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
                adapterIndex++
            ) {
                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }

            if (adapter.Get() == nullptr)
            {
                for (UINT adapterIndex{}; SUCCEEDED(m_factory->EnumAdapters1(adapterIndex, &adapter)); adapterIndex++)
                {
                    DXGI_ADAPTER_DESC1 desc{};
                    adapter->GetDesc1(&desc);

                    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

                    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
                    {
                        break;
                    }
                }
            }

            ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));
        }

        D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{};
        shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_7;

        ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

        ASSERT(static_cast<int>(shaderModel.HighestShaderModel) <= static_cast<int>(D3D_SHADER_MODEL_6_7), "Device doesn't support shader model 6.7");

        ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&m_infoQueue1)));

        m_infoQueue1->RegisterMessageCallback(
            [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void*)
            {
                ELogLevel level = ELogLevel::EDEBUG;
                switch (severity)
                {
                case D3D12_MESSAGE_SEVERITY_CORRUPTION: level = ELogLevel::EFATAL; break;
                case D3D12_MESSAGE_SEVERITY_ERROR:      level = ELogLevel::EERROR; break;
                case D3D12_MESSAGE_SEVERITY_WARNING:    level = ELogLevel::EWARN;  break;
                case D3D12_MESSAGE_SEVERITY_INFO:       level = ELogLevel::EINFO;  break;
                case D3D12_MESSAGE_SEVERITY_MESSAGE:    level = ELogLevel::EDEBUG; break;
                }
                if (g_PlatformConsoleWrite) {
                    g_PlatformConsoleWrite(level, description);
                }
            },
            D3D12_MESSAGE_CALLBACK_FLAG_NONE,
            nullptr,
            &m_infoQueueCookie
        );

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
        m_commandQueue->SetName(L"DX12Device::m_commandQueue");
    }

    std::unique_ptr<NSRHI::IBuffer> DX12Device::CreateBuffer(const NSRHI::BufferDesc& desc)
    {
        return std::make_unique<DX12Buffer>(m_device.Get(), desc);
    }

    std::unique_ptr<NSRHI::ITexture> DX12Device::CreateTexture(const NSRHI::TextureDesc&)
    {
        // NOT "reserved for later" any more — that comment was true when
        // written and is now false. The depth buffer it calls dead is very
        // much alive, and the front-end creates its render target, depth
        // buffer, checkerboard AND ImGui's font atlas through here. This
        // assert fires during Renderer::Initialize, so DX12 never reaches
        // a frame at all.
        //
        // DX12Texture still has only two constructors — wrap a swapchain
        // backbuffer, and create a depth-stencil. What is missing is a
        // third for the general case: a committed resource with
        // ResourceFlagsFor(desc.usage) (already written, and it already
        // handles RenderTarget/UnorderedAccess/DENY_SHADER_RESOURCE), no
        // RTV/DSV heap of its own, and COPY_DEST as the initial state so
        // the regional CopyBufferToTexture path can fill it.
        ASSERT(false, "CreateTexture: DX12 general texture constructor not written yet — see comment");
        return nullptr;
    }

    std::unique_ptr<NSRHI::IPipelineLayout> DX12Device::CreatePipelineLayout(const NSRHI::PipelineLayoutDesc& desc)
    {
        return std::make_unique<DX12PipelineLayout>(m_device.Get(), desc);
    }

    std::unique_ptr<NSRHI::IPipeline> DX12Device::CreateGraphicsPipeline(const NSRHI::GraphicsPipelineDesc& desc)
    {
        return std::make_unique<DX12Pipeline>(m_device.Get(), desc);
    }

    std::unique_ptr<NSRHI::IDescriptorHeap> DX12Device::CreateDescriptorHeap(const NSRHI::DescriptorHeapDesc& desc)
    {
        return std::make_unique<DX12DescriptorHeap>(m_device.Get(), desc);
    }

    void DX12Device::CreateShaderResourceView(NSRHI::IDescriptorHeap& heap,
                                              NSRHI::DescriptorOffset where,
                                              NSRHI::ITexture* texture)
    {
        // The neutral replacement for IDescriptor::Validate's pointer
        // comparison — catches an offset built by a different heap.
        ASSERT(heap.Validate(where), "Descriptor offset does not belong to this heap");

        auto* dxTexture = static_cast<DX12Texture*>(texture);
        if (not dxTexture) return;

        auto& dxHeap = static_cast<DX12DescriptorHeap&>(heap);

        // A null desc means "the resource's own format and full mip
        // chain", which is what a whole-texture SRV is. Mip ranges, array
        // slices and cube views need a real D3D12_SHADER_RESOURCE_VIEW_DESC
        // built from a neutral TextureViewDesc — see IDevice.h for why that
        // parameter is not here yet.
        m_device->CreateShaderResourceView(dxTexture->Raw(), nullptr, dxHeap.CpuHandle(where.index));
    }
}
