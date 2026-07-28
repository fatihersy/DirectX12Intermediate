#include "stdafx.h"
#include "core/Math.h"

// Gribb-Hartmann frustum-plane extraction for a row-vector (v * M)
// view-projection matrix. With this convention each plane comes from a
// COLUMN of the matrix: col3 +/- col{0,1}, and col2 alone for near
// (D3D/Vulkan clip space has z in [0, 1], unlike OpenGL's [-1, 1] which
// would use col3 + col2).
//
// NOTE: the near-plane row below was previously
//   XMVectorSet(m._13, m._24, m._34, m._44)
// which mixes column 3 into column 2 — only the first component was
// right, so the near plane was wrong. Corrected here to the full
// column 2 (_13, _23, _33, _43). SFrustum has no callers yet (it's
// culling scaffolding), so nothing depended on the old behavior.
NSMath::SFrustum::SFrustum(const NSMath::Float4x4& m)
{
    // Left: col3 + col0
    planes[0] = NormalizePlane({ m._14 + m._11, m._24 + m._21, m._34 + m._31, m._44 + m._41 });
    // Right: col3 - col0
    planes[1] = NormalizePlane({ m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41 });

    // Bottom: col3 + col1
    planes[2] = NormalizePlane({ m._14 + m._12, m._24 + m._22, m._34 + m._32, m._44 + m._42 });
    // Top: col3 - col1
    planes[3] = NormalizePlane({ m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42 });

    // Near: col2
    planes[4] = NormalizePlane({ m._13, m._23, m._33, m._43 });
    // Far: col3 - col2
    planes[5] = NormalizePlane({ m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43 });
}
