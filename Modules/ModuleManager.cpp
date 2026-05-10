#include "ModuleManager.hpp"
#include "ModuleHeader.hpp"

void Module::Initialize(uintptr_t gameBase, HudElement* renderInfoHud, HudElement* watermarkHud, HudElement* keystrokesHud, HudElement* cpsHud, HudElement* fpsHud) {
    ArrayList::Initialize();
    RenderInfo::Initialize(renderInfoHud);
    Watermark::Initialize(watermarkHud);
    Keystrokes::Initialize(keystrokesHud);
    CPSCounter::Initialize(cpsHud);
    FPSCounter::Initialize(fpsHud);
    MotionBlur::Initialize();
    Info::Initialize();
    UnlockFPS::Initialize();
}

void Module::UpdateAnimation(unsigned long long now) {
    RenderInfo::UpdateAnimation(now);
    Keystrokes::UpdateAnimation(now);
    Watermark::UpdateAnimation(now);
    FPSCounter::UpdateAnimation(now);
    MotionBlur::UpdateAnimation(now);
}

void Module::RenderDisplay(float sw, float sh) {
    Watermark::RenderDisplay();
    Keystrokes::RenderDisplay(sw, sh);
    RenderInfo::RenderWindow();
    CPSCounter::RenderDisplay(sw, sh);
    FPSCounter::RenderDisplay(sw, sh);
}

void Module::RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd) {
    UnlockFPS::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Watermark::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    RenderInfo::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Keystrokes::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    CPSCounter::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    FPSCounter::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    MotionBlur::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
}
