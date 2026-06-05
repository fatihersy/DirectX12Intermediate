#pragma once

namespace NSScene
{
    struct TerrainChunk
    {
        NSTerrain::ChunkKey key;
        NSTerrain::ChunkBounds bound;
    };
    struct TerrainPage
    {
        NSTerrain::PageKey key;
        NSTerrain::PageBounds bound;
        std::vector<TerrainChunk> chunks;
    };

    struct Terrain
    {
        NSTerrain::TerrainDesc desc;
        std::vector<TerrainPage> pages;
        bool isInitialized{};
    };

    struct Camera
    {
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

        float camYaw{};
        float camPitch{};

        float camSpeed = 100.f;
        float lookSensitivity = .01f;
    };

    struct CullTerrainResult
    {
        struct CulledChunk
        {
            NSTerrain::ChunkKey key;
            NSTerrain::ChunkBounds bound;
        };
        struct CulledPage
        {
            NSTerrain::PageKey key;
            NSTerrain::PageBounds bound;

            std::vector<CulledChunk> chunks;
        };

        std::vector<CulledPage> pages;
    };

    class IScene
    {
    public:
        virtual void OnDestroy(NSRenderer::Ctx rendererCtx) = 0;

        virtual bool ValidateKey(NSModel::SceneModelKey sceneKey) = 0;
        virtual bool ValidateKeys(NSModel::SceneModelKey sceneKey, NSModel::RegisterModelKey regKey) = 0;

        virtual const NSScene::Terrain& GetTerrain() const = 0;
        virtual const NSScene::Camera& GetMainCamera() const = 0;

        virtual std::vector<NSModel::SceneModelKey> CullModels(const NSScene::Camera& camera, NSModel::SceneModelKey excludedModelKey = NSModel::SceneModelKey()) = 0;
        virtual CullTerrainResult CullTerrain(const NSScene::Camera& camera) = 0;

        static constexpr float FAR_CLIP = 20000.f;
        static constexpr float NEAR_CLIP = 0.1f;
    };
}
