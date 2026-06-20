#include "stdafx.h"
#include "app.h"

#include "dxgidebug.h"

#include "DXSampleHelper.h"
#include "Platform.h"

#include "RenderPass.h"
#include "Model.h"

IApp* IApp::s_instance = nullptr;

Platform plat;

app::app(UINT width, UINT height, std::wstring title, HINSTANCE hInstance, int nCmdShow) : IApp(width, height, title)
{
    s_instance = this;

    im_windowedRECT = { 0L, 0L, static_cast<LONG>(width), static_cast<LONG>(height)};
    im_aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    plat = Platform(SWindow{
        .hInstance = hInstance,
        .hWnd = nullptr,
        .pApp = this,
        .nCmdShow = nCmdShow,
        .width = width,
        .height = height,
        .title = title
    });

    im_assetsPath = std::filesystem::current_path();

    WCHAR executablePath[512];
    GetExecutablePath(executablePath, _countof(executablePath));
    im_executablePath = executablePath;

    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ThrowIfFailed(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&im_wicFactory)));

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(plat.GetWindow());
}
app::~app()
{
    s_instance = nullptr;
}
void app::OnDestroy()
{
    m_scene.OnDestroy(m_renderer.GetCtx());
    m_renderer.OnDestroy();

    if(m_mouse.release()) {}
    m_mouse.reset();
    if(m_keyboard.release()) {}
    m_keyboard.reset();

    im_wicFactory.Reset();
    m_factory.Reset();
    im_device.Reset();

    ComPtr<IDXGIDebug1> dxgiDebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
    {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }
    dxgiDebug.Reset();
};

int app::Run()
{
    MSG msg{};

    while (msg.message != WM_QUIT)
    {
        plat.Dispatch(msg);
    }

    return static_cast<int>(msg.wParam);
}
void app::LoadPipeline()
{
    UINT dxgiFactoryFlags{};

    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();

        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    // Seeking compatible device
    {
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIFactory6> factory;
        if (SUCCEEDED(m_factory->QueryInterface(IID_PPV_ARGS(&factory))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
                adapterIndex++
            )
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

        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&im_device)));
    }

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{};
    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_7;

    ThrowIfFailed(im_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));

    if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6)
    {
        g_FError("app::LoadPipeline()::Minimum required shader model is 0x67. Found:%#02x", shaderModel.HighestShaderModel);
        throw std::runtime_error("");
    }

    {
        NSRenderer::RendererDescription desc {
            .wnd = plat.GetWindow(),
            .width = im_width,
            .height = im_height,
            .streamingDistance = 2000
        };
        m_renderer.Init(m_factory.Get(), im_device.Get(), im_wicFactory.Get(), desc);
    }

}
void app::LoadAssets()
{
    m_renderer.Execute([this](NSRenderer::Ctx ctx, NSDX12::GraphicsCommandList cmdList)
    {
        NSTerrain::TerrainDesc terrainDesc
        {
            .maxHeight = 8194.f,
            .pageCountX = 32,
            .pageCountZ = 32,
            .chunkCountX = 32,
            .chunkCountZ = 32,

            .heightmapDesc = {
                .relativePath = NSTerrain::kSourceFileHeightmap,
                .format = DXGI_FORMAT_R16_UNORM,
                .channels = {NSTexture::ChY}
            },
            .diffuseDesc = {
                .relativePath = NSTerrain::kSourceFileDiffuse,
                .format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                .channels = {
                    NSTexture::ChR,
                    NSTexture::ChG,
                    NSTexture::ChB,
                    NSTexture::ChA
                }
            }
        };


        ASSERT(this->m_renderer.CreateTerrain(cmdList, "terrain", terrainDesc));

        DirectX::XMFLOAT3 camEye = {
            0.f,
            600.f,
            0.f
        };

        m_scene.OnInit(ctx, im_device.Get(), im_wicFactory.Get(), this->m_renderer.GetTerrain(), 12.f);

        std::shared_ptr<NSScene::Camera> mainCam = m_scene.GetMainCamera();

        m_scene.SetupCameraInfiniteProjection(
            DE_REF(mainCam),
            DirectX::XM_PIDIV4,
            im_aspectRatio,
            m_scene.NEAR_CLIP
        );

        mainCam->SetCamera(camEye, { 0.f, 0.f, -1.f, 0.f }, { 0.f, 1.f, 0.f, 0.f });

        ObserverKey sDomeKey;
        {
            std::shared_ptr<Model> skyDome = m_scene.AddObject<NSModel::SDome>
            (
                NSModel::AddCtx { .name = L"SkyDome" },
                NSModel::SDome {
                    .radius = 10000.f,
                    .sliceCount = 64,
                    .stackCount = 32
                },
                ctx
            );

            skyDome->m_flags.Set(NSModel::EModelFlag::ATMOSPHERE);

            skyDome->UploadGPU(ctx, cmdList);
            skyDome->m_registerKey = ctx.registerModel(skyDome->m_name, skyDome->m_id, cmdList, NSRenderer::ERegModelFlag::UNSEEN_TO_ENV_CAPTURE)->m_id;
        }

        {
            const float stride = 11.f;
            const float gridStartPosX = 5.f * stride / -2.f;
            const float gridStartPosY = 5.f * stride / -2.f;

            int32_t idx{1};
            for (int32_t itr{}; itr < 36; itr++)
            {
                const int32_t col = itr % 6;
                const int32_t row = itr / 6;
                const float metallic = col * .2f;
                const float roughness = row * .2f;
                const float posX = -col * stride - gridStartPosX + camEye.x;
                const float posY = row * stride + gridStartPosY + camEye.y;
                const float posZ = 0.f + camEye.z - 100.f;

                NSModel::PrimitiveTraits<NSModel::SSphere> desc({
                    .radius = 5.f,
                    .sliceCount = 20,
                    .stackCount = 20
                });

                DirectX::XMFLOAT3 position({ posX, posY, posZ });
                std::shared_ptr<Model> model = m_scene.AddObject<NSModel::SSphere>
                (
                    NSModel::AddCtx {
                        .name = NSTool::wformat(L"Sphere%d", idx).c_str(),
                        .position = position,
                        .metallic = metallic,
                        .roughness = roughness
                    },
                    desc,
                    ctx
                );
                model->m_flags.Set(NSModel::EModelFlag::GENERATE_ENV_CUBEMAP);
                model->m_flags.Set(NSModel::EModelFlag::PBR_MODEL);
                model->m_flags.Set(NSModel::EModelFlag::STATIC);

                model->ICullable_Id = model->m_id;

                ASSERT(std::holds_alternative<NSMath::SBoundSphere>(model->ICullable_Bound), "Model has an unsupported bounding type");
                auto& modelBB = std::get<NSMath::SBoundSphere>(model->ICullable_Bound);

                modelBB.radius = desc.desc.radius;
                modelBB.sliceCount = desc.desc.sliceCount;
                modelBB.stackCount = desc.desc.stackCount;

                idx++;
            }
        }

        const DirectX::XMMATRIX envCamProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 1.f, m_scene.NEAR_CLIP, m_scene.FAR_CLIP);

        m_scene.m_models.ForEach([this, &ctx, &sDomeKey, &envCamProj](EntityID, std::shared_ptr<Model> model) -> LoopCondition
        {
            if(model->m_flags.HasLeastOne(NSModel::EModelFlag::ATMOSPHERE)) return ELoopConditionFlag::CONTINUE;

            if (model->m_flags.HasLeastAll(NSModel::EModelFlag::GENERATE_ENV_CUBEMAP))
            {
                model->envCameraKeys[0] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), { 1, 0, 0, 0 }, { 0, 1, 0, 0 }}))->m_id;
                model->envCameraKeys[1] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), {-1, 0, 0, 0 }, { 0, 1, 0, 0 }}))->m_id;
                model->envCameraKeys[2] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), { 0, 1, 0, 0 }, { 0, 0,-1, 0 }}))->m_id;
                model->envCameraKeys[3] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), { 0,-1, 0, 0 }, { 0, 0, 1, 0 }}))->m_id;
                model->envCameraKeys[4] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), { 0, 0, 1, 0 }, { 0, 1, 0, 0 }}))->m_id;
                model->envCameraKeys[5] = m_scene.m_cameras.Add(std::make_shared<NSScene::Camera>(NSScene::Camera{envCamProj, model->GetPosition(), { 0, 0,-1, 0 }, { 0, 1, 0, 0 }}))->m_id;
            }

            model->ForEach([model, &ctx](Mesh& mesh, UINT meshIndex) -> LoopCondition
            {
                // Pre upload
                {
                    ctx.barrierBatch.get().Add(NSBarrier::kApp_beginModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                        mesh.defaultVertexBuffer.Get(),
                        D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_COPY_DEST
                    ));
                    ctx.barrierBatch.get().Add(NSBarrier::kApp_beginModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                        mesh.defaultIndexBuffer.Get(),
                        D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_COPY_DEST
                    ));
                    for (NSTexture::Texture& tex : mesh.material.m_textures)
                    {
                        ctx.barrierBatch.get().Add(NSBarrier::kApp_beginModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                            tex.defaultBuffer.Get(),
                            D3D12_RESOURCE_STATE_COMMON,
                            D3D12_RESOURCE_STATE_COPY_DEST
                        ));
                    }
                }

                // Post upload
                {
                    ctx.barrierBatch.get().Add(NSBarrier::kApp_endModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                        mesh.defaultVertexBuffer.Get(),
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
                    ));
                    ctx.barrierBatch.get().Add(NSBarrier::kApp_endModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                        mesh.defaultIndexBuffer.Get(),
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_INDEX_BUFFER
                    ));
                    for (NSTexture::Texture& tex : mesh.material.m_textures)
                    {
                        ctx.barrierBatch.get().Add(NSBarrier::kApp_endModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                            tex.defaultBuffer.Get(),
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                        ));
                    }
                }

                return ELoopConditionFlag::CONTINUE;
            });

            return ELoopConditionFlag::CONTINUE;
        });

        ASSERT(ctx.barrierBatch.get().Execute(NSBarrier::kApp_beginModelLoad, cmdList));

        m_scene.m_models.ForEach([&](EntityID, std::shared_ptr<::Model> model) -> LoopCondition
        {
            model->UploadGPU(ctx, cmdList, false);
            std::shared_ptr<NSRenderer::Model> regModel = ctx.registerModel(model->m_name, model->m_id, cmdList, NSRenderer::ERegModelFlag::UNDEFINED);
            model->m_registerKey = regModel->m_id;

            ctx.barrierBatch.get().Add(NSBarrier::kApp_endModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                regModel->m_envCubemap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            ));

            return ELoopConditionFlag::CONTINUE;
        });

        ASSERT(ctx.barrierBatch.get().Execute(NSBarrier::kApp_endModelLoad, cmdList));
    });
}
void app::OnInit()
{
    LoadPipeline();
    LoadAssets();

    plat.ShowWindow();

    m_keyboardTracker.Reset();
};
void app::OnUpdate()
{
    m_timer.Tick(NULL);

    app::UpdateBindings();

    m_renderer.Update();

    m_scene.OnUpdate();
};
void app::OnRender()
{
    m_renderer.BeginFrame();

    m_renderer.DrawScene(m_scene);

    m_renderer.EndFrame();
};
void app::OnResize(UINT width, UINT height)
{
    if (width == 0 or height == 0 or (width == im_width and height == im_height)) return;

    im_width = width;
    im_height = height;
    im_aspectRatio = static_cast<FLOAT>(im_width) / static_cast<FLOAT>(im_height);

    m_renderer.Resize(im_width, im_height);

    m_scene.SetupCameraInfiniteProjection(DE_REF(m_scene.GetMainCamera()), DirectX::XM_PIDIV4, im_aspectRatio, m_scene.NEAR_CLIP);
};
void app::ToggleFullScreen()
{
    im_isFullScreen = not im_isFullScreen;

    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    GetMonitorInfo(MonitorFromWindow(plat.GetWindow(), MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
    const UINT monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    const UINT monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    const UINT monitorLeft = monitorInfo.rcMonitor.left;
    const UINT monitorTop = monitorInfo.rcMonitor.top;

    if (im_isFullScreen)
    {
        SetWindowLong(plat.GetWindow(), GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(plat.GetWindow(), HWND_TOP, monitorLeft, monitorTop, monitorWidth, monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        OnResize(monitorWidth, monitorHeight);
        return;
    }

    const UINT windowWidth = im_windowedRECT.right - im_windowedRECT.left;
    const UINT windowHeight = im_windowedRECT.bottom - im_windowedRECT.top;

    const UINT windowLeft = monitorLeft + static_cast<UINT>(monitorWidth / 2.f) - static_cast<UINT>(windowWidth / 2.f);
    const UINT windowTop = monitorTop + static_cast<UINT>(monitorHeight / 2.f) - static_cast<UINT>(windowHeight / 2.f);

    SetWindowLong(plat.GetWindow(), GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    SetWindowPos(plat.GetWindow(), HWND_TOP, monitorLeft, monitorTop, monitorWidth, monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    OnResize(monitorWidth, monitorHeight);
};

void app::UpdateBindings()
{
    auto kbState = m_keyboard->GetState();
    auto mouseState = m_mouse->GetState();

    m_keyboardTracker.Update(kbState);

    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::F1))
    {
        m_renderer.m_flag.Toggle(NSRenderer::ERendererFlag::MODE_WIREFRAME);
    }
    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::Escape))
    {
        m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
    }
    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::End))
    {
        PostMessage(plat.GetWindow(), WM_CLOSE, 0, 0);
    }
    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::Insert))
    {
        if (m_mouse->GetState().positionMode == DirectX::Mouse::MODE_RELATIVE)
        {
            m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
        }
        else  m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
    }

    // Camera Movement
    {
        std::shared_ptr<NSScene::Camera> mainCam = m_scene.GetMainCamera();

        DirectX::XMVECTOR move = DirectX::XMVectorZero();
        bool moved{};

        if (kbState.W) {
            move = DirectX::XMVectorAdd(move, mainCam->camFwd);
        }
        if (kbState.S) {
            move = DirectX::XMVectorSubtract(move, mainCam->camFwd);
        }
        if (kbState.A) {
            auto left = DirectX::XMVector3Cross(mainCam->camFwd, mainCam->camUp);
            left = DirectX::XMVector3Normalize(left);
            move = DirectX::XMVectorAdd(move, left);
        }
        if (kbState.D) {
            auto right = DirectX::XMVector3Cross(mainCam->camUp, mainCam->camFwd);
            right = DirectX::XMVector3Normalize(right);
            move = DirectX::XMVectorAdd(move, right);
        }
        if (kbState.Q) {
            move = DirectX::XMVectorAdd(move, mainCam->camUp);
        }
        if (kbState.E) {
            move = DirectX::XMVectorSubtract(move, mainCam->camUp);
        }

        if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(move), DirectX::g_XMEpsilon)) {
            move = DirectX::XMVector3Normalize(move);
            move = DirectX::XMVectorScale(move, mainCam->camSpeed * static_cast<FLOAT>(m_timer.GetElapsedSeconds()));
            mainCam->camEye = DirectX::XMVectorAdd(mainCam->camEye, move);

            moved = true;
        }

        if (mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE)
        {
            FLOAT dx = static_cast<FLOAT>(mouseState.x) * mainCam->lookSensitivity;
            FLOAT dy = static_cast<FLOAT>(mouseState.y) * mainCam->lookSensitivity;

            if (std::abs(dx) > DirectX::g_XMEpsilon.f[0])
            {
                mainCam->camYaw += dx;
                moved = true;
            }
            if (std::abs(dy) > DirectX::g_XMEpsilon.f[0])
            {
                mainCam->camPitch -= dy;
                mainCam->camPitch = std::clamp(mainCam->camPitch, -89.f, 89.f);
                moved = true;
            }

            m_mouse->ResetScrollWheelValue();
        }

        if (moved)
        {
            mainCam->generation++;
        }
    }

    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::NumPad1))
    {
        NSRenderPass::IRenderPass& zPrepass = m_renderer.GetPass(NSRenderPass::RenderPassID::PASSID_Z);
        zPrepass.SetIsEnabled(not zPrepass.IsEnabled());
    }
}
