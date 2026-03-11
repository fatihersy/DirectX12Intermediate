#pragma once

#include "Model.h"

struct Camera
{
    DirectX::XMMATRIX viewMatrix;
    DirectX::XMMATRIX projectionMatrix;
    DirectX::XMVECTOR camEye;
    DirectX::XMVECTOR camFwd;
    DirectX::XMVECTOR camUp;

    float camYaw{};
    float camPitch{};

    float camSpeed{};
    float lookSensitivity{};
};

class Frustum {
    Frustum() {};
    Frustum(DirectX::XMMATRIX viewProj);

    inline bool TestSphere(DirectX::FXMVECTOR center, float radius) const {
        using namespace DirectX;
        for (size_t i = 0; i < 6; i++)
        {
            float dist = XMVectorGetX(XMPlaneDot(Planes[i], center));
            if (dist < -radius)
                return false;
        }
        return true;
    }

    DirectX::XMVECTOR Planes[6]{};
};

class Scene {
public:
    Scene(){};
    Scene(ID3D12Device14* device, IWICImagingFactory2* wicFactory, DirectX::XMVECTOR camEye, float camSpeed, float lookSens, float timeOfDay);
    ~Scene();

    void OnDestroy(NSRenderer::Ctx& rendererCtx);

    void OnUpdate();
    void UpdateCamera();

    ID3D12Device14* m_device = nullptr;
    IWICImagingFactory2* m_wicFactory = nullptr;

    template<typename T> requires IsPrimitiveMesh<T>
    Model& AddObject(NSRenderer::Ctx rendererCtx, const char* name, DirectX::XMFLOAT3 position, PrimitiveTraits<T> desc, float metallic = 0.f, float roughness = 0.f, float opacity = 1.f)
    {
        Model& model = m_models.emplace_back(Model(name, m_device, m_wicFactory));
        model.As<T>(rendererCtx, desc);
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

    std::vector<Model> m_models;
    Camera m_camera;

    float m_timeOfDay{};

    DirectX::XMVECTOR m_lightDir{};
    DirectX::XMVECTOR m_lightColor{};

    static constexpr float FAR_CLIP = 20000.f;
    static constexpr float NEAR_CLIP = .01f;
};
