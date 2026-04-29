#pragma once

namespace NSScene
{
    struct TerrainChunk
    {
        NSTerrain::ChunkKey key;
        NSTerrain::ChunkBounds bound;
        bool isVisible{};
    };

    struct Terrain
    {
        NSTerrain::TerrainDesc desc;
        std::vector<TerrainChunk> chunks;
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

        float camSpeed{};
        float lookSensitivity{};
    };
}
