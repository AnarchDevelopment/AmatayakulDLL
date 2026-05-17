#include "Shaders.hpp"
#include <d3d11.h>

ID3D11Device* Shaders::m_device = nullptr;
ID3D11DeviceContext* Shaders::m_context = nullptr;
ID3D11ShaderResourceView* Shaders::m_backbufferSRV = nullptr;
bool Shaders::m_initialized = false;

void Shaders::SetDevice(ID3D11Device* device) {
    m_device = device;
    if (m_device) {
        m_device->GetImmediateContext(&m_context);
    }
}

void Shaders::NewFrame() {
    if (!m_device || !m_context) return;
    
    if (m_backbufferSRV) {
        m_backbufferSRV->Release();
        m_backbufferSRV = nullptr;
    }
    
    ID3D11RenderTargetView* rtv = nullptr;
    m_context->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) return;
    
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    rtv->Release();
    if (!res) return;
    
    m_device->CreateShaderResourceView(res, nullptr, &m_backbufferSRV);
    res->Release();
}

void Shaders::GausenBlur(ImDrawList* drawList, ImVec4 color) {
    if (!m_backbufferSRV || !drawList) return;
    
    ImVec2 size = ImGui::GetIO().DisplaySize;
    
    // Draw background color overlay
    drawList->AddRectFilled(ImVec2(0, 0), size, ImGui::GetColorU32(color));
    
    // Simulated Gaussian Blur (8-tap offset draw)
    // This creates a soft focus effect by layering the backbuffer with slight offsets.
    float blurRadius = 4.0f;
    ImU32 blurCol = IM_COL32(255, 255, 255, 35); 
    
    for (float i = -blurRadius; i <= blurRadius; i += 2.0f) {
        for (float j = -blurRadius; j <= blurRadius; j += 2.0f) {
            drawList->AddImage((ImTextureID)m_backbufferSRV, ImVec2(i, j), ImVec2(size.x + i, size.y + j), ImVec2(0,0), ImVec2(1,1), blurCol);
        }
    }
}

void Shaders::ClearBlur() {
    if (m_backbufferSRV) {
        m_backbufferSRV->Release();
        m_backbufferSRV = nullptr;
    }
}

void Shaders::DeviceReset() {
    ClearBlur();
    if (m_context) {
        m_context->Release();
        m_context = nullptr;
    }
    m_device = nullptr;
}
