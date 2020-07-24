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


class FileDialog : public Dialog
{
public:
    FileDialog(const std::string& caption, const std::string& path_str,
        const FILEFILTER* pcFileFilter_, int* pnFilter_, Window* pParent_ = nullptr);
    FileDialog(const FileDialog&) = delete;
    void operator= (const FileDialog&) = delete;

public:
    void SetPath(const std::string& path);

public:
    void OnNotify(Window* pWindow_, int nParam_) override;
    virtual void OnOK() = 0;

protected:
    static bool s_fShowHidden;

protected:
    FileView* m_pFileView = nullptr;
    Window* m_pFile = nullptr;
    TextControl* m_pPath = nullptr;
    ComboBox* m_pFilter = nullptr;
    CheckBox* m_pShowHidden = nullptr;
    TextButton* m_pRefresh = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;

    const FILEFILTER* m_pcFileFilter = nullptr;
    int* m_pnFilter = nullptr;
};


class BrowseFloppy final : public FileDialog
{
public:
    BrowseFloppy(int nDrive_, Window* pParent_ = nullptr);
    void OnOK() override;

protected:
    int m_nDrive = 0;
};


class BrowseTape final : public FileDialog
{
public:
    BrowseTape(Window* pParent_ = nullptr);
    void OnOK() override;
};


class FileBrowser final : public FileDialog
{
public:
    FileBrowser(EditControl* pEdit_, Window* pParent_, const std::string& caption, const FILEFILTER* pcsFilter_, int* pnFilter_);
    FileBrowser(const FileBrowser&) = delete;
    void operator= (const FileBrowser&) = delete;

    void OnOK() override;

protected:
    EditControl* m_pEdit = nullptr;
};


class HDDProperties : public Dialog
{
public:
    HDDProperties(EditControl* pEdit_, Window* pParent_, const char* pcszCaption_);
    HDDProperties(const HDDProperties&) = delete;
    void operator= (const HDDProperties&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    EditControl* m_pEdit = nullptr;
    EditControl* m_pFile = nullptr;
    EditControl* m_pSize = nullptr;
    TextButton* m_pBrowse = nullptr;
    TextButton* m_pCreate = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class AboutDialog final : public Dialog
{
public:
    AboutDialog(Window* pParent_ = nullptr);
    AboutDialog(const AboutDialog&) = delete;
    void operator= (const AboutDialog&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;
    void EraseBackground(FrameBuffer& fb) override;

protected:
    Window* m_pCloseButton = nullptr;
};


class OptionView final : public ListView
{
public:
    OptionView(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
        : ListView(pParent_, nX_, nY_, nWidth_, nHeight_, 6) { }
};

class OptionsDialog final : public Dialog
{
public:
    OptionsDialog(Window* pParent_ = nullptr);
    OptionsDialog(const OptionsDialog&) = delete;
    void operator= (const OptionsDialog&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    ListView* m_pOptions = nullptr;
    TextButton* m_pClose = nullptr;
    TextControl* m_pStatus = nullptr;
};

class ImportDialog : public Dialog
{
public:
    ImportDialog(Window* pParent_ = nullptr);
    ImportDialog(const ImportDialog&) = delete;
    void operator= (const ImportDialog&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    EditControl* m_pFile = nullptr;
    EditControl* m_pAddr = nullptr;
    EditControl* m_pPage = nullptr;
    EditControl* m_pOffset = nullptr;
    TextButton* m_pBrowse = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
    RadioButton* m_pBasic = nullptr;
    RadioButton* m_pPageOffset = nullptr;
    FrameControl* m_pFrame = nullptr;

protected:
    static std::string s_filepath;
    static unsigned int s_uAddr, s_uPage, s_uOffset;
    static bool s_fUseBasic;
};

class ExportDialog final : public ImportDialog
{
public:
    ExportDialog(Window* pParent_ = nullptr);
    ExportDialog(const ExportDialog&) = delete;
    void operator= (const ExportDialog&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    EditControl* m_pLength = nullptr;
    static unsigned int s_uLength;
};


class NewDiskDialog final : public Dialog
{
public:
    NewDiskDialog(int nDrive_, Window* pParent_ = nullptr);
    NewDiskDialog(const NewDiskDialog&) = delete;
    void operator= (const NewDiskDialog&) = delete;

    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    ComboBox* m_pType = nullptr;
    CheckBox* m_pCompress = nullptr;
    CheckBox* m_pFormat = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;

protected:
    static std::string s_filepath;
    static unsigned int s_uType;
    static bool s_fCompress;
    static bool s_fFormat;
};
