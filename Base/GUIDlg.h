// Part of SimCoupe - A SAM Coupe emulator
//
// GUIDlg.h: Dialog boxes using the GUI controls
//
//  Copyright (c) 1999-2015 Simon Owen
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

#pragma once

#include "GUI.h"


struct FILEFILTER
{
    const char* pcszDesc;       // Strings describing the filters, separated by '|' symbols
    const char* pcszExts[10];   // Array of extensions for each string above, separated by ';' symbols
};


class CFileDialog : public CDialog
{
public:
    CFileDialog(const char* pcszCaption_, const char* pcszPath_,
        const FILEFILTER* pcFileFilter_, int* pnFilter_, CWindow* pParent_ = nullptr);
    CFileDialog(const CFileDialog&) = delete;
    void operator= (const CFileDialog&) = delete;

public:
    void SetPath(const char* pcszPath_);

public:
    void OnNotify(CWindow* pWindow_, int nParam_) override;
    virtual void OnOK() = 0;

protected:
    static bool s_fShowHidden;

protected:
    CFileView* m_pFileView = nullptr;
    CWindow* m_pFile = nullptr;
    CTextControl* m_pPath = nullptr;
    CComboBox* m_pFilter = nullptr;
    CCheckBox* m_pShowHidden = nullptr;
    CTextButton* m_pRefresh = nullptr;
    CTextButton* m_pOK = nullptr;
    CTextButton* m_pCancel = nullptr;

    const FILEFILTER* m_pcFileFilter = nullptr;
    int* m_pnFilter = nullptr;
};


class CInsertFloppy final : public CFileDialog
{
public:
    CInsertFloppy(int nDrive_, CWindow* pParent_ = nullptr);
    CInsertFloppy(const CInsertFloppy&) = delete;
    void operator= (const CInsertFloppy&) = delete;

    void OnOK() override;

protected:
    int m_nDrive = 0;
};


class CInsertTape final : public CFileDialog
{
public:
    CInsertTape(CWindow* pParent_ = nullptr);
    void OnOK() override;
};


class CFileBrowser final : public CFileDialog
{
public:
    CFileBrowser(CEditControl* pEdit_, CWindow* pParent_, const char* pcszCaption_, const FILEFILTER* pcsFilter_, int* pnFilter_);
    CFileBrowser(const CFileBrowser&) = delete;
    void operator= (const CFileBrowser&) = delete;

    void OnOK() override;

protected:
    CEditControl* m_pEdit = nullptr;
};


class CHDDProperties : public CDialog
{
public:
    CHDDProperties(CEditControl* pEdit_, CWindow* pParent_, const char* pcszCaption_);
    CHDDProperties(const CHDDProperties&) = delete;
    void operator= (const CHDDProperties&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CEditControl* m_pEdit = nullptr;
    CEditControl* m_pFile = nullptr;
    CEditControl* m_pSize = nullptr;
    CTextButton* m_pBrowse = nullptr;
    CTextButton* m_pCreate = nullptr;
    CTextButton* m_pOK = nullptr;
    CTextButton* m_pCancel = nullptr;
};


class CAboutDialog final : public CDialog
{
public:
    CAboutDialog(CWindow* pParent_ = nullptr);
    CAboutDialog(const CAboutDialog&) = delete;
    void operator= (const CAboutDialog&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;
    void EraseBackground(CScreen* pScreen_) override;

protected:
    CWindow* m_pCloseButton = nullptr;
};


class COptionView final : public CListView
{
public:
    COptionView(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
        : CListView(pParent_, nX_, nY_, nWidth_, nHeight_, 6) { }
};

class COptionsDialog final : public CDialog
{
public:
    COptionsDialog(CWindow* pParent_ = nullptr);
    COptionsDialog(const COptionsDialog&) = delete;
    void operator= (const COptionsDialog&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CListView* m_pOptions = nullptr;
    CTextButton* m_pClose = nullptr;
    CTextControl* m_pStatus = nullptr;
};

class CImportDialog : public CDialog
{
public:
    CImportDialog(CWindow* pParent_ = nullptr);
    CImportDialog(const CImportDialog&) = delete;
    void operator= (const CImportDialog&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CEditControl* m_pFile = nullptr;
    CEditControl* m_pAddr = nullptr;
    CEditControl* m_pPage = nullptr;
    CEditControl* m_pOffset = nullptr;
    CTextButton* m_pBrowse = nullptr;
    CTextButton* m_pOK = nullptr;
    CTextButton* m_pCancel = nullptr;
    CRadioButton* m_pBasic = nullptr;
    CRadioButton* m_pPageOffset = nullptr;
    CFrameControl* m_pFrame = nullptr;

protected:
    static char s_szFile[];
    static unsigned int s_uAddr, s_uPage, s_uOffset;
    static bool s_fUseBasic;
};

class CExportDialog final : public CImportDialog
{
public:
    CExportDialog(CWindow* pParent_ = nullptr);
    CExportDialog(const CExportDialog&) = delete;
    void operator= (const CExportDialog&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CEditControl* m_pLength = nullptr;
    static unsigned int s_uLength;
};


class CNewDiskDialog final : public CDialog
{
public:
    CNewDiskDialog(int nDrive_, CWindow* pParent_ = nullptr);
    CNewDiskDialog(const CNewDiskDialog&) = delete;
    void operator= (const CNewDiskDialog&) = delete;

    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CComboBox* m_pType = nullptr;
    CCheckBox* m_pCompress = nullptr;
    CCheckBox* m_pFormat = nullptr;
    CTextButton* m_pOK = nullptr;
    CTextButton* m_pCancel = nullptr;

protected:
    static char s_szFile[MAX_PATH];
    static unsigned int s_uType;
    static bool s_fCompress;
    static bool s_fFormat;
};


#ifdef _DEBUG

class CTestDialog final : public CDialog
{
public:
    CTestDialog(CWindow* pParent_ = nullptr);
    CTestDialog(const CTestDialog&) = delete;
    void operator= (const CTestDialog&) = delete;

public:
    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    CWindow* m_pEnable = nullptr;
    CWindow* m_pClose = nullptr;
    CWindow* m_apControls[32];
};

#endif  // _DEBUG
