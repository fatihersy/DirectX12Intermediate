#include "stdafx.h"
#include "Scene.h"

#include "IApp.h"

#include "Renderer.h"

Scene::Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, NSScene::Camera cam, float timeOfDay) : m_device(device), m_wicFactory(wicFactory)
{
    m_camera = cam;
    m_camera.projMatrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, IApp::GetInstance()->im_aspectRatio, NEAR_CLIP, FAR_CLIP);
    m_camera.camSpeed = 10.f;
    m_camera.lookSensitivity = .1f;

    m_timeOfDay = timeOfDay;

    m_lightDir = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.f);
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);
}
Scene::~Scene() {}

void Scene::OnDestroy(NSRenderer::Ctx rendererCtx)
{
    for (Model& model : m_models)
    {
        model.ResetUploadHeaps();
        model.UnloadGPU(rendererCtx);
    }
}

void Scene::OnUpdate()
{
    DirectX::XMFLOAT3 fcamEye{};
    DirectX::XMStoreFloat3(&fcamEye, m_camera.camEye);

    UpdateCamera();

    NSMath::SFrustum frustum(m_camera.viewMatrix * m_camera.projMatrix);

    for (NSScene::TerrainChunk& chunk : m_terrain.chunks)
    {
        // Whole for loop can be moved
        chunk.isVisible = frustum.TestAABB(chunk.bound.aabb);
    }
}


void Scene::UpdateCamera()
{
    DirectX::XMMATRIX rotMatrix = DirectX::XMMatrixRotationRollPitchYaw(
        DirectX::XMConvertToRadians(m_camera.camPitch),
        DirectX::XMConvertToRadians(m_camera.camYaw),
        0.f
    );

    m_camera.camFwd = DirectX::XMVector3TransformCoord({0.f, 0.f, -1.f, 0.f}, rotMatrix);
    m_camera.camFwd = DirectX::XMVector3Normalize(m_camera.camFwd);

    DirectX::XMVECTOR lookAt = DirectX::XMVectorAdd(m_camera.camEye, m_camera.camFwd);

    m_camera.camUp = DirectX::XMVector3TransformCoord({ 0.f, 1.f, 0.f, 0.f }, rotMatrix);
    m_camera.camUp = DirectX::XMVector3Normalize(m_camera.camUp);

    m_camera.viewMatrix = DirectX::XMMatrixLookAtLH(m_camera.camEye, lookAt, m_camera.camUp);
}

void Scene::CullScene(const NSScene::Camera* camOverride, NSModel::SceneModelKey excludedModelKey)
{
    const NSScene::Camera* pCamera = camOverride ? camOverride : &m_camera;

    m_modelsCulled.clear();

    NSMath::SFrustum frustum(pCamera->viewMatrix * pCamera->projMatrix);

    for (size_t itr{}; itr < m_models.size(); itr++)
    {
        Model& model = m_models[itr];

        if (itr == excludedModelKey.index and model.m_sceneKey.id == excludedModelKey.id) continue;

        DirectX::XMFLOAT3 fPos = model.GetPosition();
        DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&fPos);

        if (frustum.TestSphere(model.m_collision))
        {
            m_modelsCulled.push_back(model.m_sceneKey);
        }
    }
}

void Scene::ForEachModel(std::function<void(Model& model)> ForEach)
{
    for (Model& model : m_models)
    {
        ForEach(model);
    }
}
