// Part of SimCoupe - A SAM Coupe emulator
//
// Direct3D.h: Direct3D 9 display
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef DIRECT3D9_H
#define DIRECT3D9_H

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#undef new
#endif

#define DIRECT3D_VERSION	0x0900
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9math.h>

#include "Video.h"

class Direct3D9Video : public VideoBase
{
    public:
        Direct3D9Video ();
        ~Direct3D9Video ();

    public:
        int GetCaps () const;
        bool Init (bool fFirstInit_);

        void Update (CScreen* pScreen_, bool *pafDirty_);
        void UpdateSize ();
        void UpdatePalette ();

        void DisplayToSamSize (int* pnX_, int* pnY_);
        void DisplayToSamPoint (int* pnX_, int* pnY_);

    protected:
        HRESULT CreateTextures ();
		HRESULT CreateShaders ();
        HRESULT CreateVertices ();
        HRESULT CreateDevice ();
        bool Reset (bool fNewDevice_=false);
        bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);

    private:
        LPDIRECT3D9 m_pd3d;
        LPDIRECT3DDEVICE9 m_pd3dDevice;
        LPDIRECT3DTEXTURE9 m_pTexture;
        LPDIRECT3DVERTEXBUFFER9 m_pVertexBuffer;
		LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;
		LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
		LPDIRECT3DPIXELSHADER9 m_pPixelShader;
        D3DPRESENT_PARAMETERS m_d3dpp;

        RECT m_rTarget;
};

#endif // DIRECT3D9_H
