// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.h: Integrated Z80 debugger
//
//  Copyright (c) 1999-2003  Simon Owen
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

#ifndef DEBUG_H
#define DEBUG_H

#include "CScreen.h"
#include "Expr.h"
#include "GUI.h"
#include "Memory.h"

typedef struct tagBREAKPT
{
    union
    {
        const BYTE* pAddr;
        struct { WORD wMask, wCompare; } Port;
    };

    EXPR* pExpr;
    struct tagBREAKPT* pNext;
}
BREAKPT;


class Debug
{
    public:
        static bool Start (BREAKPT* pBreak_=NULL);
        static void Stop ();
        static void Refresh ();

        static void OnRet ();

        static bool IsActive ();
        static bool IsBreakpointSet ();
        static bool BreakpointHit ();
};


// Address wrapper - currently for regular WORD addresses, but with plans to extend to handle virtual addresses too
class CAddr
{
    public:
        CAddr (WORD wAddr_=0) : m_wAddr(wAddr_) { }

    public:
        UINT GetPC () const { return m_wAddr; }
        BYTE* GetPhys () const { return phys_read_addr(m_wAddr); }

        int sprint (char* psz_) { return sprintf(psz_, "%04X", m_wAddr); }

    public:
        UINT operator= (WORD wAddr_) { m_wAddr = wAddr_; return m_wAddr; }
        bool operator== (const CAddr& addr_) { return m_wAddr == addr_.m_wAddr; }
        bool operator!= (const CAddr& addr_) { return m_wAddr != addr_.m_wAddr; }
        CAddr operator++ (int) { CAddr a(m_wAddr); m_wAddr++; Normalise(); return a; }
        CAddr operator-- (int) { CAddr a(m_wAddr); m_wAddr--; Normalise(); return a; }
        CAddr operator++ () { m_wAddr++; return Normalise(); }
        CAddr operator-- () { m_wAddr--; return Normalise(); }
        CAddr operator-= (UINT u_) { m_wAddr -= u_; return Normalise(); }
        CAddr operator+= (UINT u_) { m_wAddr += u_; return Normalise(); }
        CAddr operator+ (UINT u_) { CAddr a(m_wAddr + u_); return a.Normalise(); }
        CAddr operator- (UINT u_) { CAddr a(m_wAddr - u_); return a.Normalise(); }
        BYTE operator* () const { return read_byte(m_wAddr); }
        BYTE operator[] (UINT u_) const { return read_byte(m_wAddr+u_); }

        CAddr Normalise () { return *this; }

    protected:
        WORD m_wAddr;
};


class CView : public CWindow
{
    public:
        CView (CWindow* pParent_) : CWindow(pParent_, 5, 5, 256, 240) { }

    public:
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) { return false; }

        CAddr GetAddress () const { return m_addr; }
        virtual void SetAddress (CAddr Addr_, bool fForceTop_=false) = 0;

    public:
        bool IsTabStop() const { return true; }

    protected:
        CAddr m_addr;
};


class CCodeView : public CView
{
    public:
        CCodeView (CWindow* pParent_);
        ~CCodeView () { delete m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (CAddr Addr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        void SetFlowTarget ();
        void cmdNavigate (int nKey_, int nMods_);

    protected:
        UINT m_uRows, m_uColumns, m_uTarget;
        const char* m_pcszTarget;
        char* m_pszData;

        static CAddr s_aAddrs[];
};


class CTextView : public CView
{
    public:
        CTextView (CWindow* pParent_);
        ~CTextView () { delete m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (CAddr addr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    protected:
        UINT m_uRows, m_uColumns;
        char* m_pszData;

        static CAddr s_aAddrs[];
};

class CNumView : public CView
{
    public:
        CNumView (CWindow* pParent_);
        ~CNumView () { delete m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (CAddr addr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    protected:
        UINT m_uRows, m_uColumns;
        char* m_pszData;

        static CAddr s_aAddrs[];
};

/*
class CMemView : public CView
{
    public:
        CMemView (CWindow* pParent_);

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
};
*/

class CGraphicsView : public CView
{
    public:
        CGraphicsView (CWindow* pParent_);
        ~CGraphicsView () { delete m_pbData; }

    public:
        void SetAddress (CAddr addr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    protected:
        UINT m_uStrips, m_uStripWidth, m_uStripLines;
        BYTE* m_pbData;

        static UINT s_uMode, s_uWidth, s_uZoom;
};


class CRegisterPanel : public CWindow
{
    public:
        CRegisterPanel (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void Draw (CScreen* pScreen_);
};


class CDebugger : public CDialog
{
    public:
        CDebugger (BREAKPT* pBreak_=NULL);
        ~CDebugger ();

        bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        void EraseBackground (CScreen* pScreen_);
        void Draw (CScreen* pScreen_);

        void SetAddress (CAddr Addr_);

    protected:
        void Refresh ();

    protected:
        CView* m_pView;
        CRegisterPanel* m_pRegPanel;

        static bool s_fTransparent;
};


typedef bool (*PFNINPUTPROC)(EXPR *pExpr_);

class CInputDialog : public CDialog
{
    public:
        CInputDialog (CWindow* pParent_, const char* pcszCaption_, const char* pcszPrompt_, PFNINPUTPROC pfn_);

    public:
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CEditControl* m_pInput;
        PFNINPUTPROC m_pfnNotify;
};

#endif
