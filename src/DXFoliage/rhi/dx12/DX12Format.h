#pragma once

#include "PlatformHeaders_DX12.h"
#include "rhi/RHITypes.h"

// Shared NSRHI::EFormat -> DXGI_FORMAT mapping, used by every DX12 backend
// file that needs to translate a neutral format into a concrete one
// (textures, pipeline render-target/depth formats, and eventually vertex
// attribute formats).
namespace NSRHIDX12
{
    inline DXGI_FORMAT ToDXGIFormat(NSRHI::EFormat format)
    {
        switch (format)
        {
            case NSRHI::EFormat::R8G8B8A8_UNORM:      return DXGI_FORMAT_R8G8B8A8_UNORM;
            case NSRHI::EFormat::B8G8R8A8_UNORM:      return DXGI_FORMAT_B8G8R8A8_UNORM;
            case NSRHI::EFormat::R16G16B16A16_FLOAT:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case NSRHI::EFormat::R32G32_FLOAT:        return DXGI_FORMAT_R32G32_FLOAT;
            case NSRHI::EFormat::R32G32B32_FLOAT:     return DXGI_FORMAT_R32G32B32_FLOAT;
            case NSRHI::EFormat::R32G32B32A32_FLOAT:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case NSRHI::EFormat::D32_FLOAT:           return DXGI_FORMAT_D32_FLOAT;
            case NSRHI::EFormat::D24_UNORM_S8_UINT:   return DXGI_FORMAT_D24_UNORM_S8_UINT;
            default:                                  return DXGI_FORMAT_UNKNOWN;
        }
    }
}
