#pragma once

#include "SceneTypes.h"
#include "TerrainTypes.h"

#include "Model.h"

class Scene : public NSScene::IScene
{
public:
    Scene(){};
    ~Scene();

    EntityKey<Scene> m_id;

    void OnInit(NSRenderer::Ctx rendererCtx, ID3D12Device14* device, IWICImagingFactory2* wicFactory, std::shared_ptr<NSTerrain::ITerrainView> inTerrainView, float timeOfDay);
    void OnDestroy(NSRenderer::Ctx rendererCtx) override;

    void OnUpdate();
    void UpdateCamera(NSScene::Camera& camera);

    bool ValidateKey(ObserverKey entKey) override
    {
        return m_models.Contains(entKey);
    }

    std::shared_ptr<NSScene::CullResult> Cull(ObserverKey camKey, Flag<NSScene::EIncCullFlag> flag, ObserverKey excludeKey = {}) override;
    std::shared_ptr<NSScene::Camera> GetMainCamera() override {
        ASSERT(m_cameras.Contains(mainCameraKey), "Main camera did't initialized yet");
        return m_cameras.Get(mainCameraKey);
    }

    template<typename T> requires NSModel::IsPrimitiveMesh<T>
    std::shared_ptr<::Model> AddObject(NSModel::AddCtx ctx, NSModel::PrimitiveTraits<T> desc, NSRenderer::Ctx rendererCtx)
    {
        std::shared_ptr<::Model> model = m_models.Add(std::make_shared<Model>(Model(m_device, m_wicFactory, ctx.name)));

        model->As<T>(rendererCtx, desc)
                .SetPosition(ctx.position)
                .SetMetallic(ctx.metallic)
                .SetRoughness(ctx.roughness)
                .SetOpacity(ctx.opacity);

        return model;
    }
    bool AddObject(NSRenderer::Ctx rendererCtx, const std::filesystem::path& path, NSModel::AddCtx ctx, std::shared_ptr<Model> outModel)
    {
        outModel = m_models.Add(std::make_shared<::Model>(Model(m_device, m_wicFactory, ctx.name)));

        if (outModel->Load(rendererCtx, path))
        {
            outModel->
                 SetPosition(ctx.position)
                .SetMetallic(ctx.metallic)
                .SetRoughness(ctx.roughness)
                .SetOpacity(ctx.opacity);

            return true;
        }

        return false;
    }

    void SetupCameraInfiniteProjection(NSScene::Camera& camera, float fovY, float aspect, float nearZ);

    ID3D12Device14* m_device = nullptr;
    IWICImagingFactory2* m_wicFactory = nullptr;
    std::shared_ptr<NSTerrain::ITerrainView> m_terrainView{};

    EntityMap<Model> m_models;
    EntityMap<NSScene::Camera> m_cameras;

    float m_timeOfDay{};

    DirectX::XMVECTOR m_lightDir{};
    DirectX::XMVECTOR m_lightColor{};

    private:
        ObserverKey mainCameraKey;
};
