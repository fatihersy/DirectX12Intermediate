#pragma once

#include "Model.h"

class Scene {
public:
    Scene(){};
    Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, DirectX::XMVECTOR camEye, float camSpeed, float lookSens, float timeOfDay);
    ~Scene();

    void OnDestroy(NSRenderer::Ctx rendererCtx);

    void OnUpdate();
    void UpdateCamera();

    bool ValidateKey(SceneModelKey key) {
        return m_models.at(key.index).m_sceneKey.id == key.id;
    }
    bool ValidateKeys(SceneModelKey lhs, RegisterModelKey rhs) {
        return m_models.at(lhs.index).m_sceneKey.id == lhs.id
               and
               m_models.at(lhs.index).m_registerKey.id == rhs.id
               and
               lhs.id == rhs.id;
    }

    ID3D12Device14* m_device = nullptr;
    IWICImagingFactory2* m_wicFactory = nullptr;

    template<typename T> requires IsPrimitiveMesh<T>
    Model& AddObject(NSRenderer::Ctx rendererCtx, const char* name, DirectX::XMFLOAT3 position, PrimitiveTraits<T> desc, float metallic = 0.f, float roughness = 0.f, float opacity = 1.f)
    {
        Model& model = m_models.emplace_back(Model(name, m_device, m_wicFactory));
        model.As<T>(rendererCtx, desc);
        model.SetPosition(position);
        model.SetMetallic(metallic);
        model.SetRoughness(roughness);
        model.SetOpacity(opacity);
        return model;
    }

    bool AddObject(NSRenderer::Ctx rendererCtx, const std::filesystem::path& filepath, const char* name, Model& outModel, DirectX::XMFLOAT3 position)
    {
        outModel = m_models.emplace_back(Model(name, m_device, m_wicFactory));

        if (outModel.Load(rendererCtx, filepath))
        {
            outModel.SetPosition(position);
            return true;
        }
        return false;
    }

    void CullScene(const Camera* cam = nullptr, SceneModelKey excludeModel = {UINT32_MAX, UINT32_MAX});

    std::vector<Model> m_models;
    std::vector<SceneModelKey> m_modelsCulled;
    Camera m_camera;

    float m_timeOfDay{};

    DirectX::XMVECTOR m_lightDir{};
    DirectX::XMVECTOR m_lightColor{};

    static constexpr float FAR_CLIP = 20000.f;
    static constexpr float NEAR_CLIP = .01f;
};
