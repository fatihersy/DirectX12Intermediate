#include "stdafx.h"
#include "app.h"
#include "platform_win32.h"
#include "DXSampleHelper.h"

platform plat{};

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title),
    m_viewport{},
    m_scissorRect{},
    m_rtvDescriptorSize{},
    m_constantDataGpuVirtualAddr{},
    m_constantDataCpuAddr(nullptr),
    m_frameIndex{},
    m_fenceEvent(nullptr),
    m_fenceGeneration{},
    m_aspectRatio{}
{
    plat = platform(width, height, title, hInstance, nCmdShow, this);

    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;

    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    m_worldMatrix = DirectX::XMMatrixIdentity();



    m_viewMatrix = {};
    m_projectionMatrix = {};
    m_lightDir = {};
    m_lightColor = {};
}
app::~app() {}
void app::OnDestroy() {}

void app::OnInit()
{
    app::LoadPipeline();
    app::LoadAssets();
    plat.PlatShowWindow();
}
void app::Run()
{
    MSG msg = { 0 };

    while (msg.message != WM_QUIT)
    {
        plat.PlatMessageDispatch(msg);
    }
}
void app::OnUpdate()
{

}
void app::OnRender()
{

}
void app::WaitForGPU()
{
    UINT64 fenceGen = m_fenceGeneration;

}
void app::MoveToNextFrame()
{

}


void app::LoadPipeline() {}
void app::LoadAssets() {}
void app::PopulateCommandList() {}


void app::OnKeyDown(UINT8 key) {}
void app::OnKeyUp(UINT8 key) {}
