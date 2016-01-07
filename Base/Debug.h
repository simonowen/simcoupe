// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.h: Integrated Z80 debugger
//
//  Copyright (c) 1999-2014 Simon Owen
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

namespace Debug
{
	bool Start (BREAKPT* pBreak_=nullptr);
	void Stop ();
	void FrameEnd ();

	void OnRet ();
	bool RetZHook ();

	bool IsActive ();
	bool IsBreakpointSet ();
	bool BreakpointHit ();
}

enum ViewType { vtDis, vtTxt, vtHex, vtGfx, vtBpt, vtTrc };

class CView : public CWindow
{
    public:
        CView (CWindow* pParent_)
        : CWindow(pParent_, 4, 5, pParent_->GetWidth()-8, pParent_->GetHeight()-10-12), m_wAddr(0) { }

    public:
        bool OnMessage (int /*nMessage_*/, int /*nParam1_*/, int /*nParam2_*/) override { return false; }

        WORD GetAddress () const { return m_wAddr; }
        virtual void SetAddress (WORD wAddr_, bool /*fForceTop_*/=false) { m_wAddr = wAddr_; }
        virtual bool cmdNavigate (int nKey_, int nMods_) = 0;

    private:
        WORD m_wAddr = 0;
};

class CTextView : public CView
{
    public:
        CTextView (CWindow *pParent_);

    public:
        int GetLines () const { return m_nLines; }
        void SetLines (int nLines_) { m_nLines = nLines_; }
        int GetTopLine () const { return m_nTopLine; }

        virtual void DrawLine (CScreen *pScreen_, int nX_, int nY_, int nLine_) = 0;
        virtual bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;
        virtual bool cmdNavigate (int nKey_, int nMods_) override;
        virtual void OnDblClick (int /*nLine_*/) { }
        virtual void OnDelete () { }

    protected:
        void Draw (CScreen* pScreen_) override;

    private:
        int m_nLines = 0;    // Lines of content to display
        int m_nTopLine = 0;  // Line number of first visible line
        int m_nRows = 0;     // Maximum visible rows
};

class CDisView : public CView
{
    public:
        static const UINT INVALID_TARGET = 0U-1;

    public:
        CDisView (CWindow* pParent_);
        CDisView (const CDisView &) = delete;
        void operator= (const CDisView &) = delete;
        ~CDisView () { delete[] m_pszData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;

    public:
        static void DrawRegisterPanel (CScreen* pScreen_, int nX_, int nY_);

    protected:
        bool SetCodeTarget ();
        bool SetDataTarget ();
        bool cmdNavigate (int nKey_, int nMods_) override;

    private:
        UINT m_uRows = 0, m_uColumns = 0;
        UINT m_uCodeTarget = INVALID_TARGET;
        UINT m_uDataTarget = INVALID_TARGET;
        const char *m_pcszDataTarget = nullptr;
        char *m_pszData = nullptr;

        static WORD s_wAddrs[];
        static bool m_fUseSymbols;
};


class CTxtView : public CView
{
    public:
        CTxtView (CWindow* pParent_);
        CTxtView (const CTxtView &) = delete;
        void operator= (const CTxtView &) = delete;
        ~CTxtView () { delete[] m_pszData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;

    protected:
        bool cmdNavigate (int nKey_, int nMods_) override;

    private:
        int m_nRows = 0, m_nColumns = 0;
        char *m_pszData = nullptr;

        bool m_fEditing = false;
        WORD m_wEditAddr = 0;
};

class CHexView : public CView
{
    public:
        CHexView (CWindow* pParent_);
        CHexView (const CHexView &) = delete;
        void operator= (const CHexView &) = delete;
        ~CHexView () { delete[] m_pszData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;

    protected:
        bool cmdNavigate (int nKey_, int nMods_) override;

    private:
        int m_nRows = 0, m_nColumns = 0;
        char *m_pszData = nullptr;

        bool m_fEditing = false, m_fRightNibble = false;
        WORD m_wEditAddr = 0;
};

/*
class CMemView : public CView
{
    public:
        CMemView (CWindow* pParent_);

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
};
*/

class CGfxView : public CView
{
    public:
        CGfxView (CWindow* pParent_);
        CGfxView (const CGfxView &) = delete;
        void operator= (const CGfxView &) = delete;
        ~CGfxView () { delete[] m_pbData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;

    protected:
        bool cmdNavigate (int nKey_, int nMods_) override;

    protected:
        bool m_fGrid = true;
        UINT m_uStrips = 0, m_uStripWidth = 0, m_uStripLines = 0;
        BYTE *m_pbData = nullptr;

        static UINT s_uMode, s_uWidth, s_uZoom;
};

class CBptView : public CView
{
    public:
        CBptView (CWindow* pParent_);
        CBptView (const CBptView &) = delete;
        void operator= (const CBptView &) = delete;
        ~CBptView () { delete[] m_pszData; }

    public:
        void SetAddress (WORD wAddr_, bool fForceTop_=false) override;
        void Draw (CScreen* pScreen_) override;
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_) override;

    protected:
        bool cmdNavigate (int nKey_, int nMods_) override;

    private:
        int m_nRows = 0, m_nLines = 0, m_nTopLine = 0;
        int m_nActive = -1;
        char *m_pszData = nullptr;
};

class CTrcView final : public CTextView
{
    public:
        CTrcView (CWindow* pParent_);
        CTrcView (const CTrcView &) = delete;
        void operator= (const CTrcView &) = delete;

    public:
        void DrawLine (CScreen* pScreen_, int nX_, int nY_, int nLine_) override;
        bool cmdNavigate (int nKey_, int nMods_) override;
        void OnDblClick (int nLine_) override;
        void OnDelete () override;

    private:
        bool m_fFullMode = false;
};


class CDebugger final : public CDialog
{
    public:
        CDebugger (BREAKPT* pBreak_=nullptr);
        CDebugger (const CDebugger &) = delete;
        void operator= (const CDebugger &) = delete;
        ~CDebugger ();

        bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0) override;
        void OnNotify (CWindow* pWindow_, int nParam_) override;
        void EraseBackground (CScreen* pScreen_) override;
        void Draw (CScreen* pScreen_) override;

        void Refresh ();
        void SetSubTitle (const char *pcszSubTitle_);
        void SetAddress (WORD wAddr_);
        void SetView (ViewType nView);
        void SetStatus (const char *pcsz_, bool fOneShot_=false, const GUIFONT *pFont_=nullptr);
        void SetStatusByte (WORD wAddr_);
        bool Execute (const char* pcszCommand_);

    protected:
        ViewType m_nView = vtDis;
        CView *m_pView = nullptr;
        CEditControl *m_pCommandEdit = nullptr;
        CTextControl *m_pStatus = nullptr;

        std::string m_sStatus {};

        static bool s_fTransparent;
};


typedef bool (*PFNINPUTPROC)(EXPR *pExpr_);

class CInputDialog final : public CDialog
{
    public:
        CInputDialog (CWindow* pParent_, const char* pcszCaption_, const char* pcszPrompt_, PFNINPUTPROC pfn_);
        CInputDialog (const CInputDialog &) = delete;
        void operator= (const CInputDialog &) = delete;

    public:
        void OnNotify (CWindow* pWindow_, int nParam_) override;

    protected:
        CEditControl *m_pInput = nullptr;
        PFNINPUTPROC m_pfnNotify = nullptr;
};

#endif
