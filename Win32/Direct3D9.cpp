// Part of SimCoupe - A SAM Coupe emulator
//
// Direct3D.cpp: Direct3D 9 display
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

#include "SimCoupe.h"
#include <windows.h>
#include <windowsx.h>

#include "Direct3D9.h"
#include "D3D9_VS.h"
#include "D3D9_PS.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

#define TEXTURE_SIZE    1024
#define NUM_VERTICES    4

const D3DVERTEXELEMENT9 vertexDecl[] =
{
    {0,  0, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    D3DDECL_END()
};

struct CUSTOMVERTEX
{
    CUSTOMVERTEX(long x_, long y_) : x(static_cast<short>(x_)), y(static_cast<short>(y_)) { }
    short x, y;
};


// SAM palette in native surface values (faster if kept at file scope)
static uint32_t adwPalette[N_PALETTE_COLOURS];


Direct3D9Video::~Direct3D9Video()
{
    if (m_pTexture) m_pTexture->Release();
    if (m_pPixelShader) m_pPixelShader->Release();
    if (m_pVertexShader) m_pVertexShader->Release();
    if (m_pVertexBuffer) m_pVertexBuffer->Release();
    if (m_pVertexDecl) m_pVertexDecl->Release();
    if (m_pd3dDevice) m_pd3dDevice->Release();
    if (m_pd3d) m_pd3d->Release();
}


int Direct3D9Video::GetCaps() const
{
    return VCAP_STRETCH | VCAP_FILTER | VCAP_SCANHIRES;
}

bool Direct3D9Video::Init()
{
    m_pd3d = Direct3DCreate9(D3D_SDK_VERSION);
    CreateDevice();
    UpdateSize();

    return m_pd3d != nullptr;
}


void Direct3D9Video::UpdatePalette()
{
    const COLOUR* pSAM = IO::GetPalette();
    const uint32_t dwRmask = 0x00ff0000, dwGmask = 0x0000ff00, dwBmask = 0x000000ff, dwAmask = 0xff000000;

    // Build the palette from SAM colours
    for (int i = 0; i < N_PALETTE_COLOURS; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR* p = &pSAM[i];
        uint8_t r = p->bRed, g = p->bGreen, b = p->bBlue, a = 0xff;

        // Set native pixel value
        adwPalette[i] = RGB2Native(r, g, b, a, dwRmask, dwGmask, dwBmask, dwAmask);
    }

    // Redraw to reflect any changes
    Video::SetDirty();
}

// Update the display to show anything that's changed since last time
void Direct3D9Video::Update(CScreen* pScreen_, bool* pafDirty_)
{
    HRESULT hr;

    if (!m_pd3dDevice)
        return;

    if (FAILED(hr = m_pd3dDevice->TestCooperativeLevel()))
    {
        if (hr == D3DERR_DEVICELOST)
        {
            TRACE("D3DERR_DEVICELOST\n");
            return;
        }

        if (hr == D3DERR_DEVICENOTRESET)
        {
            TRACE("D3DERR_DEVICENOTRESET\n");
            Reset();
            return;
        }
        else
        {
            TRACE("TestCooperativeLevel() failed (other)\n");
            return;
        }
    }

    // Draw any changed lines to the back buffer
    if (!DrawChanges(pScreen_, pafDirty_))
        return;

    hr = m_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0L);
    hr = m_pd3dDevice->BeginScene();

    bool fFilter = GUI::IsActive() ? GetOption(filtergui) || (GetOption(scale) & 1) : GetOption(filter);
    uint32_t dwFilter = fFilter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, dwFilter);
    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, dwFilter);

    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    float vertexConsts[][4] =
    {
        {
            2.0f / pScreen_->GetPitch(),
            -2.0f / pScreen_->GetHeight() * (GUI::IsActive() ? 1.0f : 2.0f),
            GetOption(scanhires) ? (float(m_rTarget.right) / pScreen_->GetPitch()) : 1.0f,
            GetOption(scanhires) ? (float(m_rTarget.bottom) / pScreen_->GetHeight() / (GUI::IsActive() ? 2.0f : 1.0f)) : 1.0f,
        },
        {
            0.5f / TEXTURE_SIZE,
            1.0f / TEXTURE_SIZE,
            1.0f,
            1.0f,
        }
    };

    float pixelConsts[][4] =
    {
        {
            GetOption(scanlines) && !GUI::IsActive() ? GetOption(scanlevel) / 100.0f : 1.0f,
            1.0f,
            1.0f,
            1.0f,
        }
    };

    hr = m_pd3dDevice->SetVertexShaderConstantF(0, vertexConsts[0], ARRAYSIZE(vertexConsts));
    hr = m_pd3dDevice->SetPixelShaderConstantF(0, pixelConsts[0], ARRAYSIZE(pixelConsts));

    hr = m_pd3dDevice->SetVertexDeclaration(m_pVertexDecl);
    hr = m_pd3dDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(CUSTOMVERTEX));
    hr = m_pd3dDevice->SetTexture(0, m_pTexture);

    hr = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    hr = m_pd3dDevice->EndScene();
    hr = m_pd3dDevice->Present(nullptr, nullptr, hwndCanvas, nullptr);
}

HRESULT Direct3D9Video::CreateTextures()
{
    HRESULT hr = 0;

    if (!m_pd3dDevice)
        return D3DERR_INVALIDDEVICE;

    if (m_pTexture) m_pTexture->Release(), m_pTexture = nullptr;

    if (FAILED(hr = m_pd3dDevice->CreateTexture(TEXTURE_SIZE, TEXTURE_SIZE, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTexture, nullptr)))
        return hr;

    UpdatePalette();
    Video::SetDirty();

    return hr;
}

HRESULT Direct3D9Video::CreateVertices()
{
    HRESULT hr = 0;

    if (!m_pd3dDevice)
        return D3DERR_INVALIDDEVICE;

    if (m_pVertexDecl) m_pVertexDecl->Release(), m_pVertexDecl = nullptr;
    hr = m_pd3dDevice->CreateVertexDeclaration(vertexDecl, &m_pVertexDecl);

    if (m_pVertexBuffer) m_pVertexBuffer->Release(), m_pVertexBuffer = nullptr;

    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(NUM_VERTICES * sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_pVertexBuffer, nullptr)))
        return hr;

    uint32_t dwWidth = Frame::GetWidth();
    uint32_t dwHeight = Frame::GetHeight();

    CUSTOMVERTEX* pVertices = nullptr;
    hr = m_pVertexBuffer->Lock(0, NUM_VERTICES * sizeof(CUSTOMVERTEX), (void**)&pVertices, 0);

    // Main display, also used for scanlines
    pVertices[0] = CUSTOMVERTEX(0, dwHeight);
    pVertices[1] = CUSTOMVERTEX(0, 0);
    pVertices[2] = CUSTOMVERTEX(dwWidth, dwHeight);
    pVertices[3] = CUSTOMVERTEX(dwWidth, 0);

    hr = m_pVertexBuffer->Unlock();


    UINT uWidthView = m_d3dpp.BackBufferWidth;
    UINT uHeightView = m_d3dpp.BackBufferHeight;

    UINT uWidth = dwWidth, uHeight = dwHeight;
    if (GetOption(ratio5_4)) uWidth = uWidth * 5 / 4;

    UINT uWidthFit = MulDiv(uWidth, uHeightView, uHeight);
    UINT uHeightFit = MulDiv(uHeight, uWidthView, uWidth);

    if (uWidthFit <= uWidthView)
    {
        uWidth = uWidthFit;
        uHeight = uHeightView;
    }
    else if (uHeightFit <= uHeightView)
    {
        uWidth = uWidthView;
        uHeight = uHeightFit;
    }

    SetRect(&m_rTarget, 0, 0, uWidth, uHeight);

    return D3D_OK;
}

HRESULT Direct3D9Video::CreateShaders()
{
    HRESULT hr = D3D_OK;

    if (m_pVertexShader) m_pVertexShader->Release(), m_pVertexShader = nullptr;
    hr = m_pd3dDevice->CreateVertexShader((DWORD*)g_D3D9_VS, &m_pVertexShader);
    hr = m_pd3dDevice->SetVertexShader(m_pVertexShader);

    if (m_pPixelShader) m_pPixelShader->Release(), m_pPixelShader = nullptr;
    hr = m_pd3dDevice->CreatePixelShader((DWORD*)g_D3D9_PS, &m_pPixelShader);
    hr = m_pd3dDevice->SetPixelShader(m_pPixelShader);

    return hr;
}

HRESULT Direct3D9Video::CreateDevice()
{
    HRESULT hr = D3D_OK;

    if (!m_pd3d)
        return D3DERR_INVALIDDEVICE;

    if (m_pd3dDevice) m_pd3dDevice->Release(), m_pd3dDevice = nullptr;

    D3DDISPLAYMODE d3ddm;
    hr = m_pd3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr))
    {
        TRACE("GetAdapterDisplayMode failed with %#08lx\n", hr);
        return hr;
    }

    ZeroMemory(&m_d3dpp, sizeof(m_d3dpp));
    m_d3dpp.hDeviceWindow = g_hwnd;
    m_d3dpp.Windowed = true;
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferCount = 1;
    m_d3dpp.Flags = 0;
    m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    m_d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;

    if (GetOption(fullscreen))
    {
        m_d3dpp.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
        m_d3dpp.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    hr = m_pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwndCanvas, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &m_d3dpp, &m_pd3dDevice);
    if (FAILED(hr))
        TRACE("CreateDevice failed with %#08lx\n", hr);

    return hr;
}

bool Direct3D9Video::Reset(bool fNewDevice_)
{
    static bool fResetting = false;
    HRESULT hr = 0;

    if (!m_pd3d || fResetting)
        return false;

    if (m_pTexture) m_pTexture->Release(), m_pTexture = nullptr;
    if (m_pVertexBuffer) m_pVertexBuffer->Release(), m_pVertexBuffer = nullptr;

    m_d3dpp.BackBufferWidth = m_rTarget.right;
    m_d3dpp.BackBufferHeight = m_rTarget.bottom;

    if (m_pd3dDevice && !fNewDevice_)
    {
        fResetting = true;
        if (FAILED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
            TRACE("Reset() returned %#08lx\n", hr);
        fResetting = false;
    }
    else
    {
        if (m_pd3dDevice) m_pd3dDevice->Release(), m_pd3dDevice = nullptr;
        hr = CreateDevice();
    }

    if (m_pd3dDevice)
    {
        // Disable culling, z-buffer, and lighting
        hr = m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        hr = m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        hr = m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        hr = m_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

        CreateTextures();
        CreateVertices();
        CreateShaders();
    }

    Video::SetDirty();

    return SUCCEEDED(hr);
}

// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool Direct3D9Video::DrawChanges(CScreen* pScreen_, bool* pafDirty_)
{
    HRESULT hr = 0;

    int nWidth = pScreen_->GetPitch();
    int nHeight = pScreen_->GetHeight();

    bool fHalfHeight = !GUI::IsActive();
    if (fHalfHeight) nHeight /= 2;

    RECT rect = { 0, 0, nWidth / 4, nHeight / 4 };
    D3DLOCKED_RECT d3dlr;
    if (!m_pTexture || FAILED(hr = m_pTexture->LockRect(0, &d3dlr, &rect, 0)))
    {
        TRACE("!!! DrawChanges() failed to lock back surface (%#08lx)\n", hr);
        return false;
    }

    uint32_t* pdwBack = reinterpret_cast<uint32_t*>(d3dlr.pBits), * pdw = pdwBack;
    LONG lPitchDW = d3dlr.Pitch >> 2;

    uint8_t* pbSAM = pScreen_->GetLine(0), * pb = pbSAM;
    LONG lPitch = pScreen_->GetPitch();

    int nRightHi = nWidth >> 3;

    nWidth <<= 2;

    for (int y = 0; y < nHeight; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
    {
        if (!pafDirty_[y])
            continue;

        for (int x = 0; x < nRightHi; x++)
        {
            pdw[0] = adwPalette[pb[0]];
            pdw[1] = adwPalette[pb[1]];
            pdw[2] = adwPalette[pb[2]];
            pdw[3] = adwPalette[pb[3]];
            pdw[4] = adwPalette[pb[4]];
            pdw[5] = adwPalette[pb[5]];
            pdw[6] = adwPalette[pb[6]];
            pdw[7] = adwPalette[pb[7]];

            pdw += 8;
            pb += 8;
        }

        pafDirty_[y] = false;
    }

    // With bilinear filtering enabled, the GUI display in the lower half bleeds
    // into the bottom line of the display, so clear it when changing modes.
    static bool fLastHalfHeight = true;
    if (fHalfHeight && !fLastHalfHeight)
    {
        auto pb = reinterpret_cast<uint8_t*>(d3dlr.pBits) + nHeight * d3dlr.Pitch;
        memset(pb, 0, d3dlr.Pitch);
    }
    fLastHalfHeight = fHalfHeight;

    m_pTexture->UnlockRect(0);

    return true;
}

void Direct3D9Video::UpdateSize()
{
    // Don't attempt to adjust for a minimised state
    if (IsMinimized(g_hwnd))
        return;

    // Fetch current client size
    RECT rNew;
    GetClientRect(hwndCanvas, &rNew);

    // If it's changed since last time, reinitialise the device
    if (!EqualRect(&rNew, &m_rTarget))
    {
        m_rTarget = rNew;
        Reset();
    }
}


// Map a native size/offset to SAM view port
void Direct3D9Video::DisplayToSamSize(int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth;

    *pnX_ = *pnX_ * Frame::GetWidth() / (m_rTarget.right << nHalfWidth);
    *pnY_ = *pnY_ * Frame::GetHeight() / (m_rTarget.bottom << nHalfHeight);
}

// Map a native client point to SAM view port
void Direct3D9Video::DisplayToSamPoint(int* pnX_, int* pnY_)
{
    DisplayToSamSize(pnX_, pnY_);
}
