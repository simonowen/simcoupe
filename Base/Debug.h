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
#include "GUI.h"

class Debug
{
    public:
        static bool Start();
        static void Refresh ();

        static void OnRet ();

        static bool IsActive ();
        static bool IsBreakpointSet ();
        static bool BreakpointHit ();
};


class CDisassembly : public CWindow
{
    public:
        CDisassembly (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void SetAddress (WORD wAddr_);
        void Draw (CScreen* pScreen_);
};

class CRegisterPanel : public CWindow
{
    public:
        CRegisterPanel (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void Draw (CScreen* pScreen_);
};

class CCommandLine : public CEditControl
{
    public:
        CCommandLine (CWindow* pParent_, int nX_, int nY_, int nWidth_);

    public:
        bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        void Execute (const char* pcszCommand_);
};


class CDebugger : public CDialog
{
    public:
        CDebugger (CWindow* pParent_=NULL);
        ~CDebugger ();

        void OnNotify (CWindow* pWindow_, int nParam_);
        bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        void EraseBackground (CScreen* pScreen_);
        void Draw (CScreen* pScreen_);

    protected:
        void Refresh ();

    protected:
    public:
        CImageButton *m_pStepInto, *m_pStepOver, *m_pStepOut, *m_pStepToCursor;
        CTextButton *m_pClose;
        CButton *m_pTransparent;
        CCommandLine* m_pCmdLine;
        CDisassembly* m_pDisassembly;
        CRegisterPanel* m_pRegPanel;
};

#endif
