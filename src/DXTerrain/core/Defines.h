#pragma once

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

template<typename T>
using MemberRef = std::optional<std::reference_wrapper<T>>;

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

template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

#define DE_REF(PTR) (*PTR)

template<typename T>
using undertype = std::underlying_type_t<T>;

template<typename TEnum>
concept IsEnumClass = not std::is_convertible_v<TEnum, std::underlying_type_t<TEnum>>;

template<typename TEnum> requires std::is_enum_v<TEnum> and requires { TEnum::Force32Bit; } and IsEnumClass<TEnum>
struct Flag
{
    using T = std::underlying_type_t<TEnum>;

    constexpr Flag() = default;
    constexpr Flag(TEnum e) : bits(static_cast<T>(e)) {}

    constexpr Flag(T raw) : bits(raw) {}

    template<typename TOperand>
    static constexpr T toRaw(TOperand v)
    {
        if constexpr (std::same_as<TOperand, Flag<TEnum>>) return v.bits;
        else if constexpr (std::same_as<TOperand, TEnum>) return static_cast<T>(v);
        else if constexpr (std::same_as<TOperand, T>) return v;
        else static_assert(false, "Invalid type");
    }

    template<typename TLHS, typename TRHS>
    friend constexpr Flag operator| (TLHS lhs, TRHS rhs) {
        return Flag(toRaw(lhs) | toRaw(rhs));
    }
    template<typename TLHS, typename TRHS>
    friend constexpr Flag operator& (TLHS lhs, TRHS rhs) {
        return Flag(toRaw(lhs) & toRaw(rhs));
    }
    template<typename TLHS, typename TRHS>
    friend constexpr Flag operator^ (TLHS lhs, TRHS rhs) {
        return Flag(toRaw(lhs) ^ toRaw(rhs));
    }
    template<typename TVAL>
    friend constexpr Flag operator~(TVAL val) {
        return Flag(~toRaw(val));
    }

    template<typename TVAL>
    constexpr Flag&  operator|=(TVAL val) {
        bits |= toRaw(val);
        return *this;
    }
    template<typename TVAL>
    constexpr Flag& operator&=(TVAL val) {
        bits &= toRaw(val);
        return *this;
    }
    template<typename TVAL>
    constexpr Flag& operator^=(TVAL val) {
        bits ^= toRaw(val);
        return *this;
    }

    template<typename TVAL>
    constexpr bool HasLeastOne(TVAL e) const {
        return (bits & toRaw(e)) != 0;
    }
    template<typename TVAL>
    constexpr bool HasLeastAll(TVAL e) const {
        return (bits & toRaw(e)) == toRaw(e);
    }
    template<typename TVAL>
    constexpr bool HasExact(TVAL e) const {
        return bits == toRaw(e);
    }
    constexpr bool Empty() const {
        return bits == 0;
    }

    template<typename TVAL>
    constexpr Flag& Set(TVAL e) {
        bits |= toRaw(e);
        return DE_REF(this);
    }
    template<typename TVAL>
    constexpr Flag& UnSet(TVAL e) {
        bits &= ~toRaw(e);
        return DE_REF(this);
    }
    template<typename TVAL>
    constexpr Flag& Toggle(TVAL e) {
        bits ^= toRaw(e);
        return DE_REF(this);
    }

    constexpr explicit operator T() const { return bits; }

private:
    T bits{};
};

template<typename TEnum> requires std::is_enum_v<TEnum>
constexpr undertype<TEnum> operator| (TEnum lhs, TEnum rhs) {
    return undertype<TEnum>(lhs) | undertype<TEnum>(rhs);
}
template<typename TEnum> requires std::is_enum_v<TEnum>
constexpr undertype<TEnum> operator& (TEnum lhs, TEnum rhs) {
    return Flag<TEnum>::toRaw(lhs) & Flag<TEnum>::toRaw(rhs);
}
template<typename TEnum> requires std::is_enum_v<TEnum>
constexpr undertype<TEnum> operator^ (TEnum lhs, TEnum rhs) {
    return Flag<TEnum>::toRaw(lhs) & Flag<TEnum>::toRaw(rhs);
}
template<typename TEnum> requires std::is_enum_v<TEnum>
constexpr undertype<TEnum> operator~(TEnum val) {
    return ~Flag<TEnum>::toRaw(val);
}

//#include "EntityTypes.h"
//#include "core/Math.h"
//#include "StepTimer.h"
//#include "Logger.h"
//#include "Tools.h"
//#include "AllocatorTypes.h"
//#include "RendererTypes.h"
//#include "ModelTypes.h"
//#include "Texture.h"
//#include "TerrainTypes.h"
//#include "SceneTypes.h"
