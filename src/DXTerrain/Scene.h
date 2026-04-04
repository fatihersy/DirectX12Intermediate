#pragma once

#include "Model.h"

class Scene
{
public:
    Scene(){};
    Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, NSScene::Camera cam, float timeOfDay);
    ~Scene();

    void OnDestroy(NSRenderer::Ctx rendererCtx);

    void OnUpdate();
    void UpdateCamera();

    bool ValidateKey(NSModel::SceneModelKey sceneKey)
    {
        assert(m_models.size() > sceneKey.index);

        return m_models[sceneKey.index].m_sceneKey.id == sceneKey.id;
    }
    bool ValidateKeys(NSModel::SceneModelKey sceneKey, NSModel::RegisterModelKey regKey)
    {
        assert(m_models.size() > sceneKey.index);

        return
            m_models[sceneKey.index].m_sceneKey.id == sceneKey.id
            and
            m_models[sceneKey.index].m_registerKey.id == regKey.id
            and
            sceneKey.id == regKey.id;
    }

    ID3D12Device14* m_device = nullptr;
    IWICImagingFactory2* m_wicFactory = nullptr;

    template<typename T> requires NSModel::IsPrimitiveMesh<T>
    Model& AddObject(NSRenderer::Ctx rendererCtx, NSModel::PrimitiveTraits<T> desc, NSModel::AddCtx ctx)
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

    void CullScene(const NSScene::Camera* camOverride = nullptr, NSModel::SceneModelKey excludedModelKey = NSModel::SceneModelKey());

    std::vector<Model> m_models;
    std::vector<NSModel::SceneModelKey> m_modelsCulled;
    NSScene::Camera m_camera;

    float m_timeOfDay{};

    DirectX::XMVECTOR m_lightDir{};
    DirectX::XMVECTOR m_lightColor{};

    static constexpr float FAR_CLIP = 20000.f;
    static constexpr float NEAR_CLIP = 0.1f;
};
