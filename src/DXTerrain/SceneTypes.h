#pragma once

#include "core/EntityTypes.h"
#include "core/Math.h"
#include "RendererTypes.h"

namespace NSScene
{
    enum class EIncCullFlag : uint32_t
    {
        UNDEFINED           = 0,
        STATIC_OBJECTS      = 1 << 0,
        DYNAMIC_OBJECTS     = 1 << 1,
        TERRAIN             = 1 << 2,
        ALL                 = 1 << 3,
        Force32Bit = UINT32_MAX
    };

    struct CullResult
    {
        EntityKey<CullResult> m_id;
        ObserverKey culledSceneKey;

        std::vector<NSMath::ICullable> m_culledObjects;

        Flag<EIncCullFlag> flag;
        uint64_t generation{};
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

        EntityMap<CullResult> cullResults;
        uint64_t generation{};

        float camYaw{};
        float camPitch{};

        float camSpeed = 5.f;
        float lookSensitivity = .01f;
    };

    class IScene
    {
    public:
        virtual void OnDestroy(NSRenderer::Ctx rendererCtx) = 0;

        virtual bool ValidateKey(ObserverKey entKey) = 0;

        virtual std::shared_ptr<NSScene::Camera> GetMainCamera() = 0;
        virtual std::shared_ptr<NSScene::CullResult> Cull(ObserverKey camKey, Flag<NSScene::EIncCullFlag> flag, ObserverKey excludeKey = {}) = 0;

        static constexpr float FAR_CLIP = 20000.f;
        static constexpr float NEAR_CLIP = 0.1f;
    };
}
