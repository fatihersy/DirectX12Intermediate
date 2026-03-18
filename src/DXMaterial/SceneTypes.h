#pragma once

struct Camera
{
    Camera() {};
    Camera(const DirectX::XMMATRIX proj, DirectX::XMFLOAT3 fEye, DirectX::XMFLOAT4 fDir, DirectX::XMFLOAT4 fUp)
    {
        SetCamera(fEye, fDir, fUp);
        projectionMatrix = proj;
    }

    DirectX::XMMATRIX viewMatrix{};
    DirectX::XMMATRIX projectionMatrix{};
    DirectX::XMVECTOR camEye{};
    DirectX::XMVECTOR camFwd{};
    DirectX::XMVECTOR camUp{};

    void SetCamera(DirectX::XMFLOAT3 fEye, DirectX::XMFLOAT4 fDir, DirectX::XMFLOAT4 fUp)
    {
        camEye = DirectX::XMLoadFloat3(&fEye);
        camFwd = DirectX::XMLoadFloat4(&fDir);
        camUp = DirectX::XMLoadFloat4(&fUp);

        viewMatrix = DirectX::XMMatrixLookAtLH(camEye, DirectX::XMVectorAdd(camEye, camFwd), camUp);
    }

    float camYaw{};
    float camPitch{};

    float camSpeed{};
    float lookSensitivity{};
};

class Frustum
{
public:
    Frustum() {};
    Frustum(DirectX::XMMATRIX viewProj);

    bool TestSphere(DirectX::FXMVECTOR center, float radius) const {
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
