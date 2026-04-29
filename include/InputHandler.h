#pragma once

#include "raylib.h"
#include <algorithm>
#include <type_traits>

template <typename T> class ContinuousInput {
  private:
    float timer           = 0.0f;
    float accum           = 0.0f;
    const float holdDelay = 0.35f;

    void ApplyHold(T &target, float step, T limit, bool isIncreasing) {
        if constexpr (std::is_integral_v<T>) {
            accum += step;
            if (accum >= 1.0f) {
                float wholeStep = accum;
                target          = isIncreasing ? std::min<T>(target + wholeStep, limit)
                                               : std::max<T>(target - wholeStep, limit);
                accum -= wholeStep;
            }
        } else {
            target = isIncreasing ? std::min<T>(target + static_cast<T>(step), limit)
                                  : std::max<T>(target - static_cast<T>(step), limit);
        }
    }

  public:
    void Update(const int keyInc, const int keyDec, T &target, T tapStep, const float holdRate,
                T minVal, T maxVal) {
        const float dt = GetFrameTime();

        if (IsKeyDown(keyInc)) {
            timer += dt;
            if (IsKeyPressed(keyInc)) {
                target = std::min<T>(target + tapStep, maxVal);
            } else if (timer > holdDelay) {
                ApplyHold(target, holdRate * dt, maxVal, true);
            }
        } else if (IsKeyDown(keyDec)) {
            timer += dt;
            if (IsKeyPressed(keyDec)) {
                target = std::max<T>(target - tapStep, minVal);
            } else if (timer > holdDelay) {
                ApplyHold(target, holdRate * dt, minVal, false);
            }
        } else {
            timer = 0.0f;
            accum = 0.0f;
        }
    }
};