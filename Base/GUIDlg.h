// Part of SimCoupe - A SAM Coupé emulator
//
// GUIDlg.h: Dialog boxes using the GUI controls
//
//  Copyright (c) 1999-2002  Simon Owen
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

#ifndef GUIDLG_H
#define GUIDLG_H

#include "GUI.h"


class CFileDialog : public CDialog
{
    public:
        CFileDialog (const char* pcszCaption_, const char* pcszPath_, CWindow* pParent_=NULL);

    public:
        void OnNotify (CWindow* pWindow_, int nParam_);
        virtual void OnOK () = 0;

    protected:
        CFileView* m_pFileView;
        CTextControl* m_pPath;
        CComboBox* m_pFilter;
        CCheckBox* m_pShowHidden;
        CTextButton *m_pRefresh, *m_pOK, *m_pCancel;
};


class CInsertFloppy : public CFileDialog
{
    public:
        CInsertFloppy (int nDrive_, CWindow* pParent_=NULL);
        void OnOK ();

    protected:
        int m_nDrive;
};


class CAboutDialog : public CDialog
{
    public:
        CAboutDialog (CWindow* pParent_=NULL);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CWindow* m_pCloseButton;
};


class CTestDialog : public CDialog
{
    public:
        CTestDialog(CWindow* pParent_=NULL);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CWindow *m_pEnable, *m_pClose, *m_apControls[12];
};

#endif // GUIDLG_H
