#include "ModuleManager.hpp"

void Module::Initialize(uintptr_t gameBase, HudElement* renderInfoHud, HudElement* watermarkHud, HudElement* keystrokesHud, HudElement* cpsHud) {
    Reach::Initialize(gameBase);
    Hitbox::Initialize(gameBase);
    Timer::Initialize(gameBase);
    FullBright::Initialize(gameBase);
    RenderInfo::Initialize(renderInfoHud);
    Watermark::Initialize(watermarkHud);
    Keystrokes::Initialize(keystrokesHud);
    CPSCounter::Initialize(cpsHud);
    Terminal::Initialize();
    Info::Initialize();
    UnlockFPS::Initialize();
}

void Module::UpdateAnimation(unsigned long long now) {
    RenderInfo::UpdateAnimation(now);
    MotionBlur::UpdateAnimation(now);
    Keystrokes::UpdateAnimation(now);
    Watermark::UpdateAnimation(now);
}

void Module::RenderDisplay(float sw, float sh) {
    Watermark::RenderDisplay();
    Keystrokes::RenderDisplay(sw, sh);
    RenderInfo::RenderWindow();
    CPSCounter::RenderDisplay(sw, sh);
}

void Module::RenderArrayList(ImDrawList* draw, ImVec2 arrayListStart, float& yPos, ImVec2& arrayListEnd) {
    Reach::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Hitbox::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    AutoSprint::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    FullBright::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Timer::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    UnlockFPS::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Watermark::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    RenderInfo::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    MotionBlur::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    Keystrokes::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
    CPSCounter::RenderArrayList(draw, arrayListStart, yPos, arrayListEnd);
}
