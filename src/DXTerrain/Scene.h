#pragma once

#include "Model.h"

class Scene : public NSScene::IScene
{
public:
    Scene(){};
    Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, float timeOfDay);
    ~Scene();

    void OnDestroy(NSRenderer::Ctx rendererCtx) override;

    void OnUpdate();
    void UpdateCamera();

    bool ValidateKey(NSModel::SceneModelKey sceneKey) override
    {
        return
            m_models.size() > sceneKey.index
            and
            m_models[sceneKey.index].m_sceneKey.id == sceneKey.id;
    }
    bool ValidateKeys(NSModel::SceneModelKey sceneKey, NSModel::RegisterModelKey regKey) override
    {
        return
            m_models.size() > sceneKey.index
            and
            m_models[sceneKey.index].m_sceneKey.id == sceneKey.id
            and
            m_models[sceneKey.index].m_registerKey.id == regKey.id
            and
            sceneKey.id == regKey.id;
    }

    const NSScene::Terrain& GetTerrain() const override {
        return m_terrain;
    }
    const NSScene::Camera& GetMainCamera() const override {
        return m_camera;
    }

    std::vector<NSModel::SceneModelKey> CullModels(const NSScene::Camera& camera, NSModel::SceneModelKey excludedModelKey = NSModel::SceneModelKey()) override;
    std::vector<NSTerrain::ChunkKey> CullTerrain(const NSScene::Camera& camera) override;

    template<typename T> requires NSModel::IsPrimitiveMesh<T>
    Model& AddObject(NSModel::AddCtx ctx, NSModel::PrimitiveTraits<T> desc, NSRenderer::Ctx rendererCtx)
    {
        Model& model = m_models.emplace_back(Model(m_device, m_wicFactory, ctx.name));

        model
            .As<T>(rendererCtx, desc)
            .SetPosition(ctx.position)
            .SetMetallic(ctx.metallic)
            .SetRoughness(ctx.roughness)
            .SetOpacity(ctx.opacity);

        return model;
    }
    bool AddObject(NSRenderer::Ctx rendererCtx, const std::filesystem::path& path, NSModel::AddCtx ctx, Model& outModel)
    {
        outModel = m_models.emplace_back(Model(m_device, m_wicFactory, ctx.name));

        if (outModel.Load(rendererCtx, path))
        {
            outModel
                .SetPosition(ctx.position)
                .SetMetallic(ctx.metallic)
                .SetRoughness(ctx.roughness)
                .SetOpacity(ctx.opacity);

            return true;
        }

        return false;
    }

    void ForEachModel(std::function<void(Model& model)> ForEach);

    void SetupCameraInfiniteProjection(float fovY, float aspect, float nearZ);

    ID3D12Device14* m_device = nullptr;
    IWICImagingFactory2* m_wicFactory = nullptr;
    NSScene::Terrain m_terrain{};

    std::vector<Model> m_models;
    NSScene::Camera m_camera{};

    float m_timeOfDay{};

    DirectX::XMVECTOR m_lightDir{};
    DirectX::XMVECTOR m_lightColor{};
};
