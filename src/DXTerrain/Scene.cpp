#include "stdafx.h"
#include "Scene.h"

Scene::Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, float timeOfDay) : m_device(device), m_wicFactory(wicFactory)
{
    m_timeOfDay = timeOfDay;
    m_lightDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(-0.45f, -0.78f, 0.44f, 0.f));
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);
    m_camera = NSScene::Camera({}, { 0.f, 0.f, -1.f, 0.f }, { 0.f, 1.f, 0.f, 0.f });
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

std::vector<NSModel::SceneModelKey> Scene::CullModels(const NSScene::Camera& camera, NSModel::SceneModelKey excludedModelKey)
{
    std::vector<NSModel::SceneModelKey> m_modelsCulled;

    NSMath::SFrustum frustum(camera.viewMatrix * camera.projMatrix);

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

    return m_modelsCulled;
}

NSScene::CullTerrainResult Scene::CullTerrain(const NSScene::Camera& camera)
{
    NSScene::CullTerrainResult result{};

    NSMath::SFrustum frustum(camera.viewMatrix * camera.projMatrix);

    for (NSScene::TerrainPage& page : m_terrain.pages)
    {
        NSScene::CullTerrainResult::CulledPage culledPage{};

        if (frustum.TestAABB(page.bound.aabb))
        {
            culledPage.bound = page.bound;
            culledPage.key = page.key;

            NSScene::CullTerrainResult::CulledChunk culledChunk{};

            for (NSScene::TerrainChunk& chunk : page.chunks)
            {
                if (frustum.TestAABB(chunk.bound.aabb))
                {
                    culledChunk.bound = chunk.bound;
                    culledChunk.key = chunk.key;

                    culledPage.chunks.push_back(culledChunk);
                }
            }

            result.pages.push_back(culledPage);
        }
    }

    return result;
}

void Scene::ForEachModel(std::function<void(Model& model)> ForEach)
{
    for (Model& model : m_models)
    {
        ForEach(model);
    }
}

void Scene::SetupCameraInfiniteProjection(float fovY, float aspect, float nearZ)
{
    const float f = 1.f / tanf(fovY * 0.5f);
    m_camera.projMatrix = DirectX::XMMATRIX(
        f / aspect, 0.f,   0.f,  0.f,
        0.f,          f,   0.f,  0.f,
        0.f,        0.f,   0.f,  1.f,
        0.f,        0.f, nearZ,  0.f
    );
}
