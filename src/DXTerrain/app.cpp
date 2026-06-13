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

    m_renderer.Init(m_factory.Get(), im_device.Get(), im_wicFactory.Get(), plat.GetWindow(), im_width, im_height);
}
void app::LoadAssets()
{
    m_renderer.Execute([this](NSRenderer::Ctx ctx, NSRenderer::GraphicsCommandList cmdList)
    {
        m_scene = Scene(im_device.Get(), im_wicFactory.Get(), 12.f);
        m_scene.SetupCameraInfiniteProjection(
            DirectX::XM_PIDIV4,
            im_aspectRatio,
            m_scene.NEAR_CLIP
        );

        m_scene.m_terrain.desc = NSTerrain::TerrainDesc
        {
            .worldWidth = 16384.f,
            .worldDepth = 16384.f,
            .maxHeight = 8194.f,

            .pageCountX = 32,
            .pageCountZ = 32,

            .chunkCountX = 32,
            .chunkCountZ = 32,
            .vertsPerChunkEdge = 33,

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

        DirectX::XMFLOAT3 camEye{};

        if (this->m_renderer.CreateTerrain(cmdList, "terrain", m_scene.m_terrain.desc))
        {
            camEye = {
                0.f,
                m_scene.m_terrain.desc.maxHeight * .5f,
                0.f
            };
            m_scene.m_camera.SetCamera(camEye, { 0.f, 0.f, -1.f, 0.f }, { 0.f, 1.f, 0.f, 0.f });

            for (const NSTerrain::TerrainPage& page : this->m_renderer.GetTerrain().GetPages())
            {
                m_scene.m_terrain.pages.push_back({
                    .key = page.key,
                    .bound = page.bounds
                });

                for (const NSTerrain::TerrainChunk& chunk : page.chunks)
                {
                    m_scene.m_terrain.pages.back().chunks.push_back({
                        .key = chunk.key,
                        .bound = chunk.bounds
                    });
                }
            }

            m_scene.m_terrain.isInitialized = true;
        }

        NSModel::SceneModelKey sDomeKey{};
        {
            Model& skyDome = m_scene.AddObject<NSModel::SDome>
            (
                NSModel::AddCtx { .name = L"SkyDome" },
                NSModel::SDome {
                    .radius = 10000.f,
                    .sliceCount = 64,
                    .stackCount = 32
                },
                ctx
            );

            skyDome.m_sceneKey.id = this->im_nextId++;
            skyDome.m_sceneKey.index = 0u;
            skyDome.SetFlag(NSModel::EModelFlag::MODEL_FLAG_ATMOSPHERE);

            skyDome.UploadGPU(ctx, cmdList);
            skyDome.m_registerKey = ctx.registerModel(skyDome.m_name, skyDome.m_sceneKey, cmdList, NSModel::ERegModelFlag::MODEL_FLAG_UNSEEN_TO_ENV_CAPTURE).registerKey;
            sDomeKey = skyDome.m_sceneKey;
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

                Model& model = m_scene.AddObject<NSModel::SSphere>
                (
                    NSModel::AddCtx {
                        .name = NSTool::wformat(L"Sphere%d", idx).c_str(),
                        .position = { posX, posY, posZ },
                        .metallic = metallic,
                        .roughness = roughness
                    },
                    desc,
                    ctx
                );
                model.m_sceneKey.id = this->im_nextId++;
                model.m_sceneKey.index = idx;
                model.SetFlag(NSModel::EModelFlag::MODEL_FLAG_GENERATE_ENV_CUBEMAP);
                model.SetFlag(NSModel::EModelFlag::MODEL_FLAG_PBR_MODEL);

                model.m_collision.radius = desc.desc.radius;
                model.m_collision.sliceCount = desc.desc.sliceCount;
                model.m_collision.stackCount = desc.desc.stackCount;
                idx++;
            }
        }

        m_scene.ForEachModel([&ctx, &sDomeKey](Model& model)
        {
            if (model.m_sceneKey.id == sDomeKey.id) return;

            model.ForEach([model, &ctx](Mesh& mesh, UINT meshIndex)
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
            });
        });

        ASSERT(ctx.barrierBatch.get().Execute(NSBarrier::kApp_beginModelLoad, cmdList));

        for (size_t itr = 1u; itr < m_scene.m_models.size(); itr++)
        {
            Model& model = m_scene.m_models[itr];

            ASSERT(model.m_sceneKey.index == itr);

            model.UploadGPU(ctx, cmdList, false);
            NSRenderer::Model& regModel = ctx.registerModel(model.m_name, model.m_sceneKey, cmdList, NSModel::ERegModelFlag::MODEL_FLAG_NONE);
            model.m_registerKey = regModel.registerKey;

            ctx.barrierBatch.get().Add(NSBarrier::kApp_endModelLoad, CD3DX12_RESOURCE_BARRIER::Transition(
                regModel.m_envCubemap.cubemapTexture.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            ));
        }

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

    app::UpdateKeyBindings();
    app::UpdateMouseBindings();

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

    m_scene.SetupCameraInfiniteProjection(DirectX::XM_PIDIV4, im_aspectRatio, m_scene.NEAR_CLIP);
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

void app::UpdateKeyBindings()
{
    auto kbState = m_keyboard->GetState();
    m_keyboardTracker.Update(kbState);

    if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::F1))
    {
        m_renderer.FlipFlag(NSRenderer::ERendererFlag::MODE_WIREFRAME);
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
        DirectX::XMVECTOR move = DirectX::XMVectorZero();

        if (kbState.W) {
            move = DirectX::XMVectorAdd(move, m_scene.m_camera.camFwd);
        }
        if (kbState.S) {
            move = DirectX::XMVectorSubtract(move, m_scene.m_camera.camFwd);
        }
        if (kbState.A) {
            auto left = DirectX::XMVector3Cross(m_scene.m_camera.camFwd, m_scene.m_camera.camUp);
            left = DirectX::XMVector3Normalize(left);
            move = DirectX::XMVectorAdd(move, left);
        }
        if (kbState.D) {
            auto right = DirectX::XMVector3Cross(m_scene.m_camera.camUp, m_scene.m_camera.camFwd);
            right = DirectX::XMVector3Normalize(right);
            move = DirectX::XMVectorAdd(move, right);
        }
        if (kbState.Q) {
            move = DirectX::XMVectorAdd(move, m_scene.m_camera.camUp);
        }
        if (kbState.E) {
            move = DirectX::XMVectorSubtract(move, m_scene.m_camera.camUp);
        }

        if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(move), DirectX::g_XMEpsilon)) {
            move = DirectX::XMVector3Normalize(move);
            move = DirectX::XMVectorScale(move, m_scene.m_camera.camSpeed * static_cast<FLOAT>(m_timer.GetElapsedSeconds()));
            m_scene.m_camera.camEye = DirectX::XMVectorAdd(m_scene.m_camera.camEye, move);
        }

        if (m_keyboardTracker.IsKeyReleased(DirectX::Keyboard::NumPad1))
        {
            NSRenderPass::IRenderPass& zPrepass = m_renderer.GetPass(NSRenderPass::RenderPassID::PASSID_Z);
            zPrepass.SetIsEnabled(not zPrepass.IsEnabled());
        }
    }
}
void app::UpdateMouseBindings()
{
    auto mouseState = m_mouse->GetState();

    if (mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE) {
        FLOAT dx = static_cast<FLOAT>(mouseState.x) * m_scene.m_camera.lookSensitivity;
        FLOAT dy = static_cast<FLOAT>(mouseState.y) * m_scene.m_camera.lookSensitivity;

        m_scene.m_camera.camYaw += dx;
        m_scene.m_camera.camPitch -= dy;

        m_scene.m_camera.camPitch = std::clamp(m_scene.m_camera.camPitch, -89.f, 89.f);

        m_mouse->ResetScrollWheelValue();
    }
}
