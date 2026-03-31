#pragma once

#include "stdafx.h"
#include "MeshTypes.h"

class Primitives
{
public:

    static void CreateSphere(
        float radius,
        UINT sliceCount,
        UINT stackCount,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        // Top pole vertex
        Vertex topVertex;
        topVertex.position = DirectX::XMFLOAT3(0.0f, radius, 0.0f);
        topVertex.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
        topVertex.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
        topVertex.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
        topVertex.texCoord = DirectX::XMFLOAT2(0.0f, 0.0f);
        outVertices.push_back(topVertex);

        float phiStep = DirectX::XM_PI / stackCount;
        float thetaStep = 2.0f * DirectX::XM_PI / sliceCount;

        // Generate vertices for each stack ring
        for (UINT i = 1; i <= stackCount - 1; ++i)
        {
            float phi = i * phiStep;

            for (UINT j = 0; j <= sliceCount; ++j)
            {
                float theta = j * thetaStep;

                Vertex v;

                // Spherical to Cartesian
                v.position.x = radius * sinf(phi) * cosf(theta);
                v.position.y = radius * cosf(phi);
                v.position.z = radius * sinf(phi) * sinf(theta);

                // Normal for sphere is normalized position
                DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&v.position);
                DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(P));

                // Tangent
                DirectX::XMFLOAT3 tangent;
                tangent.x = -radius * sinf(phi) * sinf(theta);
                tangent.y = 0.0f;
                tangent.z = radius * sinf(phi) * cosf(theta);
                DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&tangent);
                DirectX::XMStoreFloat3(&v.tangent, DirectX::XMVector3Normalize(T));

                // Bitangent
                DirectX::XMVECTOR N = DirectX::XMLoadFloat3(&v.normal);
                T = DirectX::XMLoadFloat3(&v.tangent);
                DirectX::XMVECTOR B = DirectX::XMVector3Cross(N, T);
                DirectX::XMStoreFloat3(&v.bitangent, DirectX::XMVector3Normalize(B));

                // UV coordinates
                v.texCoord.x = theta / (2.0f * DirectX::XM_PI);
                v.texCoord.y = phi / DirectX::XM_PI;

                outVertices.push_back(v);
            }
        }

        // Bottom pole vertex
        Vertex bottomVertex;
        bottomVertex.position = DirectX::XMFLOAT3(0.0f, -radius, 0.0f);
        bottomVertex.normal = DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f);
        bottomVertex.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
        bottomVertex.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
        bottomVertex.texCoord = DirectX::XMFLOAT2(0.0f, 1.0f);
        outVertices.push_back(bottomVertex);

        // Top cap indices
        for (UINT i = 1; i <= sliceCount; ++i)
        {
            outIndices.push_back(0);
            outIndices.push_back(i + 1);
            outIndices.push_back(i);
        }

        // Interior stack indices
        UINT baseIndex = 1;
        UINT ringVertexCount = sliceCount + 1;
        for (UINT i = 0; i < stackCount - 2; ++i)
        {
            for (UINT j = 0; j < sliceCount; ++j)
            {
                outIndices.push_back(baseIndex + i * ringVertexCount + j);
                outIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                outIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
            }
        }

        // Bottom cap indices
        UINT southPoleIndex = static_cast<UINT>(outVertices.size()) - 1;
        baseIndex = southPoleIndex - ringVertexCount;

        for (UINT i = 0; i < sliceCount; ++i)
        {
            outIndices.push_back(southPoleIndex);
            outIndices.push_back(baseIndex + i);
            outIndices.push_back(baseIndex + i + 1);
        }
    }

    static void CreateCube(
        float width,
        float height,
        float depth,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        float w = width * 0.5f;
        float h = height * 0.5f;
        float d = depth * 0.5f;

        // 24 vertices (4 per face for proper UVs and normals)
        Vertex vertices[24];

        // Front face (+Z)
        vertices[0] = { {-w, -h, d}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 1} };
        vertices[1] = { {-w,  h, d}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0} };
        vertices[2] = { { w,  h, d}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {1, 0} };
        vertices[3] = { { w, -h, d}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {1, 1} };

        // Back face (-Z)
        vertices[4] = { { w, -h, -d}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, 1} };
        vertices[5] = { { w,  h, -d}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, 0} };
        vertices[6] = { {-w,  h, -d}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {1, 0} };
        vertices[7] = { {-w, -h, -d}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {1, 1} };

        // Top face (+Y)
        vertices[8] = { {-w, h, -d}, {0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1} };
        vertices[9] = { {-w, h,  d}, {0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0} };
        vertices[10] = { { w, h,  d}, {0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {1, 0} };
        vertices[11] = { { w, h, -d}, {0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {1, 1} };

        // Bottom face (-Y)
        vertices[12] = { {-w, -h,  d}, {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, {0, 1} };
        vertices[13] = { {-w, -h, -d}, {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0} };
        vertices[14] = { { w, -h, -d}, {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, {1, 0} };
        vertices[15] = { { w, -h,  d}, {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, {1, 1} };

        // Right face (+X)
        vertices[16] = { {w, -h,  d}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {0, 1} };
        vertices[17] = { {w,  h,  d}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {0, 0} };
        vertices[18] = { {w,  h, -d}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {1, 0} };
        vertices[19] = { {w, -h, -d}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {1, 1} };

        // Left face (-X)
        vertices[20] = { {-w, -h, -d}, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1} };
        vertices[21] = { {-w,  h, -d}, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 0} };
        vertices[22] = { {-w,  h,  d}, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 0} };
        vertices[23] = { {-w, -h,  d}, {-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 1} };

        outVertices.assign(vertices, vertices + 24);

        // 36 indices (6 faces * 2 triangles * 3 vertices)
        UINT indices[] = {
            0,  1,  2,  0,  2,  3,   // Front
            4,  5,  6,  4,  6,  7,   // Back
            8,  9,  10, 8,  10, 11,  // Top
            12, 13, 14, 12, 14, 15,  // Bottom
            16, 17, 18, 16, 18, 19,  // Right
            20, 21, 22, 20, 22, 23   // Left
        };

        outIndices.assign(indices, indices + 36);
    }


    static void CreatePlane(
        float width,
        float depth,
        UINT widthSubdivisions,
        UINT depthSubdivisions,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        UINT vertexCount = (widthSubdivisions + 1) * (depthSubdivisions + 1);
        outVertices.reserve(vertexCount);

        float halfWidth = width * 0.5f;
        float halfDepth = depth * 0.5f;

        float dx = width / widthSubdivisions;
        float dz = depth / depthSubdivisions;

        float du = 1.0f / widthSubdivisions;
        float dv = 1.0f / depthSubdivisions;

        // Create vertices
        for (UINT i = 0; i <= depthSubdivisions; ++i)
        {
            float z = halfDepth - i * dz;
            for (UINT j = 0; j <= widthSubdivisions; ++j)
            {
                float x = -halfWidth + j * dx;

                Vertex v;
                v.position = DirectX::XMFLOAT3(x, 0.0f, z);
                v.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
                v.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
                v.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f);
                v.texCoord = DirectX::XMFLOAT2(j * du, i * dv);

                outVertices.push_back(v);
            }
        }

        // Create indices
        for (UINT i = 0; i < depthSubdivisions; ++i)
        {
            for (UINT j = 0; j < widthSubdivisions; ++j)
            {
                UINT topLeft = i * (widthSubdivisions + 1) + j;
                UINT topRight = topLeft + 1;
                UINT bottomLeft = (i + 1) * (widthSubdivisions + 1) + j;
                UINT bottomRight = bottomLeft + 1;

                // First triangle
                outIndices.push_back(topLeft);
                outIndices.push_back(bottomLeft);
                outIndices.push_back(topRight);

                // Second triangle
                outIndices.push_back(topRight);
                outIndices.push_back(bottomLeft);
                outIndices.push_back(bottomRight);
            }
        }
    }


    static void CreateCylinder(
        float topRadius,
        float bottomRadius,
        float height,
        UINT sliceCount,
        UINT stackCount,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        float stackHeight = height / stackCount;
        float radiusStep = (topRadius - bottomRadius) / stackCount;
        UINT ringCount = stackCount + 1;

        // Generate vertices for each stack ring
        for (UINT i = 0; i < ringCount; ++i)
        {
            float y = -0.5f * height + i * stackHeight;
            float r = bottomRadius + i * radiusStep;

            float dTheta = 2.0f * DirectX::XM_PI / sliceCount;
            for (UINT j = 0; j <= sliceCount; ++j)
            {
                Vertex v;

                float c = cosf(j * dTheta);
                float s = sinf(j * dTheta);

                v.position = DirectX::XMFLOAT3(r * c, y, r * s);

                v.texCoord.x = static_cast<float>(j) / sliceCount;
                v.texCoord.y = 1.0f - static_cast<float>(i) / stackCount;

                // Tangent
                v.tangent = DirectX::XMFLOAT3(-s, 0.0f, c);

                // Compute normal (slope in Y direction)
                float dr = bottomRadius - topRadius;
                DirectX::XMFLOAT3 bitangent(dr * c, -height, dr * s);

                DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&v.tangent);
                DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);
                DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
                DirectX::XMStoreFloat3(&v.normal, N);

                // Store bitangent
                DirectX::XMStoreFloat3(&v.bitangent, DirectX::XMVector3Normalize(B));

                outVertices.push_back(v);
            }
        }

        // Add indices for sides
        UINT ringVertexCount = sliceCount + 1;
        for (UINT i = 0; i < stackCount; ++i)
        {
            for (UINT j = 0; j < sliceCount; ++j)
            {
                UINT baseIdx = i * ringVertexCount;

                outIndices.push_back(baseIdx + j);
                outIndices.push_back(baseIdx + j + 1);
                outIndices.push_back(baseIdx + j + ringVertexCount);

                outIndices.push_back(baseIdx + j + ringVertexCount);
                outIndices.push_back(baseIdx + j + 1);
                outIndices.push_back(baseIdx + j + ringVertexCount + 1);
            }
        }

        // Build top cap
        BuildCylinderCap(topRadius, height, sliceCount, true, outVertices, outIndices);

        // Build bottom cap
        BuildCylinderCap(bottomRadius, height, sliceCount, false, outVertices, outIndices);
    }


    static void CreateCone(
        float bottomRadius,
        float height,
        UINT sliceCount,
        UINT stackCount,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        CreateCylinder(0.0f, bottomRadius, height, sliceCount, stackCount, outVertices, outIndices);
    }

    static void CreateDome(
        float radius,
        UINT sliceCount,
        UINT stackCount,
        std::vector<Vertex>& outVertices,
        std::vector<UINT>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        // Top pole vertex
        Vertex topVertex;
        topVertex.position = DirectX::XMFLOAT3(0.0f, radius, 0.0f);
        topVertex.normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
        topVertex.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
        topVertex.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
        topVertex.texCoord = DirectX::XMFLOAT2(0.5f, 0.0f); // Adjusted for dome UV range
        outVertices.push_back(topVertex);

        float phiStep = DirectX::XM_PI / (2.0f * stackCount); // Half the sphere for hemisphere
        float thetaStep = 2.0f * DirectX::XM_PI / sliceCount;

        // Generate vertices for each stack ring in the upper hemisphere
        for (UINT i = 1; i <= stackCount; ++i)
        {
            float phi = i * phiStep;

            for (UINT j = 0; j <= sliceCount; ++j)
            {
                float theta = j * thetaStep;

                Vertex v;

                // Spherical to Cartesian
                v.position.x = radius * sinf(phi) * cosf(theta);
                v.position.y = radius * cosf(phi);
                v.position.z = radius * sinf(phi) * sinf(theta);

                // Normal for sphere is normalized position
                DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&v.position);
                DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(P));

                // Tangent
                DirectX::XMFLOAT3 tangent;
                tangent.x = -radius * sinf(phi) * sinf(theta);
                tangent.y = 0.0f;
                tangent.z = radius * sinf(phi) * cosf(theta);
                DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&tangent);
                DirectX::XMStoreFloat3(&v.tangent, DirectX::XMVector3Normalize(T));

                // Bitangent
                DirectX::XMVECTOR N = DirectX::XMLoadFloat3(&v.normal);
                T = DirectX::XMLoadFloat3(&v.tangent);
                DirectX::XMVECTOR B = DirectX::XMVector3Cross(N, T);
                DirectX::XMStoreFloat3(&v.bitangent, DirectX::XMVector3Normalize(B));

                // UV coordinates (scale y to 0-1 for dome: top 0, horizon 1)
                v.texCoord.x = theta / (2.0f * DirectX::XM_PI);
                v.texCoord.y = phi / (DirectX::XM_PI / 2.0f);

                outVertices.push_back(v);
            }
        }

        // Top cap indices
        for (UINT i = 1; i <= sliceCount; ++i)
        {
            outIndices.push_back(0);
            outIndices.push_back(i + 1);
            outIndices.push_back(i);
        }

        // Interior stack indices
        UINT baseIndex = 1;
        UINT ringVertexCount = sliceCount + 1;
        for (UINT i = 0; i < stackCount - 1; ++i)
        {
            for (UINT j = 0; j < sliceCount; ++j)
            {
                outIndices.push_back(baseIndex + i * ringVertexCount + j);
                outIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                outIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
                outIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
            }
        }
    }

private:
    static void BuildCylinderCap(
        float radius,
        float height,
        UINT sliceCount,
        bool isTop,
        std::vector<Vertex>& vertices,
        std::vector<UINT>& indices)
    {
        UINT baseIndex = static_cast<UINT>(vertices.size());

        float y = 0.5f * height;
        if (!isTop) y = -y;

        float dTheta = 2.0f * DirectX::XM_PI / sliceCount;

        // Cap center vertex
        Vertex centerVertex;
        centerVertex.position = DirectX::XMFLOAT3(0.0f, y, 0.0f);
        centerVertex.normal = DirectX::XMFLOAT3(0.0f, isTop ? 1.0f : -1.0f, 0.0f);
        centerVertex.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
        centerVertex.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
        centerVertex.texCoord = DirectX::XMFLOAT2(0.5f, 0.5f);
        vertices.push_back(centerVertex);

        // Cap ring vertices
        for (UINT i = 0; i <= sliceCount; ++i)
        {
            float x = radius * cosf(i * dTheta);
            float z = radius * sinf(i * dTheta);

            Vertex v;
            v.position = DirectX::XMFLOAT3(x, y, z);
            v.normal = DirectX::XMFLOAT3(0.0f, isTop ? 1.0f : -1.0f, 0.0f);
            v.tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
            v.bitangent = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);

            v.texCoord.x = x / radius * 0.5f + 0.5f;
            v.texCoord.y = z / radius * 0.5f + 0.5f;

            vertices.push_back(v);
        }

        // Cap indices
        UINT centerIndex = baseIndex;
        UINT ringStartIndex = baseIndex + 1;

        for (UINT i = 0; i < sliceCount; ++i)
        {
            if (isTop)
            {
                indices.push_back(centerIndex);
                indices.push_back(ringStartIndex + i);
                indices.push_back(ringStartIndex + i + 1);
            }
            else
            {
                indices.push_back(centerIndex);
                indices.push_back(ringStartIndex + i + 1);
                indices.push_back(ringStartIndex + i);
            }
        }
    }
};
