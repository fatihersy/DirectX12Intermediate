#pragma once

namespace NSUnits
{
    constexpr float kMetersPerTexel = 1.0f;
    constexpr float kWorldUnitsPerMeter = 1.0f;

    struct Meters
    {
        float value{};

        constexpr Meters() = default;
        constexpr explicit Meters(float meters) : value(meters) {}

        constexpr float ToWorld() const { return value * kWorldUnitsPerMeter; }
    };
}
