#include "stdafx.h"
#include "Scene.h"

void Scene::OnInit(ID3D12Device14* device, IWICImagingFactory2* wicFactory, float timeOfDay)
{
    m_device = device;
    m_wicFactory = wicFactory;
    m_timeOfDay = timeOfDay;
    m_lightDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(-0.45f, -0.78f, 0.44f, 0.f));
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

    std::shared_ptr<NSScene::Camera> mainCam = m_cameras.Add();

    mainCameraKey = mainCam->m_id;
}
Scene::~Scene() {}

void Scene::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    m_models.ForEach([&rendererCtx](EntityID id, std::shared_ptr<Model> model) -> LoopCondition
    {
        model->ResetUploadHeaps();
        model->UnloadGPU(rendererCtx);

        return ELoopConditionFlag::CONTINUE;
    });

    m_models.Clear();
    m_cameras.Clear();
}

void Scene::OnUpdate()
{
    std::shared_ptr<NSScene::Camera> mainCam = m_cameras.Get(mainCameraKey);

    DirectX::XMFLOAT3 fcamEye{};
    DirectX::XMStoreFloat3(&fcamEye, mainCam->camEye);

    UpdateCamera(DE_REF(mainCam));
}


void Scene::UpdateCamera(NSScene::Camera& camera)
{
    DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationRollPitchYaw
    (
        DirectX::XMConvertToRadians(camera.camPitch),
        DirectX::XMConvertToRadians(camera.camYaw),
        0.f
    );

    camera.camFwd = DirectX::XMVector3TransformCoord({0.f, 0.f, -1.f, 0.f}, rotMatrix);
    camera.camFwd = DirectX::XMVector3Normalize(camera.camFwd);

    DirectX::XMVECTOR lookAt = DirectX::XMVectorAdd(camera.camEye, camera.camFwd);

    camera.camUp = DirectX::XMVector3TransformCoord({ 0.f, 1.f, 0.f, 0.f }, rotMatrix);
    camera.camUp = DirectX::XMVector3Normalize(camera.camUp);

    camera.viewMatrix = DirectX::XMMatrixLookAtLH(camera.camEye, lookAt, camera.camUp);
}

std::shared_ptr<NSScene::CullResult> Scene::Cull(ObserverKey camKey, Flag<NSScene::EIncCullFlag> flag, ObserverKey excludeKey)
{
    ASSERT(m_cameras.Contains(camKey));

    std::shared_ptr<NSScene::Camera> camera = m_cameras.Get(camKey);

    std::shared_ptr<NSScene::CullResult> results;

    camera->cullResults.ForEach([this, &flag, &results, &camera](EntityID, std::shared_ptr<NSScene::CullResult> cacheResult) -> LoopCondition
    {
        if (cacheResult->culledSceneKey != m_id or not cacheResult->flag.HasExact(flag)) return ELoopConditionFlag::CONTINUE;

        results = cacheResult;

        return ELoopConditionFlag::BREAK;
    });

    if (not results)
    {
        results = camera->cullResults.Add();
    }
    else if (results->generation >= camera->generation) return results;

    results->generation = camera->generation;
    NSMath::SFrustum frustum(camera->viewMatrix * camera->projMatrix);

    if (flag.HasLeastOne(NSScene::EIncCullFlag::STATIC_OBJECTS | NSScene::EIncCullFlag::DYNAMIC_OBJECTS))
    {
        m_models.ForEach([&frustum, &camera, &results, &flag](EntityID, std::shared_ptr<::Model> model) -> LoopCondition
        {
            if (not model->m_flags.HasLeastOne(NSModel::EModelFlag::STATIC | NSModel::EModelFlag::DYNAMIC)) return ELoopConditionFlag::CONTINUE;

            if (frustum.TestBounds(model->ICullable_Bound))
            {
                results->m_culledObjects.push_back(DE_REF(model));
            }
            return ELoopConditionFlag::CONTINUE;
        });
    }
    if (flag.HasLeastOne(NSScene::EIncCullFlag::TERRAIN))
    {
        m_terrain.pages.ForEach([&frustum, &camera, &results](EntityID, std::shared_ptr<NSScene::TerrainPage> page) -> LoopCondition
        {
            if (frustum.TestBounds(page->ICullable_Bound))
            {
                results->m_culledObjects.push_back(DE_REF(page));
            }

            return ELoopConditionFlag::CONTINUE;
        });
    }

    return results;
}

void Scene::SetupCameraInfiniteProjection(NSScene::Camera& camera, float fovY, float aspect, float nearZ)
{
    const float f = 1.f / tanf(fovY * 0.5f);
    camera.projMatrix = DirectX::XMMATRIX(
        f / aspect, 0.f,   0.f,  0.f,
        0.f,          f,   0.f,  0.f,
        0.f,        0.f,   0.f,  1.f,
        0.f,        0.f, nearZ,  0.f
    );
}
