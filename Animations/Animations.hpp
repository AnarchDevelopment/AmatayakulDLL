#pragma once

class Animations {
public:
    // === Easing Functions ===
    // Cubic easing (smooth in/out)
    static float SmoothInertia(float t);
    
    // Quadratic easing (in/out)
    static float EaseInOutQuad(float t);
    
    // Exponential easing (out)
    static float EaseOutExpo(float t);
    
    // Elastic bounce effect (in/out)
    static float EaseInOutElastic(float t);
    
    // Linear (no easing)
    static float Linear(float t);
    
    // === Animation Utilities ===
    // Linear Interpolation
    static float Lerp(float a, float b, float t);
    static struct ImVec4 Lerp(const struct ImVec4& a, const struct ImVec4& b, float t);

    // Calculate progress from time with duration
    static float GetProgress(float elapsed, float duration);
    
    // Clamp value between 0 and 1
    static float Clamp01(float value);
};
