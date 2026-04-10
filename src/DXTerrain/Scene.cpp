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
    std::for_each(m_models.begin(), m_models.end(), [&rendererCtx](Model& model)
    {
        model.ResetUploadHeaps();
        model.UnloadGPU(rendererCtx);
    });
}

void Scene::OnUpdate()
{
    DirectX::XMFLOAT3 fcamEye{};
    DirectX::XMStoreFloat3(&fcamEye, m_camera.camEye);

    UpdateCamera();
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

    NSScene::Frustum frustum(pCamera->viewMatrix * pCamera->projMatrix);

    for (size_t itr{}; itr < m_models.size(); itr++)
    {
        Model& model = m_models[itr];

        if (itr == excludedModelKey.index and model.m_sceneKey.id == excludedModelKey.id) continue;

        DirectX::XMFLOAT3 fPos = model.GetPosition();
        DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&fPos);

        if (frustum.TestSphere(pos, model.collision.radius))
        {
            m_modelsCulled.push_back(model.m_sceneKey);
        }
    }
}

NSScene::Frustum::Frustum(DirectX::XMMATRIX viewProj)
{
    using namespace DirectX;

    XMFLOAT4X4 m{};
    XMStoreFloat4x4(&m, viewProj);

    // Left:   row3 + row0
    Planes[0] = XMPlaneNormalize(
        XMVectorSet(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41)
    );

    // Right:  row3 - row0
    Planes[1] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41)
    );

    // Bottom: row3 + row1
    Planes[2] = XMPlaneNormalize(
        XMVectorSet(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42)
    );

    // Top:    row3 - row1
    Planes[3] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42)
    );

    // Near:   row2  (DX clip space z in [0,1])
    Planes[4] = XMPlaneNormalize(
        XMVectorSet(m._13, m._23, m._33, m._43)
    );

    // Far:    row3 - row2
    Planes[5] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43)
    );
}
