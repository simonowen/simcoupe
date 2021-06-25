// Part of SimCoupe - A SAM Coupe emulator
//
// D3D11.cpp: Direct3D 11 display
//
//  Copyright (c) 2012-2020 Simon Owen
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

#include "SimCoupe.h"
#include "D3D11.h"
#include "D3D11_Aspect_VS.h"
#include "D3D11_Sample_PS.h"
#include "D3D11_Copy_VS.h"
#include "D3D11_Palette_PS.h"
#include "D3D11_Blend_PS.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

Direct3D11Video::Direct3D11Video(HWND hwnd) :
    m_hwnd(hwnd)
{
}

Rect Direct3D11Video::DisplayRect() const
{
    return m_rDisplay;
}

void Direct3D11Video::OptionsChanged()
{
    auto blend_factor = GetOption(motionblur) ? (GetOption(blurpercent) / 100.0f) : 0.0f;
    m_ps_constants.blend_factor = blend_factor;
    UpdateBuffer(m_pPSConstants.Get(), m_ps_constants);

    UpdatePalette();

    SetRectEmpty(&m_rSource);
    SetRectEmpty(&m_rTarget);
}

void Direct3D11Video::Update(const FrameBuffer& screen)
{
    if (SUCCEEDED(DrawChanges(screen)))
        Render();
}

void Direct3D11Video::ResizeWindow(int height) const
{
    if (GetOption(fullscreen) || IsMaximized(m_hwnd) || IsMinimized(m_hwnd))
        return;

    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    auto width = static_cast<int>(std::round(height * Frame::Width() * aspect_ratio / Frame::Height()));

    RECT rect = { 0, 0, width, height };
    AdjustWindowRectEx(&rect, GetWindowStyle(m_hwnd), TRUE, GetWindowExStyle(m_hwnd));

    SetWindowPos(m_hwnd, HWND_NOTOPMOST,
        0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_SHOWWINDOW | SWP_NOMOVE);
}

std::pair<int, int> Direct3D11Video::MouseRelative()
{
    POINT ptMouse{};
    GetCursorPos(&ptMouse);
    ScreenToClient(m_hwnd, &ptMouse);

    POINT ptCentre{ m_rDisplay.x + m_rDisplay.w / 2, m_rDisplay.y + m_rDisplay.h / 2 };
    auto dx = ptMouse.x - ptCentre.x;
    auto dy = ptMouse.y - ptCentre.y;

    auto pix_x = static_cast<float>(m_rDisplay.w) / Frame::Width() * 2;
    auto pix_y = static_cast<float>(m_rDisplay.h) / Frame::Height() * 2;

    auto dx_sam = static_cast<int>(dx / pix_x);
    auto dy_sam = static_cast<int>(dy / pix_y);

    if (dx_sam || dy_sam)
    {
        ptCentre.x += static_cast<int>(std::fmod(dx, pix_x));;
        ptCentre.y += static_cast<int>(std::fmod(dy, pix_y));;
        ClientToScreen(m_hwnd, &ptCentre);
        SetCursorPos(ptCentre.x, ptCentre.y);
    }

    return { dx_sam, dy_sam };
}

///////////////////////////////////////////////////////////////////////////////

static bool Fail(HRESULT hr, const std::string& operation)
{
    if (FAILED(hr))
    {
        Message(MsgType::Fatal, "{} failed with {}", operation.c_str(), hr);
#ifdef _DEBUG
        __debugbreak();
#endif
    }

    return false;
}

bool Direct3D11Video::Init()
{
    D3D_FEATURE_LEVEL featureLevel{};
    std::vector<D3D_FEATURE_LEVEL> featureLevels
    {
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    DWORD createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    auto hr =
        D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            &m_device,
            &featureLevel,
            &m_d3dContext);
    if (FAILED(hr))
        return Fail(hr, "D3D11CreateDevice");

    ComPtr<IDXGIDevice2> pDXGIDevice;
    if (FAILED(hr = m_device.As(&pDXGIDevice)))
        return Fail(hr, "QueryInterface(IDXGIDevice2)");

    ComPtr<IDXGIAdapter> pDXGIAdapter;
    if (FAILED(hr = pDXGIDevice->GetAdapter(&pDXGIAdapter)))
        return Fail(hr, "pDXGIDevice->GetAdapter");

    ComPtr<IDXGIFactory2> pDXGIFactory;
    if (FAILED(hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), &pDXGIFactory)))
        return Fail(hr, "pDXGIDevice->GetParent(IDXGIFactory2)");

    if (GetOption(tryvrr))
    {
        ComPtr<IDXGIFactory5> factory5;
        hr = pDXGIFactory.As(&factory5);
        if (SUCCEEDED(hr))
        {
            BOOL allowTearing = FALSE;

            hr = factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing,
                sizeof(allowTearing));

            m_allow_tearing = SUCCEEDED(hr) && allowTearing;
        }
    }

    RECT rClient;
    GetClientRect(m_hwnd, &rClient);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = rClient.right;
    swapChainDesc.Height = rClient.bottom;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = IsWindows8OrGreater() ? 3 : 1;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect =
        IsWindows10OrGreater() ? DXGI_SWAP_EFFECT_FLIP_DISCARD :
        IsWindows8OrGreater() ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL :
        DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    if (m_allow_tearing && swapChainDesc.SwapEffect >= DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL)
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    else
        m_allow_tearing = false;

    hr = pDXGIFactory->CreateSwapChainForHwnd(
        m_device.Get(), m_hwnd, &swapChainDesc, nullptr, nullptr, &m_swapChain);
    if (FAILED(hr))
        return Fail(hr, "CreateSwapChainForHwnd");

    hr = pDXGIFactory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (FAILED(hr = m_device->CreateVertexShader(g_D3D11_Aspect_VS, sizeof(g_D3D11_Aspect_VS), NULL, &m_aspectVS)))
        return Fail(hr, "CreateVertexShader");

    if (FAILED(hr = m_device->CreatePixelShader(g_D3D11_Sample_PS, sizeof(g_D3D11_Sample_PS), NULL, &m_samplePS)))
        return Fail(hr, "CreatePixelShader");

    if (FAILED(hr = m_device->CreateVertexShader(g_D3D11_Copy_VS, sizeof(g_D3D11_Copy_VS), NULL, &m_copyVS)))
        return Fail(hr, "CreateVertexShader (copy)");

    if (FAILED(hr = m_device->CreatePixelShader(g_D3D11_Palette_PS, sizeof(g_D3D11_Palette_PS), NULL, &m_palettePS)))
        return Fail(hr, "CreatePixelShader (palette)");

    if (FAILED(hr = m_device->CreatePixelShader(g_D3D11_Blend_PS, sizeof(g_D3D11_Blend_PS), NULL, &m_blendPS)))
        return Fail(hr, "CreatePixelShader (blend)");

    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    D3D11_BUFFER_DESC scd{};
    scd.Usage = D3D11_USAGE_DYNAMIC;
    scd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    scd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    scd.ByteWidth = sizeof(VSConstants);
    if (FAILED(hr = m_device->CreateBuffer(&scd, nullptr, &m_pVSConstants)))
        return Fail(hr, "CreateBuffer VS consts");
    m_d3dContext->VSSetConstantBuffers(0, 1, m_pVSConstants.GetAddressOf());

    scd.ByteWidth = sizeof(PSConstants);
    if (FAILED(hr = m_device->CreateBuffer(&scd, nullptr, &m_pPSConstants)))
        return Fail(hr, "CreateBuffer PS consts");
    m_d3dContext->PSSetConstantBuffers(0, 1, m_pPSConstants.GetAddressOf());

    CD3D11_SAMPLER_DESC sd{ D3D11_DEFAULT };
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (FAILED(hr = m_device->CreateSamplerState(&sd, m_linearSS.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSS (linear)");
    m_d3dContext->PSSetSamplers(0, 1, m_linearSS.GetAddressOf());

    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (FAILED(hr = m_device->CreateSamplerState(&sd, m_pointSS.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSS (point)");
    m_d3dContext->PSSetSamplers(1, 1, m_pointSS.GetAddressOf());

    CD3D11_RASTERIZER_DESC rd{ D3D11_DEFAULT };
    if (FAILED(m_device->CreateRasterizerState(&rd, &m_defaultRS)))
        return Fail(hr, "CreateRS");
    m_d3dContext->RSSetState(m_defaultRS.Get());

    OptionsChanged();

    return true;
}

HRESULT Direct3D11Video::ResizeSource(int width, int height)
{
    HRESULT hr = S_OK;

    if (!m_d3dContext)
        return S_FALSE;

    CD3D11_TEXTURE2D_DESC screen_td(DXGI_FORMAT_R8_UNORM, width, height);
    screen_td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    screen_td.Usage = D3D11_USAGE_DYNAMIC;
    screen_td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    screen_td.MipLevels = 1;
    if (FAILED(hr = m_device->CreateTexture2D(&screen_td, NULL, m_screenTex.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateTexture2D (screen)");

    D3D11_SHADER_RESOURCE_VIEW_DESC screen_srvd{};
    screen_srvd.Format = screen_td.Format;
    screen_srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    screen_srvd.Texture2D.MipLevels = 1;
    if (FAILED(hr = m_device->CreateShaderResourceView(m_screenTex.Get(), &screen_srvd, m_palettisedSRV.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSRV (screen)");

    m_rSource.right = width;
    m_rSource.bottom = height;
    return hr;
}

HRESULT Direct3D11Video::ResizeTarget(int target_width, int target_height)
{
    if (!m_d3dContext)
        return S_FALSE;

    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    auto width = static_cast<int>(std::round(Frame::Width() * aspect_ratio));
    auto height = Frame::Height();

    int width_fit = width * target_height / height;
    int height_fit = height * target_width / width;

    if (width_fit <= target_width)
    {
        width = width_fit;
        height = target_height;
    }
    else if (height_fit <= target_height)
    {
        width = target_width;
        height = height_fit;
    }

    m_rDisplay.x = (target_width - width) / 2;
    m_rDisplay.y = (target_height - height) / 2;
    m_rDisplay.w = width;
    m_rDisplay.h = height;

    m_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_swapChainRTV.Reset();
    m_d3dContext->Flush();

    UINT swapChainFlags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    auto hr = m_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, swapChainFlags);
    if (FAILED(hr))
        return hr;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    hr = m_swapChain->GetDesc1(&swapChainDesc);

    ComPtr<ID3D11Resource> pBackBufferPtr;
    if (FAILED(hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Resource), &pBackBufferPtr)))
        return Fail(hr, "GetBuffer (swapchain)");

    D3D11_RENDER_TARGET_VIEW_DESC rtvd{};
    rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = m_device->CreateRenderTargetView(pBackBufferPtr.Get(), &rtvd, &m_swapChainRTV);
    if (FAILED(hr))
        return Fail(hr, "CreateRTV (back)");
    pBackBufferPtr.Reset();

    GetClientRect(m_hwnd, &m_rTarget);

    m_vs_constants.scale_target_x = static_cast<float>(m_rDisplay.w) / m_rTarget.right;
    m_vs_constants.scale_target_y = static_cast<float>(m_rDisplay.h) / m_rTarget.bottom;
    UpdateBuffer(m_pVSConstants.Get(), m_vs_constants);

    return S_OK;
}

HRESULT Direct3D11Video::ResizeIntermediate(bool smooth)
{
    HRESULT hr = S_OK;

    int width_scale = (m_rTarget.right + (m_rSource.right - 1)) / m_rSource.right;
    int height_scale = (m_rTarget.bottom + (m_rSource.bottom - 1)) / m_rSource.bottom;

    if (smooth)
    {
        width_scale = 1;
        height_scale = 2;
    }

    int width = m_rSource.right * width_scale;
    int height = m_rSource.bottom * height_scale;

    CD3D11_TEXTURE2D_DESC td(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height);
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    if (FAILED(hr = m_device->CreateTexture2D(&td, NULL, m_scaledTex.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateTexture2D (scaled)");

    if (FAILED(hr = m_device->CreateTexture2D(&td, NULL, m_outputTex.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateTexture2D (output)");

    if (FAILED(hr = m_device->CreateTexture2D(&td, NULL, m_prevOutputTex.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateTexture2D (prev output)");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    if (FAILED(hr = m_device->CreateShaderResourceView(m_scaledTex.Get(), &srvd, m_scaledSRV.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSRV (scaled)");

    if (FAILED(hr = m_device->CreateRenderTargetView(m_scaledTex.Get(), NULL, &m_scaledRTV)))
        return Fail(hr, "CreateRTV (scaled)");

    if (FAILED(hr = m_device->CreateShaderResourceView(m_outputTex.Get(), &srvd, m_outputSRV.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSRV (output)");

    if (FAILED(hr = m_device->CreateRenderTargetView(m_outputTex.Get(), nullptr, &m_outputRTV)))
        return Fail(hr, "CreateRTC (output)");

    if (FAILED(hr = m_device->CreateShaderResourceView(m_prevOutputTex.Get(), &srvd, m_prevOutputSRV.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateSRV (prev output)");

    if (FAILED(hr = m_device->CreateRenderTargetView(m_prevOutputTex.Get(), nullptr, &m_prevOutputRTV)))
        return Fail(hr, "CreateRTV (prev output)");

    SetRect(&m_rIntermediate, 0, 0, width, height);
    m_smooth = smooth;

    return S_OK;
}

HRESULT Direct3D11Video::UpdatePalette()
{
    HRESULT hr = S_OK;

    auto sam_palette = IO::Palette();
    std::vector<uint32_t> d3d_palette;

    std::transform(
        sam_palette.begin(), sam_palette.end(), std::back_inserter(d3d_palette),
        [&](COLOUR& c) {
            return RGB2Native(
                c.red, c.green, c.blue, 0x000000ff, 0x0000ff00, 0x00ff0000) | 0xff000000; 
        });

    CD3D11_TEXTURE1D_DESC td(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, static_cast<UINT>(d3d_palette.size()));
    td.MipLevels = 1;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = d3d_palette.data();
    if (FAILED(hr = m_device->CreateTexture1D(&td, &init_data, m_paletteTex.ReleaseAndGetAddressOf())))
        return Fail(hr, "CreateTexture1D (palette)");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    srvd.Texture1D.MipLevels = 1;
    if (FAILED(hr = m_device->CreateShaderResourceView(m_paletteTex.Get(), &srvd, &m_paletteSRV)))
        return Fail(hr, "CreateSRV (palette)");

    m_d3dContext->PSSetShaderResources(0, 1, m_paletteSRV.GetAddressOf());

    return S_OK;
}

HRESULT Direct3D11Video::DrawChanges(const FrameBuffer& screen)
{
    HRESULT hr = S_OK;

    int width = screen.Width();
    int height = screen.Height();

    RECT rClient = m_rTarget;
    if (!IsMinimized(m_hwnd))
    {
        GetClientRect(m_hwnd, &rClient);
    }

    bool smooth = !GUI::IsActive() && GetOption(smooth);
    bool source_changed = (width != m_rSource.right) || (height != m_rSource.bottom);
    bool target_changed = rClient != m_rTarget;
    bool smooth_changed = smooth != m_smooth;

    if (source_changed)
        ResizeSource(width, height);

    if (source_changed || target_changed)
        ResizeTarget(rClient.right, rClient.bottom);

    if (source_changed || target_changed || smooth_changed)
        ResizeIntermediate(smooth);

    if (!m_screenTex)
        return S_FALSE;

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(hr = m_d3dContext->Map(m_screenTex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
        return hr;

    auto src = screen.GetLine(0);
    auto dst = reinterpret_cast<uint8_t*>(ms.pData);
    for (int y = 0; y < screen.Height(); ++y)
    {
        memcpy(dst, src, width);
        src += screen.Width();
        dst += ms.RowPitch;
    }

    m_d3dContext->Unmap(m_screenTex.Get(), 0);

    return S_OK;
}

HRESULT Direct3D11Video::Render()
{
    if (!m_d3dContext)
        return S_FALSE;

    auto viewport = CD3D11_VIEWPORT(0.0f, 0.0f, 0.0f, 0.0f);
    viewport.Width = static_cast<float>(m_rIntermediate.right);
    viewport.Height = static_cast<float>(m_rIntermediate.bottom);
    m_d3dContext->RSSetViewports(1, &viewport);

    // Convert palettised data to RGB with any integer scaling.
    ID3D11ShaderResourceView* null_srvs[]{ nullptr, nullptr };
    m_d3dContext->PSSetShaderResources(2, 2, null_srvs);
    m_d3dContext->OMSetRenderTargets(1, m_scaledRTV.GetAddressOf(), nullptr);
    m_d3dContext->PSSetShaderResources(1, 1, m_palettisedSRV.GetAddressOf());
    m_d3dContext->VSSetShader(m_copyVS.Get(), nullptr, 0);
    m_d3dContext->PSSetShader(m_palettePS.Get(), nullptr, 0);
    m_d3dContext->Draw(4, 0);

    // Blend max components from new frame and faded version of previous render.
    m_d3dContext->OMSetRenderTargets(1, m_outputRTV.GetAddressOf(), nullptr);
    ID3D11ShaderResourceView* texture_srvs[]{ m_scaledSRV.Get(), m_prevOutputSRV.Get() };
    m_d3dContext->PSSetShaderResources(2, _countof(texture_srvs), texture_srvs);
    m_d3dContext->PSSetShader(m_blendPS.Get(), nullptr, 0);
    m_d3dContext->Draw(4, 0);

    viewport.Width = static_cast<float>(m_rTarget.right);
    viewport.Height = static_cast<float>(m_rTarget.bottom);
    m_d3dContext->RSSetViewports(1, &viewport);

    auto fill_intensity = GetOption(blackborder) ? 0.0f : 0.01f;
    FLOAT fill_colour[]{ fill_intensity, fill_intensity, fill_intensity, 1.0f };

    // Finally, render to the aspect correct area in the back buffer.
    m_d3dContext->OMSetRenderTargets(1, m_swapChainRTV.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(m_swapChainRTV.Get(), fill_colour);
    m_d3dContext->PSSetShaderResources(2, 1, m_outputSRV.GetAddressOf());
    m_d3dContext->VSSetShader(m_aspectVS.Get(), nullptr, 0);
    m_d3dContext->PSSetShader(m_samplePS.Get(), nullptr, 0);
    m_d3dContext->Draw(4, 0);

    auto hr = m_swapChain->Present(0, m_allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        Init();
    }
    else if (FAILED(hr))
    {
        return ResizeTarget(m_rTarget.right, m_rTarget.bottom);
    }

    std::swap(m_outputTex, m_prevOutputTex);
    std::swap(m_outputSRV, m_prevOutputSRV);
    std::swap(m_outputRTV, m_prevOutputRTV);

    return S_OK;
}
