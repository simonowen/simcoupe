// Part of SimCoupe - A SAM Coupe emulator
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


typedef struct
{
    const char* pcszDesc;       // Strings describing the filters, separated by '|' symbols
    const char* pcszExts[10];   // Array of extensions for each string above, separated by ';' symbols
}
FILEFILTER;


class CFileDialog : public CDialog
{
    public:
        CFileDialog (const char* pcszCaption_, const char* pcszPath_,
                     const FILEFILTER* pcFileFilter_, CWindow* pParent_=NULL);

    public:
        void OnNotify (CWindow* pWindow_, int nParam_);
        virtual void OnOK () = 0;

    protected:
        CFileView* m_pFileView;
        CTextControl* m_pPath;
        CComboBox* m_pFilter;
        CCheckBox* m_pShowHidden;
        CTextButton *m_pRefresh, *m_pOK, *m_pCancel;

        const FILEFILTER* m_pcFileFilter;
};


class CInsertFloppy : public CFileDialog
{
    public:
        CInsertFloppy (int nDrive_, CWindow* pParent_=NULL);
        void OnOK ();

    protected:
        int m_nDrive;
};


class CBrowseROM : public CFileDialog
{
    public:
        CBrowseROM (CEditControl* pEdit_, CWindow* pParent_=NULL);
        void OnOK ();

    protected:
        CEditControl* m_pEdit;
};


class CAboutDialog : public CDialog
{
    public:
        CAboutDialog (CWindow* pParent_=NULL);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CWindow* m_pCloseButton;
};


class COptionView : public CListView
{
    public:
        COptionView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
            : CListView (pParent_, nX_, nY_, nWidth_, nHeight_, 6) { }
};

class COptionsDialog : public CDialog
{
    public:
        COptionsDialog (CWindow* pParent_=NULL);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CListView* m_pOptions;
        CTextButton* m_pClose;
        CTextControl* m_pStatus;
};


class CTestDialog : public CDialog
{
    public:
        CTestDialog (CWindow* pParent_=NULL);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        CWindow *m_pEnable, *m_pClose, *m_apControls[12];
};

#endif // GUIDLG_H
