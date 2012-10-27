// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.h: Integrated Z80 debugger
//
//  Copyright (c) 1999-2012  Simon Owen
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

#include "Breakpoint.h"
#include "GUI.h"
#include "Screen.h"

class Debug
{
    public:
        static bool Start (BREAKPT* pBreak_=NULL);
        static void Stop ();
        static void FrameEnd ();

        static void OnRet ();

        static bool IsActive ();
        static bool IsBreakpointSet ();
        static bool BreakpointHit ();
};


enum ViewType { vtDis, vtTxt, vtHex, vtGfx, vtBpt };

class CView : public CWindow
{
    public:
        CView (CWindow* pParent_)
        : CWindow(pParent_, 4, 5, pParent_->GetWidth()-8, pParent_->GetHeight()-10-12), m_wAddr(0) { }

    public:
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) { return false; }

        WORD GetAddress () const { return m_wAddr; }
        virtual void SetAddress (WORD wAddr_, bool fForceTop_=false) { m_wAddr = wAddr_; }

    private:
        WORD m_wAddr;
};


class CDisView : public CView
{
    public:
        static const UINT INVALID_TARGET = 0U-1;

    public:
        CDisView (CWindow* pParent_);
        ~CDisView () { delete[] m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    public:
        static void DrawRegisterPanel (CScreen* pScreen_, int nX_, int nY_);

    protected:
        void SetFlowTarget ();
        bool cmdNavigate (int nKey_, int nMods_);

    private:
        UINT m_uRows, m_uColumns, m_uTarget;
        const char* m_pcszTarget;
        char* m_pszData;

        static WORD s_wAddrs[];
};


class CTxtView : public CView
{
    public:
        CTxtView (CWindow* pParent_);
        ~CTxtView () { delete[] m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    private:
        int m_nRows, m_nColumns;
        char *m_pszData;

        bool m_fEditing;
        WORD m_wEditAddr;
};

class CHexView : public CView
{
    public:
        CHexView (CWindow* pParent_);
        ~CHexView () { delete[] m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    private:
        int m_nRows, m_nColumns;
        char *m_pszData;

        bool m_fEditing, m_fRightNibble;
        WORD m_wEditAddr;
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

class CGfxView : public CView
{
    public:
        CGfxView (CWindow* pParent_);
        ~CGfxView () { delete[] m_pbData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    protected:
        bool m_fGrid;
        UINT m_uStrips, m_uStripWidth, m_uStripLines;
        BYTE* m_pbData;

        static UINT s_uMode, s_uWidth, s_uZoom;
};

class CBptView : public CView
{
    public:
        CBptView (CWindow* pParent_);
        ~CBptView () { delete[] m_pszData; m_pszData = NULL; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool cmdNavigate (int nKey_, int nMods_);

    private:
        int m_nRows, m_nLines, m_nTopLine, m_nActive;
        char *m_pszData;
};


class CDebugger : public CDialog
{
    public:
        CDebugger (BREAKPT* pBreak_=NULL);
        ~CDebugger ();

        bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        void OnNotify (CWindow* pWindow_, int nParam_);
        void EraseBackground (CScreen* pScreen_);
        void Draw (CScreen* pScreen_);

        void Refresh ();
        void SetSubTitle (const char *pcszSubTitle_);
        void SetAddress (WORD wAddr_);
        void SetView (ViewType nView);
        void SetStatus (const char *pcsz_, BYTE bColour_=WHITE, const GUIFONT *pFont_=NULL);
        void SetStatusByte (WORD wAddr_);
        bool Execute (const char* pcszCommand_);

    protected:
        ViewType m_nView;
        CView* m_pView;
        CEditControl *m_pCommandEdit;
        CTextControl *m_pStatus;

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
