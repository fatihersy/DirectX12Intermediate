#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wincodec.h>

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <vector>
#include <wrl.h>
#include <shellapi.h>
#include <map>
#include <format>
#include <array>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <functional>
#include <variant>
#include <type_traits>
#include <stdexcept>
#include <any>
#include <unordered_map>
#include <optional>
#include <bitset>
#include <typeinfo>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

constexpr std::wstring_view PROJECT_NAME = L"DXTerrain";

#include "StepTimer.h"
#include "Tools.h"
//#include "SceneTypes.h"
#include "AllocatorTypes.h"
//#include "MeshTypes.h"
#include "RendererTypes.h"
