#pragma once

#include "raylib.h"
#include <algorithm>
#include <cmath>
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
                float wholeStep = std::floor(accum);
                target = isIncreasing ? std::min<T>(target + static_cast<T>(wholeStep), limit)
                                      : std::max<T>(target - static_cast<T>(wholeStep), limit);
                accum -= wholeStep;
            }
        } else {
            target = isIncreasing ? std::min<T>(target + static_cast<T>(step), limit)
                                  : std::max<T>(target - static_cast<T>(step), limit);
        }
    }

  public:
    // Primary Signature: Handles exactly 2 keys per action.
    void Update(const int keyIncPrimary, const int keyIncSecondary, const int keyDecPrimary,
                const int keyDecSecondary, T &target, T tapStep, const float holdRate, T minVal,
                T maxVal) {
        const float dt = GetFrameTime();

        // Evaluate state locally. If secondary is 0 (KEY_NULL), it safely evaluates to false.
        const bool incDown    = IsKeyDown(keyIncPrimary) || IsKeyDown(keyIncSecondary);
        const bool incPressed = IsKeyPressed(keyIncPrimary) || IsKeyPressed(keyIncSecondary);

        const bool decDown    = IsKeyDown(keyDecPrimary) || IsKeyDown(keyDecSecondary);
        const bool decPressed = IsKeyPressed(keyDecPrimary) || IsKeyPressed(keyDecSecondary);

        if (incDown) {
            // Priority to fresh key presses to prevent timer bleed when switching keys
            if (incPressed) {
                target = std::min<T>(target + tapStep, maxVal);
                timer  = 0.0f;
                accum  = 0.0f;
            } else {
                timer += dt;
                if (timer > holdDelay) {
                    ApplyHold(target, holdRate * dt, maxVal, true);
                }
            }
        } else if (decDown) {
            if (decPressed) {
                target = std::max<T>(target - tapStep, minVal);
                timer  = 0.0f;
                accum  = 0.0f;
            } else {
                timer += dt;
                if (timer > holdDelay) {
                    ApplyHold(target, holdRate * dt, minVal, false);
                }
            }
        } else {
            timer = 0.0f;
            accum = 0.0f;
        }
    }

    void Update(const int keyInc, const int keyDec, T &target, T tapStep, const float holdRate,
                T minVal, T maxVal) {
        Update(keyInc, 0, keyDec, 0, target, tapStep, holdRate, minVal, maxVal);
    }
};