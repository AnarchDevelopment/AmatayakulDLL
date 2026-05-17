#pragma once
#include <d3d11.h>
#include "../../ImGui/imgui.h"

class Shaders {
public:
    static void SetDevice(ID3D11Device* device);
    static void NewFrame();
    static void GausenBlur(ImDrawList* drawList, ImVec4 color);
    static void ClearBlur();
    static void DeviceReset();

private:
    static ID3D11Device* m_device;
    static ID3D11DeviceContext* m_context;
    static ID3D11ShaderResourceView* m_backbufferSRV;
    static bool m_initialized;
};
