#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <wincodec.h>

#include <directx/d3dx12.h>
#include <directx/d3d12shader.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <DirectXMath.h>
#include <libassert/assert.hpp>

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
#include <cstddef>
#include <cstring>
#include <fstream>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

template<typename T>
using OptRef = std::optional<std::reference_wrapper<T>>;

constexpr std::wstring_view PROJECT_NAME = L"DXTerrain";

constexpr std::wstring_view SHADERS_FOLDER = L"shaders";

enum EArguments {
    ARG_CONSOLE_PID,
    ARG_WAIT_FOR_DEBUGGER,
    ARG_MAX
};

struct SCmdArg
{
    std::vector<std::string> aliases;
    std::string value;
    bool present{};
    bool expectsValue{};
};

inline std::array<SCmdArg, static_cast<size_t>(ARG_MAX)> g_CmdArguments =
{
    SCmdArg
    {
        .aliases = {"--console-pid="},
        .expectsValue = true
    },
    SCmdArg
    {
        .aliases = {"--debug", "-d"},
        .expectsValue = false
    }
};

template<size_t Count, class Fn, class... Args>
decltype(auto) IndexSequence(Fn&& fn, Args&&... args)
{
    return [&]<size_t... N>(std::index_sequence<N...>) -> decltype(auto)
    {
        return std::forward<Fn>(fn)(
            std::index_sequence<N...>{},
            std::forward<Args>(args)...
        );
    }(std::make_index_sequence<Count>{});
};

template<size_t Count, class Fn>
auto ArraySequence(Fn&& fn)
{
    return [&]<size_t... N>(std::index_sequence<N...>)
    {
        return std::array{
            std::forward<Fn>(fn)(std::integral_constant<size_t, N>{})...
        };
    }(std::make_index_sequence<Count>{});
};

#include "core/Math.h"
#include "StepTimer.h"
#include "Logger.h"
#include "Tools.h"
#include "ModelTypes.h"
#include "AllocatorTypes.h"
#include "RendererTypes.h"
#include "Texture.h"
#include "TerrainTypes.h"
#include "SceneTypes.h"
