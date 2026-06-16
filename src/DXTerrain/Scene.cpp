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
    m_models.ForEach([&rendererCtx](EntityID id, std::shared_ptr<Model> model)
    {
        model->ResetUploadHeaps();
        model->UnloadGPU(rendererCtx);
    });
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

std::weak_ptr<NSScene::CullResult> Scene::Cull(ObserverKey camKey, Flag<NSScene::EIncludeCull> flag, ObserverKey excludeKey)
{
    ASSERT(m_cameras.Contains(camKey));

    std::shared_ptr<NSScene::Camera> camera = m_cameras.Get(camKey);
    std::shared_ptr<NSScene::CullResult> result = camera->cullResults.Add();

    NSMath::SFrustum frustum(camera->viewMatrix * camera->projMatrix);

    if (flag.HasLeastAll(NSScene::EIncludeCull::STATIC_OBJECTS | NSScene::EIncludeCull::DYNAMIC_OBJECTS))
    {
        m_models.ForEach([&frustum, &camera, &result](EntityID, std::shared_ptr<::Model> model)
        {
            DirectX::XMFLOAT3 fPos = model->GetPosition();
            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&fPos);

            if (frustum.TestBounds(model->bound))
            {
                result->m_culledObjects.push_back(DE_REF(model));
            }
        });
    }

    return result;
}

bool NSScene::Camera::FindCull(Flag<NSScene::EIncludeCull> flag, ObserverKey sceneKey, std::weak_ptr<NSScene::CullResult> pResult)
{
    bool found{};

    if (auto result = pResult.lock())
    {
        cullResults.ForEach([this, &sceneKey, &flag, &found, &pResult](EntityID, std::shared_ptr<CullResult> cacheResult)
        {
            if (cacheResult->culledSceneKey == sceneKey and cacheResult->flag.HasExact(flag))
            {
                found = true;
                pResult = cacheResult;
                return;
            }
        });
    }

    return found;
}

void Scene::ForEachModel(std::function<void(Model& model)> ForEach)
{
    m_models.ForEach([&ForEach](EntityID, std::shared_ptr<Model> model)
    {
        ForEach(DE_REF(model));
    });
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
