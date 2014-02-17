// Part of SimCoupe - A SAM Coupe emulator
//
// Direct3D.cpp: Direct3D9 display
//
//  Copyright (c) 2012-2014 Simon Owen
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "Direct3D9.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

#pragma comment(lib, "d3d9.lib")


#define TEXTURE_SIZE    1024
#define NUM_VERTICES    12
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

struct CUSTOMVERTEX
{
    CUSTOMVERTEX(FLOAT x_, FLOAT y_, FLOAT z_, FLOAT w_, FLOAT tu_, FLOAT tv_)
    {
        position = D3DXVECTOR4(x_,y_,z_,w_);
        tu = tu_;
        tv = tv_;
    }

    D3DXVECTOR4 position;
    FLOAT       tu, tv;
};


// SAM palette in native surface values (faster if kept at file scope)
static DWORD adwPalette[N_PALETTE_COLOURS];


Direct3D9Video::Direct3D9Video ()
    : m_pd3d(NULL), m_pd3dDevice(NULL), m_pTexture(NULL), m_pScanlineTexture(NULL), m_pVertexBuffer(NULL)
{
    memset(&m_d3dpp, 0, sizeof(m_d3dpp));
    SetRectEmpty(&m_rTarget);
}

Direct3D9Video::~Direct3D9Video ()
{
    if (m_pTexture) m_pTexture->Release(), m_pTexture = NULL;
    if (m_pScanlineTexture) m_pScanlineTexture->Release(), m_pScanlineTexture = NULL;
    if (m_pVertexBuffer) m_pVertexBuffer->Release(), m_pVertexBuffer = NULL;
    if (m_pd3dDevice) m_pd3dDevice->Release(), m_pd3dDevice = NULL;
    if (m_pd3d) m_pd3d->Release(), m_pd3d = NULL;
}


int Direct3D9Video::GetCaps () const
{
    return VCAP_STRETCH | VCAP_FILTER |VCAP_SCANHIRES;
}

bool Direct3D9Video::Init (bool fFirstInit_)
{
    TRACE("Direct3D9Video::Init()\n");

    // If hardware acceleration is disabled we should fall back on DirectDraw software
    if (!GetOption(hwaccel))
        return false;

    // D3D.DLL is delay-loaded, so be prepared for this to fail if it's not available
    __try
    {
        m_pd3d = Direct3DCreate9(D3D_SDK_VERSION);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        TRACE("D3D9.dll not found!\n");
        return false;
    }

    CreateDevice();
    UpdateSize();

    return m_pd3d != NULL;
}


void Direct3D9Video::UpdatePalette ()
{
    const COLOUR *pSAM = IO::GetPalette();
    const DWORD dwRmask = 0x00ff0000, dwGmask = 0x0000ff00, dwBmask = 0x000000ff, dwAmask = 0xff000000;

    // Build the palette from SAM colours
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR *p = &pSAM[i];
        BYTE r = p->bRed, g = p->bGreen, b = p->bBlue, a = 0xff;

        // Set native pixel value
        adwPalette[i] = RGB2Native(r,g,b,a, dwRmask, dwGmask, dwBmask, dwAmask);
    }


    D3DSURFACE_DESC d3dsd;
    D3DLOCKED_RECT d3dlr;

    if (m_pScanlineTexture &&
        SUCCEEDED(m_pScanlineTexture->GetLevelDesc(0, &d3dsd)) &&
        SUCCEEDED(m_pScanlineTexture->LockRect(0, &d3dlr, 0, 0)))
    {
        DWORD *pdw = (DWORD*)d3dlr.pBits;
        int nPitchW = d3dlr.Pitch/4;

        BYTE bScanlineAlpha = ((100-GetOption(scanlevel)) * 0xff) / 100;
        DWORD dwScanlineAlpha = (bScanlineAlpha << 24);

        for (UINT y = 0; y < d3dsd.Height ; y++)
        {
            DWORD dwCol = (y & 1) ? dwScanlineAlpha : 0x00000000;

            for (UINT x = 0; x < d3dsd.Width ; x++)
                pdw[y*nPitchW + x] = dwCol;
        }

        m_pScanlineTexture->UnlockRect(0);
    }

    // Redraw to reflect any changes
    Video::SetDirty();
}

// Update the display to show anything that's changed since last time
void Direct3D9Video::Update (CScreen* pScreen_, bool *pafDirty_)
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

    hr = m_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0L);
    hr = m_pd3dDevice->BeginScene();

    bool fFilter = GUI::IsActive() ? GetOption(filtergui) : GetOption(filter);
    DWORD dwFilter = fFilter ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    DWORD dwCurrentFilter = 0;
    if (SUCCEEDED(m_pd3dDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &dwCurrentFilter)) && dwFilter != dwCurrentFilter)
    {
        hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, dwFilter);
        hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, dwFilter);
    }

    int nVertexOffset = GUI::IsActive() ? 0 : 4;

    hr = m_pd3dDevice->SetTexture(0, m_pTexture);
    hr = m_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
    hr = m_pd3dDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(CUSTOMVERTEX));
    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    hr = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nVertexOffset, 2);

    if (GetOption(scanlines) && !GUI::IsActive() && (int(m_d3dpp.BackBufferWidth) >= Frame::GetWidth()))
    {
        nVertexOffset = GetOption(scanhires) ? 8 : 0;
        hr = m_pd3dDevice->SetTexture(0, m_pScanlineTexture);
        hr = m_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
        hr = m_pd3dDevice->SetStreamSource(0, m_pVertexBuffer, 0, sizeof(CUSTOMVERTEX));
        hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
        hr = m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        hr = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, nVertexOffset, 2);
    }

    hr = m_pd3dDevice->EndScene();
    hr = m_pd3dDevice->Present(NULL, NULL, hwndCanvas, NULL);
}

HRESULT Direct3D9Video::CreateSurfaces ()
{
    HRESULT hr = 0;

    if (!m_pd3dDevice)
        return D3DERR_INVALIDDEVICE;

    if (m_pTexture) m_pTexture->Release(), m_pTexture = NULL;
    if (m_pScanlineTexture) m_pScanlineTexture->Release(), m_pScanlineTexture = NULL;

    if (FAILED(hr = m_pd3dDevice->CreateTexture(TEXTURE_SIZE, TEXTURE_SIZE, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pTexture, NULL)))
        return hr;

    if (FAILED(hr = m_pd3dDevice->CreateTexture(TEXTURE_SIZE, TEXTURE_SIZE, 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_pScanlineTexture, NULL)))
    {
        m_pTexture->Release(), m_pTexture = NULL;
        return hr;
    }

    D3DSURFACE_DESC d3dsd;
    D3DLOCKED_RECT d3dlr;
    if (SUCCEEDED(hr = m_pTexture->GetLevelDesc(0, &d3dsd)) &&
        SUCCEEDED(hr = m_pTexture->LockRect(0, &d3dlr, 0, 0)))
    {
        for (UINT  y = 0; y < TEXTURE_SIZE ; y++)
        {
            BYTE *pb = reinterpret_cast<BYTE*>(d3dlr.pBits);
            memset(pb + y*d3dlr.Pitch, 0, TEXTURE_SIZE*4);
        }

        m_pTexture->UnlockRect(0);
    }

    UpdatePalette();
    Video::SetDirty();

    return hr;
}

HRESULT Direct3D9Video::CreateVertices ()
{
    HRESULT hr = 0;

    if (!m_pd3dDevice)
        return D3DERR_INVALIDDEVICE;

    if (m_pVertexBuffer) m_pVertexBuffer->Release(), m_pVertexBuffer = NULL;

    if (FAILED(hr = m_pd3dDevice->CreateVertexBuffer(NUM_VERTICES*sizeof(CUSTOMVERTEX), D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &m_pVertexBuffer, NULL)))
        return hr;

    DWORD dwWidth = Frame::GetWidth();
    DWORD dwHeight = Frame::GetHeight();

    UINT uWidthView = m_d3dpp.BackBufferWidth;
    UINT uHeightView = m_d3dpp.BackBufferHeight;

    UINT uWidth = dwWidth, uHeight = dwHeight;
    if (GetOption(ratio5_4)) uWidth = uWidth * 5/4;


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

    RECT r = { 0, 0, uWidth, uHeight };
    OffsetRect(&r, (uWidthView - r.right)/2, (uHeightView - r.bottom)/2);
    m_rTarget = r;

    CUSTOMVERTEX *pVertices;
    hr = m_pVertexBuffer->Lock(0, NUM_VERTICES*sizeof(CUSTOMVERTEX), (void**)&pVertices, 0);


    float tW = float(dwWidth) / float(TEXTURE_SIZE);
    float tH = float(dwHeight) / float(TEXTURE_SIZE);

    // Main display, also used for scanlines
    pVertices[0] = CUSTOMVERTEX((float)r.left  - 0.5f, (float)r.bottom - 0.5f, 0.0f, 1.0f,  0.0f,   tH);
    pVertices[1] = CUSTOMVERTEX((float)r.left  - 0.5f, (float)r.top    - 0.5f, 0.0f, 1.0f,  0.0f,   0.0f);
    pVertices[2] = CUSTOMVERTEX((float)r.right - 0.5f, (float)r.bottom - 0.5f, 0.0f, 1.0f,    tW,   tH);
    pVertices[3] = CUSTOMVERTEX((float)r.right - 0.5f, (float)r.top    - 0.5f, 0.0f, 1.0f,    tW,   0.0f);

    // Main display with height doubled
    pVertices[4] = pVertices[0]; pVertices[4].tv /= 2.0f;
    pVertices[5] = pVertices[1];
    pVertices[6] = pVertices[2]; pVertices[6].tv /= 2.0f;
    pVertices[7] = pVertices[3];

    tW = float(uWidthView) / float(TEXTURE_SIZE);
    tH = float(uHeightView) / float(TEXTURE_SIZE);

    // Matching display mode for native scanlines
    pVertices[8]  = CUSTOMVERTEX(0.0f              - 0.5f, float(uHeightView) - 0.5f, 0.0f, 1.0f,  0.0f,   tH);
    pVertices[9]  = CUSTOMVERTEX(0.0f              - 0.5f, 0.0f               - 0.5f, 0.0f, 1.0f,  0.0f, 0.0f);
    pVertices[10] = CUSTOMVERTEX(float(uWidthView) - 0.5f, float(uHeightView) - 0.5f, 0.0f, 1.0f,    tW,   tH);
    pVertices[11] = CUSTOMVERTEX(float(uWidthView) - 0.5f, 0.0f               - 0.5f, 0.0f, 1.0f,    tW, 0.0f);

    m_pVertexBuffer->Unlock();

    return D3D_OK;
}

HRESULT Direct3D9Video::CreateDevice ()
{
    HRESULT hr = D3D_OK;

    if (!m_pd3d)
        return D3DERR_INVALIDDEVICE;

    if (m_pd3dDevice) m_pd3dDevice->Release(), m_pd3dDevice = NULL;

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
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD; // required for SetDialogBoxMode()
    m_d3dpp.BackBufferCount = 1;
    m_d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // required for SetDialogBoxMode()
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

bool Direct3D9Video::Reset (bool fNewDevice_)
{
    static bool fResetting = false;
    HRESULT hr = 0;

    if (!m_pd3d || fResetting)
        return false;

    if (m_pTexture) m_pTexture->Release(), m_pTexture = NULL;
    if (m_pScanlineTexture) m_pScanlineTexture->Release(), m_pScanlineTexture = NULL;
    if (m_pVertexBuffer) m_pVertexBuffer->Release(), m_pVertexBuffer = NULL;

    m_d3dpp.BackBufferWidth = m_rTarget.right - m_rTarget.left;
    m_d3dpp.BackBufferHeight = m_rTarget.bottom - m_rTarget.top;

    if (m_pd3dDevice && !fNewDevice_)
    {
        fResetting = true;
        if (FAILED(hr = m_pd3dDevice->Reset(&m_d3dpp)))
            TRACE("Reset() returned %#08lx\n", hr);
        fResetting = false;
    }
    else
    {
        if (m_pd3dDevice) m_pd3dDevice->Release(), m_pd3dDevice = NULL;
        hr = CreateDevice();
    }

    if (m_pd3dDevice)
    {
        m_pd3dDevice->SetDialogBoxMode(TRUE);
        m_pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

        CreateSurfaces();
        CreateVertices();
    }

    Video::SetDirty();

    return SUCCEEDED(hr);
}

// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool Direct3D9Video::DrawChanges (CScreen* pScreen_, bool *pafDirty_)
{
    HRESULT hr = 0;

    int nWidth = pScreen_->GetPitch();
    int nHeight = pScreen_->GetHeight();

    bool fHalfHeight = !GUI::IsActive();
    if (fHalfHeight) nHeight /= 2;

    RECT rect = { 0, 0, nWidth/4, nHeight/4 };
    D3DLOCKED_RECT d3dlr;
    if (!m_pTexture || FAILED(hr = m_pTexture->LockRect(0, &d3dlr, &rect, 0)))
    {
        TRACE("!!! DrawChanges() failed to lock back surface (%#08lx)\n", hr);
        return false;
    }

    DWORD *pdwBack = reinterpret_cast<DWORD*>(d3dlr.pBits), *pdw = pdwBack;
    LONG lPitchDW = d3dlr.Pitch >> 2;
    bool *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    LONG lPitch = pScreen_->GetPitch();

    int nRightHi = nWidth >> 3, nRightLo = nRightHi >> 1;

    nWidth <<= 2;

    for (int y = 0 ; y < nHeight ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
    {
        if (!pafDirty_[y])
            continue;

        if (pfHiRes[y])
        {
            for (int x = 0 ; x < nRightHi ; x++)
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
        }
        else
        {
            for (int x = 0 ; x < nRightLo ; x++)
            {
                pdw[0]  = pdw[1]  = adwPalette[pb[0]];
                pdw[2]  = pdw[3]  = adwPalette[pb[1]];
                pdw[4]  = pdw[5]  = adwPalette[pb[2]];
                pdw[6]  = pdw[7]  = adwPalette[pb[3]];
                pdw[8]  = pdw[9]  = adwPalette[pb[4]];
                pdw[10] = pdw[11] = adwPalette[pb[5]];
                pdw[12] = pdw[13] = adwPalette[pb[6]];
                pdw[14] = pdw[15] = adwPalette[pb[7]];

                pdw += 16;
                pb += 8;
            }
        }

        pafDirty_[y] = false;
    }

    // With bilinear filtering enabled, the GUI display in the lower half bleeds
    // into the bottom line of the display, so clear it when changing modes.
    static bool fLastHalfHeight = true;
    if (fHalfHeight && !fLastHalfHeight)
    {
        BYTE *pb = reinterpret_cast<BYTE*>(d3dlr.pBits) + nHeight*d3dlr.Pitch;
        memset(pb, 0, d3dlr.Pitch);
    }
    fLastHalfHeight = fHalfHeight;

    m_pTexture->UnlockRect(0);

    return true;
}

void Direct3D9Video::UpdateSize ()
{
    // Don't attempt to adjust for a minimised state
    if (IsMinimized(g_hwnd))
        return;

    // Determine screen location of canvas top-left
    POINT ptOffset = { 0,0 };
    ClientToScreen(hwndCanvas, &ptOffset);

    // Convert canvas rect to screen coordinates
    RECT rNew;
    GetClientRect(hwndCanvas, &rNew);
    OffsetRect(&rNew, ptOffset.x, ptOffset.y);

    // If it's changed since last time, reinitialise the video
    if (!EqualRect(&rNew, &m_rTarget))
    {
        m_rTarget = rNew;
        Reset();
    }
}


// Map a native size/offset to SAM view port
void Direct3D9Video::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth;

    *pnX_ = *pnX_ * Frame::GetWidth()  / ((m_rTarget.right-m_rTarget.left) << nHalfWidth);
    *pnY_ = *pnY_ * Frame::GetHeight() / ((m_rTarget.bottom-m_rTarget.top) << nHalfHeight);
}

// Map a native client point to SAM view port
void Direct3D9Video::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= m_rTarget.left;
    *pnY_ -= m_rTarget.top;
    DisplayToSamSize(pnX_, pnY_);
}
