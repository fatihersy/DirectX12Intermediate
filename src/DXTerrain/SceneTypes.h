#pragma once

#include "core/EntityTypes.h"
#include "core/Math.h"
#include "TerrainTypes.h"
#include "RendererTypes.h"

namespace NSScene
{
    struct TerrainPage
    {
        EntityKey<TerrainPage> m_id;
        NSTerrain::PageIndex index;
        ObserverKey m_registerKey;
        NSMath::BoundingBox bound;

        bool isVisibleTEMP{};
    };

    struct Terrain
    {
        NSTerrain::TerrainDesc desc;
        EntityMap<TerrainPage> pages;
        bool isInitialized{};
    };

    enum class EIncludeCull : uint32_t
    {
        STATIC_OBJECTS,
        DYNAMIC_OBJECTS,
        MODELS,
        TERRAIN,
        ALL,
        Force32Bit = UINT32_MAX
    };

    struct CullResult
    {
        EntityKey<CullResult> m_id;
        ObserverKey culledSceneKey;

        std::vector<NSMath::ICullable> m_culledObjects;

        Flag<EIncludeCull> flag;
        uint32_t generation{};
    private:
    };

    struct Camera
    {
        EntityKey<Camera> m_id;

        Camera(){};
        Camera(const DirectX::XMMATRIX proj, DirectX::XMFLOAT3 fEye, DirectX::XMFLOAT4 fFwd, DirectX::XMFLOAT4 fUp)
        {
            SetCamera(fEye, fFwd, fUp);
            projMatrix = proj;
        }
        Camera(DirectX::XMFLOAT3 fEye, DirectX::XMFLOAT4 fFwd, DirectX::XMFLOAT4 fUp)
        {
            SetCamera(fEye, fFwd, fUp);
        }

        DirectX::XMMATRIX viewMatrix{};
        DirectX::XMMATRIX projMatrix{};
        DirectX::XMVECTOR camEye{};
        DirectX::XMVECTOR camFwd{};
        DirectX::XMVECTOR camUp{};

        void SetCamera(DirectX::XMFLOAT3 fEye, DirectX::XMFLOAT4 fFwd, DirectX::XMFLOAT4 fUp)
        {
            camEye = DirectX::XMLoadFloat3(&fEye);
            camFwd = DirectX::XMLoadFloat4(&fFwd);
            camUp = DirectX::XMLoadFloat4(&fUp);

            viewMatrix = DirectX::XMMatrixLookAtLH(camEye, DirectX::XMVectorAdd(camEye, camFwd), camUp);
        }
        bool FindCull(Flag<NSScene::EIncludeCull> flag, ObserverKey sceneKey, std::weak_ptr<NSScene::CullResult> result);

        EntityMap<CullResult> cullResults;

        float camYaw{};
        float camPitch{};

        float camSpeed = 100.f;
        float lookSensitivity = .01f;
    };

    class IScene
    {
    public:
        virtual void OnDestroy(NSRenderer::Ctx rendererCtx) = 0;

        virtual bool ValidateKey(ObserverKey entKey) = 0;

        virtual NSScene::Terrain& GetTerrain() = 0;
        virtual std::shared_ptr<NSScene::Camera> GetMainCamera() = 0;

        virtual std::weak_ptr<NSScene::CullResult> Cull(ObserverKey camKey, Flag<NSScene::EIncludeCull> flag, ObserverKey excludeKey = {}) = 0;

        static constexpr float FAR_CLIP = 20000.f;
        static constexpr float NEAR_CLIP = 0.1f;
    };
}
