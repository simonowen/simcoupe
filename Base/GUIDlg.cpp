// Part of SimCoupe - A SAM Coupé emulator
//
// GUIDlg.cpp: Dialog boxes using the GUI controls
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

#include "SimCoupe.h"
#include "GUIDlg.h"

#include "CDisk.h"
#include "Frame.h"
#include "Options.h"

////////////////////////////////////////////////////////////////////////////////

CAboutDialog::CAboutDialog (CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 265, 184,  "About SimCoupe")
{
    new CIconControl(this, 6, 6, &sSamIcon);
    new CTextControl(this, 51, 8,   "SimCoupe - a SAM Coupe emulator", YELLOW_8);
    new CTextControl(this, 51, 22,  "Version 0.90 beta 2", YELLOW_8);
    new CTextControl(this, 16, 44,  "Win32/SDL/Allegro versions and general overhaul:");
    new CTextControl(this, 26, 57,  "Simon Owen (simon.owen@simcoupe.org)", GREY_7);
    new CTextControl(this, 16, 76,  "Based on original DOS/X SimCoupe versions by:");
    new CTextControl(this, 26, 89,  "Allan Skillman (allan.skillman@arm.com)", GREY_7);
    new CTextControl(this, 16, 108,  "Additional technical enhancements:");
    new CTextControl(this, 26, 121,  "Dave Laundon (dave.laundon@simcoupe.org)", GREY_7);
    new CTextControl(this, 16, 142, "See ReadMe.txt for additional information.", YELLOW_8);

    m_pCloseButton = new CTextButton(this, 105, 162, "Close", 50);
}

void CAboutDialog::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pCloseButton)
        Destroy();
}

////////////////////////////////////////////////////////////////////////////////

static const char* pcszDiskFilters =
    "All Disks (.dsk;.sad;.sdf;.sbt; .gz;.zip)|"
    "Disk Images (.dsk;.sad;.sdf;.sbt)|"
    "Compressed Files (.gz;.zip)|"
    "All Files";

static const char* apcszDiskFilters[] =
{
    ".dsk;.sad;.sdf;.sbt;.gz;.zip",
    ".dsk;.sad;.sdf;.sbt",
    ".gz;.zip",
    ""
};

CFileDialog::CFileDialog (const char* pcszCaption_, const char* pcszPath_, CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 527, 339, pcszCaption_)
{
    // Create all the controls for the dialog (the objects are deleted by the GUI when closed)
   (m_pFileView = new CFileView(this, 2, 2, (7*72)+19, (4*72)))->Activate();
    new CFrameControl(this, 0, (4*72)+3, m_nWidth, 1, GREY_6);
    new CTextControl(this, 3, m_nHeight-40,  "Path:");
    m_pPath = new CTextControl(this, 36, m_nHeight-40,  "");
    new CTextControl(this, 3, m_nHeight-19,  "Filter:");
    m_pFilter = new CComboBox(this, 36,m_nHeight-22, pcszDiskFilters, 200);
    m_pShowHidden = new CCheckBox(this, 252, m_nHeight-19, "Show hidden files");
    m_pRefresh = new CTextButton(this, m_nWidth - 160, m_nHeight-21, "Refresh", 56);
    m_pOK = new CTextButton(this, m_nWidth - 99, m_nHeight-21, "OK", 46);
    m_pCancel = new CTextButton(this, m_nWidth - 50, m_nHeight-21, "Cancel", 46);

    // Set the filter and path
    OnNotify(m_pFilter,0);
    m_pFileView->SetPath(pcszPath_);
}

// Handle control notifications
void CFileDialog::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pOK)
        m_pFileView->NotifyParent(1);
    else if (pWindow_ == m_pCancel)
        Destroy();
    else if (pWindow_ == m_pRefresh)
        m_pFileView->Refresh();
    else if (pWindow_ == m_pShowHidden)
        m_pFileView->ShowHidden(m_pShowHidden->IsChecked());
    else if (pWindow_ == m_pFilter)
        m_pFileView->SetFilter(apcszDiskFilters[m_pFilter->GetSelected()]);
    else if (pWindow_ == m_pFileView)
    {
        const CListViewItem* pItem = m_pFileView->GetItem();
        if (pItem)
        {
            // Folder notifications simply update the displayed path
            if (pItem->m_pIcon == &sFolderIcon)
                m_pPath->SetText(m_pFileView->GetPath());

            // Opening/double-clicking the file requires custom handling
            else if (nParam_)
                OnOK();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

CInsertFloppy::CInsertFloppy (int nDrive_, CWindow* pParent_/*=NULL*/)
    : CFileDialog("", NULL, pParent_), m_nDrive(nDrive_)
{
    // Set the dialog caption to show which drive we're dealing with
    char szCaption[32] = "Insert Floppy x";
    szCaption[strlen(szCaption)-1] = '0'+nDrive_;
    SetText(szCaption);

    // Browse from the location of the previous image, or the default directory if none
    const char* pcszImage = ((nDrive_ == 1) ? pDrive1 : pDrive2)->GetImage();
    m_pFileView->SetPath(*pcszImage ? pcszImage : OSD::GetFilePath());
}

// Handle OK being clicked when a file is selected
void CInsertFloppy::OnOK ()
{
    CDisk* pDisk;
    const char* pcszPath = m_pFileView->GetFullPath();
    if (pcszPath && (pDisk = CDisk::Open(pcszPath)))
    {
        // Insert the disk into the appropriate drive
        if (m_nDrive == 1)
            pDrive1->Insert(SetOption(disk1, pcszPath));
        else
            pDrive2->Insert(SetOption(disk2, pcszPath));

        // Update the status text and close the dialog
        Frame::SetStatus("%s  inserted into Drive 1", m_pFileView->GetItem()->m_pszLabel);
        Destroy();
    }
    else
    {
        char szBody[MAX_PATH];
        sprintf(szBody, "%s:\n\nInvalid disk image!", m_pFileView->GetItem()->m_pszLabel);
        new CMessageBox(this, szBody, "Open Failed", mbWarning);
    }
}

////////////////////////////////////////////////////////////////////////////////

CTestDialog::CTestDialog(CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 205, 198, "GUI Test")
{
    m_apControls[0] = new CEditControl(this, 8, 8, 190, "Edit control");

    m_apControls[1] = new CCheckBox(this, 8, 38, "Checked check-box");
    m_apControls[2] = new CCheckBox(this, 8, 54, "Unchecked check-box");
    reinterpret_cast<CCheckBox*>(m_apControls[1])->SetChecked();

    m_apControls[3] = new CRadioButton(this, 8, 78, "First option");
    m_apControls[4] = new CRadioButton(this, 8, 94, "Second option");
    m_apControls[5] = new CRadioButton(this, 8, 110, "Third option");
    reinterpret_cast<CRadioButton*>(m_apControls[3])->Select();

    m_apControls[6] = new CComboBox(this, 105,78, "Coch|Gwyn|Glas|Melyn", 70);
    m_apControls[7] = new CTextButton(this, 105, 103, "Button", 50);
    m_apControls[8] = new CScrollBar(this, 183,38,110,400);

    m_apControls[9] = new CIconControl(this, 8, 133, &sErrorIcon);

    m_apControls[10] = new CTextControl(this, 40, 133, "<- Icon control");
    m_apControls[11] = new CTextControl(this, 45, 149, "Coloured text control", GREEN_7);

    m_pEnable = new CCheckBox(this, 8, m_nHeight-20, "Controls enabled");
    reinterpret_cast<CCheckBox*>(m_pEnable)->SetChecked();

    m_pClose = new CTextButton(this, m_nWidth-55, m_nHeight-22, "Close", 50);

    m_pEnable->Activate();
}

void CTestDialog::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pClose)
        Destroy();
    else if (pWindow_ == m_pEnable)
    {
        bool fIsChecked = reinterpret_cast<CCheckBox*>(m_pEnable)->IsChecked();

        // Update the enabled/disabled state of the control so we can see what they look like
        for (int i = 0 ; i < 12 ; i++)
            m_apControls[i]->Enable(fIsChecked);
    }
}
