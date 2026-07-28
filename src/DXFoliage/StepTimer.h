//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>

// Helper class for animation and simulation timing. Originally built on
// Win32's QueryPerformanceCounter/LARGE_INTEGER directly; now built on
// std::chrono::steady_clock instead, which is monotonic and available
// identically on every platform this project targets. Same "ticks"
// concept (10,000,000 ticks/second) and public API as before — nothing
// that calls Tick()/GetElapsedSeconds()/etc. needs to change.
class StepTimer
{
public:
    StepTimer() :
        m_lastTime(Clock::now()),
        m_maxDeltaTicks(TicksPerSecond / 10),
        m_elapsedTicks(0),
        m_totalTicks(0),
        m_leftOverTicks(0),
        m_frameCount(0),
        m_framesPerSecond(0),
        m_framesThisSecond(0),
        m_secondCounter(0),
        m_isFixedTimeStep(false),
        m_targetElapsedTicks(TicksPerSecond / 60)
    {
    }

    // Get elapsed time since the previous Update call.
    uint64_t GetElapsedTicks() const { return m_elapsedTicks; }
    double GetElapsedSeconds() const { return TicksToSeconds(m_elapsedTicks); }

    // Get total time since the start of the program.
    uint64_t GetTotalTicks() const { return m_totalTicks; }
    double GetTotalSeconds() const { return TicksToSeconds(m_totalTicks); }

    // Get total number of updates since start of the program.
    uint32_t GetFrameCount() const { return m_frameCount; }

    // Get the current framerate.
    uint32_t GetFramesPerSecond() const { return m_framesPerSecond; }

    // Set whether to use fixed or variable timestep mode.
    void SetFixedTimeStep(bool isFixedTimestep) { m_isFixedTimeStep = isFixedTimestep; }

    // Set how often to call Update when in fixed timestep mode.
    void SetTargetElapsedTicks(uint64_t targetElapsed) { m_targetElapsedTicks = targetElapsed; }
    void SetTargetElapsedSeconds(double targetElapsed) { m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

    // Integer format represents time using 10,000,000 ticks per second.
    static constexpr uint64_t TicksPerSecond = 10000000;

    static double TicksToSeconds(uint64_t ticks) { return static_cast<double>(ticks) / TicksPerSecond; }
    static uint64_t SecondsToTicks(double seconds) { return static_cast<uint64_t>(seconds * TicksPerSecond); }

    // After an intentional timing discontinuity (for instance a blocking IO operation)
    // call this to avoid having the fixed timestep logic attempt a set of catch-up
    // Update calls.
    void ResetElapsedTime()
    {
        m_lastTime = Clock::now();

        m_leftOverTicks = 0;
        m_framesPerSecond = 0;
        m_framesThisSecond = 0;
        m_secondCounter = 0;
    }

    using LPUPDATEFUNC = void(*)(void);

    // Update timer state, calling the specified Update function the appropriate number of times.
    void Tick(LPUPDATEFUNC update = nullptr)
    {
        const TimePoint currentTime = Clock::now();

        uint64_t timeDelta = static_cast<uint64_t>(std::chrono::duration_cast<Ticks>(currentTime - m_lastTime).count());

        m_lastTime = currentTime;
        m_secondCounter += timeDelta;

        // Clamp excessively large time deltas (e.g. after paused in the debugger).
        if (timeDelta > m_maxDeltaTicks)
        {
            timeDelta = m_maxDeltaTicks;
        }

        uint32_t lastFrameCount = m_frameCount;

        if (m_isFixedTimeStep)
        {
            // Fixed timestep update logic.

            // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
            // the clock to exactly match the target value. This prevents tiny and irrelevant errors
            // from accumulating over time.
            if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))) < TicksPerSecond / 4000)
            {
                timeDelta = m_targetElapsedTicks;
            }

            m_leftOverTicks += timeDelta;

            while (m_leftOverTicks >= m_targetElapsedTicks)
            {
                m_elapsedTicks = m_targetElapsedTicks;
                m_totalTicks += m_targetElapsedTicks;
                m_leftOverTicks -= m_targetElapsedTicks;
                m_frameCount++;

                if (update)
                {
                    update();
                }
            }
        }
        else
        {
            // Variable timestep update logic.
            m_elapsedTicks = timeDelta;
            m_totalTicks += timeDelta;
            m_leftOverTicks = 0;
            m_frameCount++;

            if (update)
            {
                update();
            }
        }

        // Track the current framerate.
        if (m_frameCount != lastFrameCount)
        {
            m_framesThisSecond++;
        }

        if (m_secondCounter >= TicksPerSecond)
        {
            m_framesPerSecond = m_framesThisSecond;
            m_framesThisSecond = 0;
            m_secondCounter %= TicksPerSecond;
        }
    }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    // A duration representing one "tick" (1/10,000,000 second), matching
    // the ticks-per-second convention above.
    using Ticks = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;

    TimePoint m_lastTime;
    uint64_t m_maxDeltaTicks;

    // Derived timing data uses a canonical tick format.
    uint64_t m_elapsedTicks;
    uint64_t m_totalTicks;
    uint64_t m_leftOverTicks;

    // Members for tracking the framerate.
    uint32_t m_frameCount;
    uint32_t m_framesPerSecond;
    uint32_t m_framesThisSecond;
    uint64_t m_secondCounter;

    // Members for configuring fixed timestep mode.
    bool m_isFixedTimeStep;
    uint64_t m_targetElapsedTicks;
};
