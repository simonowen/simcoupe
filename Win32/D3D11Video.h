// Part of SimCoupe - A SAM Coupe emulator
//
// D3D11.h: Direct3D 11 display
//
//  Copyright (c) 2012-2015 Simon Owen
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

#include <dxgi1_5.h>
#include <d3d11.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#include <DirectXMath.h>

#include "Video.h"

class Direct3D11Video final : public IVideoBase
{
public:
    Direct3D11Video(HWND hwnd);

    bool Init() override;
    Rect DisplayRect() const override;
    void ResizeWindow(int height) const override;
    std::pair<int, int> MouseRelative() override;

    void OptionsChanged() override;
    void Update(const FrameBuffer& screen) override;

protected:
    template <typename T>
    HRESULT UpdateBuffer(ID3D11Buffer* pBuffer, const T& data)
    {
        HRESULT hr;
        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(hr = m_d3dContext->Map(pBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms)))
        {
            memcpy(ms.pData, &data, sizeof(data));
            m_d3dContext->Unmap(pBuffer, 0);
        }
        return hr;
    }

    HRESULT InitD3D11();
    HRESULT ResizeSource(int width, int height);
    HRESULT ResizeTarget(int width, int height);
    HRESULT ResizeIntermediate(bool smooth);
    HRESULT UpdatePalette();
    HRESULT DrawChanges(const FrameBuffer& screen);
    HRESULT Render();

private:
    HWND m_hwnd{};

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_d3dContext;
    ComPtr<IDXGISwapChain1> m_swapChain;

    ComPtr<ID3D11Texture1D> m_paletteTex;
    ComPtr<ID3D11Texture2D> m_screenTex;
    ComPtr<ID3D11Texture2D> m_scaledTex;
    ComPtr<ID3D11Texture2D> m_outputTex;
    ComPtr<ID3D11Texture2D> m_prevOutputTex;

    ComPtr<ID3D11RenderTargetView> m_swapChainRTV;
    ComPtr<ID3D11RenderTargetView> m_scaledRTV;
    ComPtr<ID3D11RenderTargetView> m_outputRTV;
    ComPtr<ID3D11RenderTargetView> m_prevOutputRTV;

    ComPtr<ID3D11ShaderResourceView> m_paletteSRV;
    ComPtr<ID3D11ShaderResourceView> m_palettisedSRV;
    ComPtr<ID3D11ShaderResourceView> m_scaledSRV;
    ComPtr<ID3D11ShaderResourceView> m_outputSRV;
    ComPtr<ID3D11ShaderResourceView> m_prevOutputSRV;

    ComPtr<ID3D11VertexShader> m_aspectVS;
    ComPtr<ID3D11VertexShader> m_copyVS;
    ComPtr<ID3D11PixelShader> m_samplePS;
    ComPtr<ID3D11PixelShader> m_palettePS;
    ComPtr<ID3D11PixelShader> m_blendPS;

    ComPtr<ID3D11Buffer> m_pVSConstants;
    ComPtr<ID3D11Buffer> m_pPSConstants;

    ComPtr<ID3D11RasterizerState> m_defaultRS;
    ComPtr<ID3D11SamplerState> m_linearSS;
    ComPtr<ID3D11SamplerState> m_pointSS;

    struct VSConstants
    {
        float scale_target_x{ 1.0f };
        float scale_target_y{ 1.0f };
        float padding[2]{};
    };

    struct PSConstants
    {
        float blend_factor{ 0.0f };
        float padding[3]{};
    };

    VSConstants m_vs_constants;
    PSConstants m_ps_constants;

    bool m_allow_tearing{ false };
    bool m_occluded{ false };

    RECT m_rSource{};
    RECT m_rTarget{};
    RECT m_rIntermediate{};

    Rect m_rDisplay{};
    bool m_smooth{};
};
