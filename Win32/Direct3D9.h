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
        HRESULT CreateSurfaces ();
        HRESULT CreateVertices ();
        HRESULT CreateDevice ();
        bool Reset (bool fNewDevice_=false);
        bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);

    private:
        LPDIRECT3D9 m_pd3d;
        LPDIRECT3DDEVICE9 m_pd3dDevice;
        LPDIRECT3DTEXTURE9 m_pTexture, m_pScanlineTexture;
        LPDIRECT3DVERTEXBUFFER9 m_pVertexBuffer;
        D3DPRESENT_PARAMETERS m_d3dpp;

        RECT m_rTarget;
};

#endif // DIRECT3D9_H
