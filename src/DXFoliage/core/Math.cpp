#include "stdafx.h"
#include "core/Math.h"

NSMath::SFrustum::SFrustum(DirectX::XMMATRIX viewProj)
{
    using namespace DirectX;

    XMFLOAT4X4 m{};
    XMStoreFloat4x4(&m, viewProj);

    // Left: row3 + row0
    planes[0] = XMPlaneNormalize(
        XMVectorSet(m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41)
    );
    // Right: row3 - row0
    planes[1] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41)
    );

    // Bottom: row3 + row1
    planes[2] = XMPlaneNormalize(
        XMVectorSet(m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42)
    );
    // Top: row3 - row1
    planes[3] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42)
    );

    // Near: row2 (DX clip space z in [0, 1])
    planes[4] = XMPlaneNormalize(
        XMVectorSet(m._13, m._24, m._34, m._44)
    );
    // Far: row3 - row2
    planes[5] = XMPlaneNormalize(
        XMVectorSet(m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43)
    );
}
