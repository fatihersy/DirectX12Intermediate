#include "stdafx.h"
#include "Scene.h"

#include <unordered_set>

void Scene::OnInit(NSRenderer::Ctx rendererCtx, ID3D12Device14* device, IWICImagingFactory2* wicFactory, std::shared_ptr<NSTerrain::ITerrainView> inTerrainView, float timeOfDay)
{
    m_device = device;
    m_wicFactory = wicFactory;
    m_timeOfDay = timeOfDay;
    m_lightDir = DirectX::XMVector3Normalize(DirectX::XMVectorSet(-0.45f, -0.78f, 0.44f, 0.f));
    m_lightColor = DirectX::XMVectorSet(0.9f, 0.9f, 0.9f, 1.0f);

    std::shared_ptr<NSScene::Camera> mainCam = m_cameras.Add();

    mainCameraKey = mainCam->m_id;

    m_terrainView = inTerrainView;

    const NSTerrain::TerrainDesc& desc = m_terrainView->GetDesc();
    m_pageCountX = desc.pageCountX;
    m_pageCountZ = desc.pageCountZ;
    m_worldPageWidth = desc.worldWidth / static_cast<float>(desc.pageCountX);
    m_worldPageDepth = desc.worldDepth / static_cast<float>(desc.pageCountZ);
    m_halfWorldWidth = 0.5f * desc.worldWidth;
    m_halfWorldDepth = 0.5f * desc.worldDepth;

    const uint32_t streamingDistance = rendererCtx.rendererDesc.streamingDistance;
    m_pageRadius = std::max(
        static_cast<int>(std::ceil(static_cast<float>(streamingDistance) / m_worldPageWidth)),
        static_cast<int>(std::ceil(static_cast<float>(streamingDistance) / m_worldPageDepth))
    );

    m_pageGrid.assign(static_cast<size_t>(m_pageCountX) * m_pageCountZ, nullptr);
    m_terrainView->GetPageLayout().ForEach([this](EntityID, std::shared_ptr<const NSTerrain::TerrainPage> page) -> LoopCondition
    {
        m_pageGrid[static_cast<size_t>(page->index.gridZ) * m_pageCountX + page->index.gridX] = page;
        return ELoopConditionFlag::CONTINUE;
    });

    DirectX::XMFLOAT3 camPos{};
    DirectX::XMStoreFloat3(&camPos, mainCam->camEye);
    UpdateTerrainStreaming(camPos);
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
    m_terrainView.reset();
}

void Scene::OnUpdate(NSRenderer::Ctx rendererCtx)
{
    std::shared_ptr<NSScene::Camera> mainCam = m_cameras.Get(mainCameraKey);

    DirectX::XMFLOAT3 camEye{};
    DirectX::XMStoreFloat3(&camEye, mainCam->camEye);

    UpdateCamera(DE_REF(mainCam));

    UpdateTerrainStreaming(camEye);
}

void Scene::UpdateTerrainStreaming(const DirectX::XMFLOAT3& camEye)
{
    using ESlotResidency = NSTerrain::StreamSlot::ESlotResidency;

    if (m_pageGrid.empty() || m_pageCountX == 0u || m_pageCountZ == 0u) return;


    const int camGX = static_cast<int>(std::floor((camEye.x + m_halfWorldWidth) / m_worldPageWidth));
    const int camGZ = static_cast<int>(std::floor((camEye.z + m_halfWorldDepth) / m_worldPageDepth));

    if (m_streamInitialized && camGX == m_lastCamGX && camGZ == m_lastCamGZ) return;
    m_streamInitialized = true;
    m_lastCamGX = camGX;
    m_lastCamGZ = camGZ;

    const int minX = std::max(0, camGX - m_pageRadius);
    const int maxX = std::min(static_cast<int>(m_pageCountX) - 1, camGX + m_pageRadius);
    const int minZ = std::max(0, camGZ - m_pageRadius);
    const int maxZ = std::min(static_cast<int>(m_pageCountZ) - 1, camGZ + m_pageRadius);
    if (minX > maxX || minZ > maxZ) return;

    EntityMap<NSTerrain::StreamSlotView>& slotViews = m_terrainView->GetSlotsView();
    const EntityMap<NSTerrain::TerrainPage>& pageLayout = m_terrainView->GetPageLayout();

    std::vector<std::shared_ptr<NSTerrain::StreamSlotView>> freeSlots;
    std::unordered_set<uint32_t> covered;

    slotViews.ForEach([&](EntityID, std::shared_ptr<NSTerrain::StreamSlotView> view) -> LoopCondition
    {
        if (view->residency == ESlotResidency::Unknown)
        {
            freeSlots.push_back(view);
            return ELoopConditionFlag::CONTINUE;
        }

        std::shared_ptr<const NSTerrain::TerrainPage> page = pageLayout.Get(view->pageKey);
        if (page == nullptr)
        {
            view->residency = ESlotResidency::Unknown;
            freeSlots.push_back(view);
            return ELoopConditionFlag::CONTINUE;
        }

        const int gx = static_cast<int>(page->index.gridX);
        const int gz = static_cast<int>(page->index.gridZ);
        const bool inBlock = (gx >= minX && gx <= maxX && gz >= minZ && gz <= maxZ);

        if (inBlock)
        {
            covered.insert(static_cast<uint32_t>(gz) * m_pageCountX + static_cast<uint32_t>(gx));
        }
        else if (view->residency != ESlotResidency::Loading)
        {
            view->residency = ESlotResidency::Unknown;
            freeSlots.push_back(view);
        }

        return ELoopConditionFlag::CONTINUE;
    });

    size_t nextFree = 0u;
    for (int gz = minZ; gz <= maxZ; ++gz)
    {
        for (int gx = minX; gx <= maxX; ++gx)
        {
            const uint32_t lin = static_cast<uint32_t>(gz) * m_pageCountX + static_cast<uint32_t>(gx);
            if (covered.count(lin)) continue;

            std::shared_ptr<const NSTerrain::TerrainPage> page = m_pageGrid[lin];
            if (page == nullptr) continue;

            if (nextFree >= freeSlots.size()) return;

            std::shared_ptr<NSTerrain::StreamSlotView> view = freeSlots[nextFree++];
            view->pageKey = page->m_id;
            view->ICullable_Bound = page->ICullable_Bound;
            view->residency = ESlotResidency::Missing;
        }
    }
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
        EntityMap<NSTerrain::StreamSlotView>& streamSlotViews = m_terrainView->GetSlotsView();

        streamSlotViews.ForEach([&frustum, &camera, &results](EntityID, std::shared_ptr<NSTerrain::StreamSlotView> view) -> LoopCondition
        {
            if (view->residency == NSTerrain::StreamSlot::ESlotResidency::Unknown) return ELoopConditionFlag::CONTINUE;

            if (frustum.TestBounds(view->ICullable_Bound))
            {
                results->m_culledObjects.push_back(DE_REF(view));
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
