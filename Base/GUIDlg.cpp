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
#include "Input.h"
#include "Options.h"


OPTIONS g_opts;

// Helper macro for detecting options changes
#define Changed(o)         (g_opts.o != GetOption(o))    
#define ChangedString(o)   (strcasecmp(g_opts.o, GetOption(o)))

////////////////////////////////////////////////////////////////////////////////

CAboutDialog::CAboutDialog (CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 285, 220,  "About SimCoupe")
{
    new CIconControl(this, 6, 6, &sSamIcon);
    new CTextControl(this, 61, 10,   "SimCoupe - a SAM Coupe emulator", YELLOW_8);
    new CTextControl(this, 61, 24,  "Version 0.90 beta 3", YELLOW_8);

    new CTextControl(this, 26, 46,  "Win32/SDL/Allegro versions and general overhaul:");
    new CTextControl(this, 36, 59,  "Simon Owen <simon.owen@simcoupe.org>", GREY_7);

    new CTextControl(this, 26, 78,  "Based on original DOS/X SimCoupe versions by:");
    new CTextControl(this, 36, 91,  "Allan Skillman <allan.skillman@arm.com>", GREY_7);

    new CTextControl(this, 26, 110,  "Additional technical enhancements:");
    new CTextControl(this, 36, 123,  "Dave Laundon <dave.laundon@simcoupe.org>", GREY_7);

    new CTextControl(this, 26, 142,  "Phillips SAA 1099 sound chip emulation:");
    new CTextControl(this, 36, 155,  "Dave Hooper <dave@rebuzz.org>", GREY_7);

    new CTextControl(this, 26, 177, "See ReadMe.txt for additional information.", YELLOW_8);

    m_pCloseButton = new CTextButton(this, 115, 199, "Close", 55);
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
    m_pFileView = new CFileView(this, 2, 2, (7*72)+19, (4*72));

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
    const char* pcszPath = m_pFileView->GetFullPath();

    if (pcszPath)
    {
        bool fInserted = false;

        // Insert the disk into the appropriate drive
        if (m_nDrive == 1)
            fInserted = pDrive1->Insert(SetOption(disk1, pcszPath));
        else
            fInserted = pDrive2->Insert(SetOption(disk2, pcszPath));

        // If we succeeded, show a status message and close the file selector
        if (fInserted)
        {
            // Update the status text and close the dialog
            Frame::SetStatus("%s  inserted into Drive %d", m_pFileView->GetItem()->m_pszLabel, m_nDrive);
            Destroy();
            return;
        }
    }

    // Report any error
    char szBody[MAX_PATH];
    sprintf(szBody, "%s:\n\nInvalid disk image!", m_pFileView->GetItem()->m_pszLabel);
    new CMessageBox(this, szBody, "Open Failed", mbWarning);
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

CTestDialog::CTestDialog (CWindow* pParent_/*=NULL*/)
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

#endif

////////////////////////////////////////////////////////////////////////////////


COptionsDialog::COptionsDialog (CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 364, 171, "Options")
{
    Move(m_nX, m_nY-40);

    m_pOptions = new COptionView(this, 2, 2, 360, 144);
    new CFrameControl(this, 0, m_nHeight-23, m_nWidth, 1);
    m_pStatus = new CTextControl(this, 4, m_nHeight-15, "", GREY_7);
    m_pClose = new CTextButton(this, m_nWidth-57, m_nHeight-19, "Close", 55);

    CListViewItem* pItem = NULL;

    // Add icons in reverse order to 
    pItem = new CListViewItem(&sSamIcon, "About", pItem);
    pItem = new CListViewItem(&sHardwareIcon, "Misc", pItem);
    pItem = new CListViewItem(&sPortIcon, "Parallel", pItem);
    pItem = new CListViewItem(&sDriveIcon, "Drives", pItem);
    pItem = new CListViewItem(&sKeyboardIcon, "Input", pItem);
    pItem = new CListViewItem(&sMidiIcon, "Midi", pItem);
    pItem = new CListViewItem(&sSoundIcon, "Sound", pItem);
    pItem = new CListViewItem(&sDisplayIcon, "Display", pItem);
    pItem = new CListViewItem(&sChipIcon, "System", pItem);
    m_pOptions->SetItems(pItem);

    // Set the initial status text
    OnNotify(m_pOptions,0);
}


class CSystemOptions : public CDialog
{
    public:
        CSystemOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "System Settings")
        {
            new CIconControl(this, 10, 10, &sChipIcon);

            new CFrameControl(this, 50, 17, 238, 45);
            new CTextControl(this, 60, 13, "Memory", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 35, "Main:");
            m_pMain = new CComboBox(this, 93, 32, "256K|512K", 60);
            new CTextControl(this, 167, 35, "External:");
            m_pExternal = new CComboBox(this, 217, 32, "None|1MB|2MB|3MB|4MB", 60);

            new CFrameControl(this, 50, 80, 238, 95);
            new CTextControl(this, 60, 76, "ROM images", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 100, "ROM 0:");
            m_pROM0 = new CEditControl(this, 107, 97, 165);

            new CTextControl(this, 63, 127, "ROM 1:");
            m_pROM1 = new CEditControl(this, 107, 125, 165);

            m_pFastReset = new CCheckBox(this, 107, 154, "Enable fast power-on reset.");

            new CTextControl(this, 61, 184, "Note: changes to the settings above require", GREY_7);
            new CTextControl(this, 61, 199, "a SAM reset to take effect.", GREY_7);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pMain->Select((GetOption(mainmem) >> 8) - 1);
            m_pExternal->Select(GetOption(externalmem));
            m_pROM0->SetText(GetOption(rom0));
            m_pROM1->SetText(GetOption(rom1));
            m_pFastReset->SetChecked(GetOption(fastreset));

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pMain,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(mainmem, (m_pMain->GetSelected()+1) << 8);
                SetOption(externalmem, m_pExternal->GetSelected());
                SetOption(rom0, m_pROM0->GetText());
                SetOption(rom1, m_pROM1->GetText());
                SetOption(fastreset, m_pFastReset->IsChecked());

                Destroy();
            }
        }

    protected:
        CCheckBox *m_pFastReset;
        CComboBox *m_pMain, *m_pExternal;
        CEditControl *m_pROM0, *m_pROM1;
        CTextButton *m_pOK, *m_pCancel;
};


class CDisplayOptions : public CDialog
{
    public:
        CDisplayOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 231, "Display Settings")
        {
            new CIconControl(this, 10, 10, &sDisplayIcon);
            new CFrameControl(this, 50, 17, 238, 185, WHITE);

            new CTextControl(this, 60, 13, "Settings", WHITE, BLUE_2);

            m_pFullScreen = new CCheckBox(this, 60, 35, "Full-screen");

                m_pScaleText = new CTextControl(this, 85, 57, "Windowed mode scale:");
                m_pScale = new CComboBox(this, 215, 54, "0.5x|1x|1.5x", 50);
                m_pDepthText = new CTextControl(this, 85, 79, "Full-screen colour depth:");
                m_pDepth = new CComboBox(this, 215, 76, "8-bit|16-bit|32-bit", 60);

            new CFrameControl(this, 63, 102, 212, 1, GREY_6);

            m_pStretch = new CCheckBox(this, 60, 115, "Stretch to fit");
            m_pSync = new CCheckBox(this, 60, 136, "Sync to 50Hz");
            m_pAutoFrameSkip = new CCheckBox(this, 60, 157, "Auto frame-skip");
            m_pScanlines = new CCheckBox(this, 165, 115, "Display scanlines");
            m_pRatio54 = new CCheckBox(this, 165, 136, "5:4 pixel shape");
            m_pFrameSkip = new CComboBox(this, 165, 154, "Show ALL frames|Show every 2nd|Show every 3rd|Show every 4th|Show every 5th|Show every 6th|Show every 7th|Show every 8th|Show every 9th|Show every 10th", 115);

            new CTextControl(this, 60, 180, "Viewable area:");
            m_pViewArea = new CComboBox(this, 140, 177, "No borders|Small borders|Short TV area (default)|TV visible area|Complete scan area", 140);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            int anDepths[] = { 0, 1, 2, 2 };
            m_pDepth->Select(anDepths[((GetOption(depth) >> 3) - 1) & 3]);
            m_pScale->Select(GetOption(scale)-1);

            m_pFullScreen->SetChecked(GetOption(fullscreen));
            m_pSync->SetChecked(GetOption(sync) != 0);
            m_pRatio54->SetChecked(GetOption(ratio5_4));
            m_pStretch->SetChecked(GetOption(stretchtofit));

            bool fScanlines = GetOption(scanlines) && !GetOption(stretchtofit);
            m_pScanlines->SetChecked(fScanlines);

            m_pAutoFrameSkip->SetChecked(!GetOption(frameskip));
            m_pFrameSkip->Select(GetOption(frameskip) ? GetOption(frameskip)-1 : 0);
            m_pViewArea->Select(GetOption(borders));

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pScale,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(fullscreen, m_pFullScreen->IsChecked());

                int anDepths[] = { 8, 16, 32 };
                SetOption(depth, anDepths[m_pDepth->GetSelected()]);
                SetOption(scale, m_pScale->GetSelected()+1);

                SetOption(sync, m_pSync->IsChecked());
                SetOption(ratio5_4, m_pRatio54->IsChecked());
                SetOption(stretchtofit, m_pStretch->IsChecked());
                SetOption(scanlines, m_pScanlines->IsChecked());

                int nFrameSkip = !m_pAutoFrameSkip->IsChecked();
                SetOption(frameskip, nFrameSkip ? m_pFrameSkip->GetSelected()+1 : 0);

                SetOption(borders, m_pViewArea->GetSelected());

                if (Changed(borders) || Changed(fullscreen) || (GetOption(fullscreen) && Changed(depth)))
                {
                    Frame::Init();

                    // Re-centre the window, including the parent if that's a dialog
                    if (GetParent()->GetType() == ctDialog)
                        reinterpret_cast<CDialog*>(GetParent())->Centre();
                    Centre();
                }

                Destroy();
            }
            else
            {
                bool fFullScreen = m_pFullScreen->IsChecked();
                m_pScaleText->Enable(!fFullScreen);
                m_pScale->Enable(!fFullScreen);
                m_pDepthText->Enable(fFullScreen);
                m_pDepth->Enable(fFullScreen);

                m_pFrameSkip->Enable(!m_pAutoFrameSkip->IsChecked());

                // SDL doesn't allow certain features to be changed at present
                m_pScaleText->Enable(false);
                m_pScale->Enable(false);
                m_pRatio54->Enable(false);
                m_pStretch->Enable(false);
                m_pScanlines->Enable(false);
            }
        }

    protected:
        CCheckBox *m_pFullScreen, *m_pStretch, *m_pSync, *m_pAutoFrameSkip, *m_pScanlines, *m_pRatio54;
        CComboBox *m_pScale, *m_pDepth, *m_pFrameSkip, *m_pViewArea;
        CTextControl *m_pScaleText, *m_pDepthText;
        CTextButton *m_pOK, *m_pCancel;
};


class CSoundOptions : public CDialog
{
    public:
        CSoundOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 231, "Sound Settings")
        {
            new CIconControl(this, 10, 10, &sSoundIcon);
            new CFrameControl(this, 50, 17, 238, 185, WHITE);

            m_pSound = new CCheckBox(this, 60, 13, "Sound enabled", WHITE, BLUE_2);

                m_pSAA = new CCheckBox(this, 70, 35, "Enable SAA 1099 chip output");

                    m_pFreqText = new CTextControl(this, 90, 57, "Frequency:");
                    m_pFreq = new CComboBox(this, 160, 54, "11025 Hz|22050 Hz|44100 Hz", 75);
                    m_pSampleSizeText = new CTextControl(this, 90, 79, "Sample size:");
                    m_pSampleSize = new CComboBox(this, 160, 76, "8-bit|16-bit", 60);

                m_pFilter = new CCheckBox(this, 70, 102, "Enable high quality filter");

            new CFrameControl(this, 63, 123, 212, 1, GREY_6);

            m_pBeeper = new CCheckBox(this, 60, 134, "Enable Spectrum-style beeper");
            m_pStereo = new CCheckBox(this, 60, 156, "Stereo sound");
            m_pLatencyText = new CTextControl(this, 60, 178, "Buffering (latency):");
            m_pLatency = new CComboBox(this, 165, 175, "1 frame (best)|2 frames|3 frames|4 frames|5 frames (default)|10 frames|15 frames|20 frames|25 frames", 115);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pSound->SetChecked(GetOption(sound));
            m_pSAA->SetChecked(GetOption(saasound));
            m_pFreq->Select(GetOption(freq)/11025 - 1);
            m_pSampleSize->Select((GetOption(bits) >> 3)-1);
            m_pFilter->SetChecked(GetOption(filter));
            m_pBeeper->SetChecked(GetOption(beeper));
            m_pStereo->SetChecked(GetOption(stereo));

            int nLatency = GetOption(latency);
            m_pLatency->Select((nLatency <= 5 ) ? nLatency - 1 : nLatency/5 + 3);

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pSound,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(sound, m_pSound->IsChecked());
                SetOption(saasound, m_pSAA->IsChecked());

                SetOption(freq, 11025 * (1 << m_pFreq->GetSelected()));
                SetOption(bits, (m_pSampleSize->GetSelected()+1) << 3);

                SetOption(filter, m_pFilter->IsChecked());
                SetOption(beeper, m_pBeeper->IsChecked());
                SetOption(stereo, m_pStereo->IsChecked());

                int nLatency = m_pLatency->GetSelected();
                SetOption(latency, (nLatency <= 5) ? nLatency + 1 : (nLatency - 3) * 5);

                if (Changed(sound) || Changed(saasound) || Changed(beeper) || Changed(freq) ||
                    Changed(bits) || Changed(stereo) || Changed(filter) || Changed(latency))
                {
                    Sound::Init();
                }

                if (Changed(beeper))
                    IO::InitBeeper();

                // If the sound was checked but the option isn't set, warn than it failed
                if (m_pSound->IsChecked() && !GetOption(sound))
                    new CMessageBox(GetParent(), "Sound init failed - device in use?", "Sound", mbWarning);

                Destroy();
            }
            else
            {
                bool fSound = m_pSound->IsChecked();

                m_pLatencyText->Enable(fSound);
                m_pLatency->Enable(fSound);
                m_pStereo->Enable(fSound);
                m_pSAA->Enable(fSound);
                m_pBeeper->Enable(fSound);

#ifdef USE_SAASOUND
                bool fSAA = fSound && m_pSAA->IsChecked();
#else
                bool fSAA = false;
                m_pSAA->Enable(false);
#endif
                m_pFreqText->Enable(fSAA);
                m_pFreq->Enable(fSAA);
                m_pSampleSizeText->Enable(fSAA);
                m_pSampleSize->Enable(fSAA);
                m_pFilter->Enable(fSAA);
            }
        }

    protected:
        CCheckBox *m_pSound, *m_pSAA, *m_pBeeper, *m_pFilter, *m_pStereo;
        CComboBox *m_pFreq, *m_pSampleSize, *m_pLatency;
        CTextControl *m_pLatencyText, *m_pFreqText, *m_pSampleSizeText;
        CTextButton *m_pOK, *m_pCancel;
};


class CMidiOptions : public CDialog
{
    public:
        CMidiOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "Midi Settings")
        {
            new CIconControl(this, 10, 15, &sMidiIcon);
            new CFrameControl(this, 50, 17, 238, 40);
            new CTextControl(this, 60, 13, "Active Device", YELLOW_8, BLUE_2);
            new CTextControl(this, 63, 33, "Device on MIDI port:");
            m_pMidi = new CComboBox(this, 170, 30, "None|Midi device|Network", 90);

            new CFrameControl(this, 50, 72, 238, 68);
            new CTextControl(this, 60, 68, "Devices", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 88, "MIDI Out:");
            m_pMidiOut = new CComboBox(this, 115, 85, "/dev/midi", 160);

            new CTextControl(this, 63, 115, "MIDI In:");
            m_pMidiIn = new CComboBox(this, 115, 113, "/dev/midi", 160);

            new CFrameControl(this, 50, 155, 238, 40);
            new CTextControl(this, 60, 151, "Network (not currently supported)", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 171, "Station ID:");
            m_pStationId = new CEditControl(this, 120, 168, 20, "0");

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pMidi->Select(GetOption(midi));

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pMidi,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(midi, m_pMidi->GetSelected());
                SetOption(midioutdev, m_pMidiOut->GetSelectedText());
                SetOption(midiindev, m_pMidiIn->GetSelectedText());

                if (Changed(midi) || Changed(midiindev) || Changed(midioutdev))
                    IO::InitMidi();

                Destroy();
            }
            else
            {
                int nType = m_pMidi->GetSelected();
                m_pMidiOut->Enable(nType == 1);
                m_pMidiIn->Enable(nType == 1);
                m_pStationId->Enable(nType == 2);

                m_pStationId->Enable(false);
            }
        }

    protected:
        CComboBox *m_pMidi, *m_pMidiOut, *m_pMidiIn;
        CEditControl *m_pStationId;
        CTextButton *m_pOK, *m_pCancel;
};


class CInputOptions : public CDialog
{
    public:
        CInputOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "Input Settings")
        {
            new CIconControl(this, 10, 10, &sKeyboardIcon);
            new CFrameControl(this, 50, 17, 238, 91);
            new CTextControl(this, 60, 13, "Keyboard", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 35, "Mapping mode:");
            m_pKeyMapping = new CComboBox(this, 145, 32, "None (raw)|SAM Coupe|Sinclair Spectrum", 115);

            m_pAltForCntrl = new CCheckBox(this, 63, 63, "Use Left-Alt for SAM Cntrl key.");
            m_pAltGrForEdit = new CCheckBox(this, 63, 85, "Use Alt-Gr for SAM Edit key.");

            new CIconControl(this, 10, 123, &sMouseIcon);
            new CFrameControl(this, 50, 125, 238, 37);
            new CTextControl(this, 60, 121, "Mouse", YELLOW_8, BLUE_2);

            m_pMouse = new CCheckBox(this, 63, 138, "Enable SAM mouse interface.");

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pKeyMapping->Select(GetOption(keymapping));
            m_pAltForCntrl->SetChecked(GetOption(altforcntrl));
            m_pAltGrForEdit->SetChecked(GetOption(altgrforedit));
            m_pMouse->SetChecked(GetOption(mouse));

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pMouse,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(keymapping, m_pKeyMapping->GetSelected());
                SetOption(altforcntrl, m_pAltForCntrl->IsChecked());
                SetOption(altgrforedit, m_pAltGrForEdit->IsChecked());
                SetOption(mouse, m_pMouse->IsChecked());

                if (Changed(keymapping) || Changed(mouse))
                    Input::Init();

                Destroy();
            }
        }

    protected:
        CComboBox *m_pKeyMapping;
        CCheckBox *m_pAltForCntrl, *m_pAltGrForEdit, *m_pMouse;
        CTextButton *m_pOK, *m_pCancel;
};


class CDriveOptions : public CDialog
{
    public:
        CDriveOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "Drive Settings")
        {
            new CIconControl(this, 10, 10, &sDriveIcon);

            new CFrameControl(this, 50, 16, 238, 79);
            new CTextControl(this, 60, 12, "Drive 1", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 30, "Drive connected:");
            m_pDrive1 = new CComboBox(this, 163, 27, "None|Floppy disk image|Device: /dev/fd0", 115);

            new CTextControl(this, 63, 53, "File:");
            m_pFile1 = new CEditControl(this, 90, 50, 188);
            m_pSave1 = new CTextButton(this, 195, 71, "Save", 40);
            m_pEject1 = new CTextButton(this, 238, 71, "Eject", 40);

            new CFrameControl(this, 50, 104, 238, 79);
            new CTextControl(this, 60, 100, "Drive 2", YELLOW_8, BLUE_2);

            new CTextControl(this, 63, 118, "Drive connected:");
            m_pDrive2 = new CComboBox(this, 163, 115, "None|Floppy disk image|Device: /dev/fd1|Atom hard disk", 115);

            new CTextControl(this, 63, 141, "File:");
            m_pFile2 = new CEditControl(this, 90, 138, 188);
            m_pSave2 = new CTextButton(this, 195, 159, "Save", 40);
            m_pEject2 = new CTextButton(this, 238, 159, "Eject", 40);

            new CFrameControl(this, 50, 189, 238, 25);
            m_pTurboLoad = new CCheckBox(this, 60, 196, "Fast disk access");
            new CTextControl(this, 165, 197, "Sensitivity:");
            m_pSensitivity = new CComboBox(this, 220, 193, "Low|Medium|High", 62);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pDrive1->Select(GetOption(drive1));
            m_pDrive2->Select(GetOption(drive2));
            m_pTurboLoad->SetChecked(GetOption(turboload) != 0);
            m_pSensitivity->Select(!GetOption(turboload) ? 1 : GetOption(turboload) <= 5 ? 2 :
                                                               GetOption(turboload) <= 50 ? 1 : 0);

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pDrive1,0);
            OnNotify(m_pDrive2,0);
            OnNotify(m_pTurboLoad,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                char sz[384]="";

                int anDriveTypes[] = { dskNone, dskImage, dskImage, dskAtom };
                SetOption(drive1, anDriveTypes[m_pDrive1->GetSelected()]);
                SetOption(drive2, anDriveTypes[m_pDrive2->GetSelected()]);

                SetOption(disk1, (m_pDrive1->GetSelected() == 2) ? OSD::GetFloppyDevice(1) : m_pFile1->GetText());
                SetOption(disk2, (m_pDrive2->GetSelected() == 2) ? OSD::GetFloppyDevice(2) : m_pFile2->GetText());

                int anSpeeds[] = { 100, 50, 5 };
                SetOption(turboload, m_pTurboLoad->IsChecked() ? anSpeeds[m_pSensitivity->GetSelected()] : 0);

                if (Changed(drive1) || Changed(drive2))
                    IO::InitDrives();

                if (*GetOption(disk1) && ChangedString(disk1) && !pDrive1->Insert(GetOption(disk1)))
                {
                    sprintf(sz, "%s\n\nNot a valid disk", GetOption(disk1));
                    new CMessageBox(this, sz, "Drive 1", mbWarning);
                    SetOption(disk1, "");
                }
                else if (*GetOption(disk2) && ChangedString(disk2) && !pDrive2->Insert(GetOption(disk2)))
                {
                    sprintf(sz, "%s\n\nNot a valid disk", GetOption(disk2));
                    new CMessageBox(this, sz, "Drive 2", mbWarning);
                    SetOption(disk2, "");
                }
                else
                    Destroy();
            }
            else if (pWindow_ == m_pTurboLoad)
                m_pSensitivity->Enable(m_pTurboLoad->IsChecked());
            else if (pWindow_ == m_pDrive1)
            {
                int nType = m_pDrive1->GetSelected();
                bool fFloppy = (nType == 1);

                m_pFile1->Enable(fFloppy);
                m_pFile1->SetText(!nType ? "" : (nType == 1) ? GetOption(disk1) : OSD::GetFloppyDevice(1));
                m_pSave1->Enable(fFloppy && pDrive1->IsModified());
                m_pEject1->Enable(fFloppy && pDrive1->IsInserted());
            }
            else if (pWindow_ == m_pDrive2)
            {
                int nType = m_pDrive2->GetSelected();
                bool fFloppy = (nType == 1);

                m_pFile2->Enable(fFloppy);
                m_pFile2->SetText(!nType ? "" : (nType == 1) ? GetOption(disk2) : OSD::GetFloppyDevice(2));
                m_pSave2->Enable(fFloppy && pDrive2->IsModified());
                m_pEject2->Enable(fFloppy && pDrive2->IsInserted());
            }
            else if (pWindow_ == m_pFile1)
            {
                m_pEject1->Enable(*m_pFile1->GetText() != 0);
                m_pSave1->Enable(false);
            }
            else if (pWindow_ == m_pFile2)
            {
                m_pEject2->Enable(*m_pFile2->GetText() != 0);
                m_pSave2->Enable(false);
            }
            else if (pWindow_ == m_pSave1)
            {
                pDrive1->Flush();
                m_pSave1->Enable(false);
            }
            else if (pWindow_ == m_pSave2)
            {
                pDrive2->Flush();
                m_pSave2->Enable(false);
            }
            else if (pWindow_ == m_pEject1)
            {
                m_pFile1->SetText("");
                m_pEject1->Enable(false);
            }
            else if (pWindow_ == m_pEject2)
            {
                m_pFile2->SetText("");
                m_pEject2->Enable(false);
            }
        }

    protected:
        CComboBox *m_pDrive1, *m_pDrive2, *m_pSensitivity;
        CEditControl *m_pFile1, *m_pFile2, *m_pMouse;
        CCheckBox* m_pTurboLoad;
        CTextButton *m_pSave1, *m_pEject1, *m_pSave2, *m_pEject2;
        CTextButton *m_pOK, *m_pCancel;
};


class CParallelOptions : public CDialog
{
    public:
        CParallelOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "Parallel Settings")
        {
            new CIconControl(this, 10, 10, &sPortIcon);
            new CFrameControl(this, 50, 17, 238, 91);
            new CTextControl(this, 60, 13, "Parallel Ports", YELLOW_8, BLUE_2);
            new CTextControl(this, 63, 33, "Devices connected to the parallel ports:");
            
            new CTextControl(this, 77, 57, "Port 1:");
            m_pPort1 = new CComboBox(this, 125, 54, "None|Printer|Mono DAC|Stereo DAC", 120);

            new CTextControl(this, 77, 82, "Port 2:");
            m_pPort2 = new CComboBox(this, 125, 79, "None|Printer|Mono DAC|Stereo DAC", 120);

            new CIconControl(this, 10, 113, &sPortIcon);
            new CFrameControl(this, 50, 120, 238, 79);

            new CTextControl(this, 60, 116, "Printer Device", YELLOW_8, BLUE_2);
            new CTextControl(this, 63, 136, "The following printer will be used for raw");
            new CTextControl(this, 63, 150, "SAM printer output:");
            
            m_pPrinter = new CComboBox(this, 63, 169, "<not currently supported>", 215);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pPort1->Select(GetOption(parallel1));
            m_pPort2->Select(GetOption(parallel2));

            // Update the state of the controls to reflect the current settings
            OnNotify(m_pPort1,0);
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(parallel1, m_pPort1->GetSelected());
                SetOption(parallel2, m_pPort2->GetSelected());
                SetOption(printerdev, "");

                if (Changed(parallel1) || Changed(parallel2) || ChangedString(printerdev))
                    IO::InitParallel();

                Destroy();
            }
            else
            {
                bool fPrinter1 = m_pPort1->GetSelected() == 1, fPrinter2 = m_pPort2->GetSelected() == 1;
                m_pPrinter->Enable(fPrinter1 || fPrinter2);
                m_pPrinter->Enable(false);
            }
        }

    protected:
        CComboBox *m_pPort1, *m_pPort2, *m_pPrinter;
        CTextButton *m_pOK, *m_pCancel;
};


class CMiscOptions : public CDialog
{
    public:
        CMiscOptions (CWindow* pParent_)
            : CDialog(pParent_, 300, 241, "Misc Settings")
        {
            new CIconControl(this, 10, 15, &sHardwareIcon);
            new CFrameControl(this, 50, 17, 238, 77);
            new CTextControl(this, 60, 13, "Clocks", YELLOW_8, BLUE_2);
            m_pSambus = new CCheckBox(this, 63, 32, "SAMBUS Clock");
            m_pDallas = new CCheckBox(this, 63, 52, "DALLAS Clock");
            m_pClockSync = new CCheckBox(this, 63, 72, "Advance SAM time relative to real time.");

            new CFrameControl(this, 50, 109, 238, 102);
            new CTextControl(this, 60, 105, "Miscellaneous", YELLOW_8, BLUE_2);
            m_pPauseInactive = new CCheckBox(this, 63, 124, "Pause the emulation when inactive.");
            m_pDriveLights = new CCheckBox(this, 63, 144, "Show disk drive LEDs.");
            m_pStatus = new CCheckBox(this, 63, 164, "Display status messages.");
            new CTextControl(this, 63, 187, "Profiling stats:");
            m_pProfile = new CComboBox(this, 140, 184, "Disabled|Speed and frame rate|Details percentages|Detailed timings", 140);

            m_pOK = new CTextButton(this, m_nWidth - 117, m_nHeight-21, "OK", 50);
            m_pCancel = new CTextButton(this, m_nWidth - 62, m_nHeight-21, "Cancel", 50);

            // Set the initial state from the options
            m_pSambus->SetChecked(GetOption(sambusclock));
            m_pDallas->SetChecked(GetOption(dallasclock));
            m_pClockSync->SetChecked(GetOption(clocksync));

            m_pPauseInactive->SetChecked(GetOption(pauseinactive));
            m_pDriveLights->SetChecked(GetOption(drivelights) != 0);
            m_pStatus->SetChecked(GetOption(status));

            m_pProfile->Select(GetOption(profile));
        }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_)
        {
            if (pWindow_ == m_pCancel)
                Destroy();
            else if (pWindow_ == m_pOK)
            {
                SetOption(sambusclock, m_pSambus->IsChecked());
                SetOption(dallasclock, m_pDallas->IsChecked());
                SetOption(clocksync, m_pClockSync->IsChecked());

                SetOption(pauseinactive, m_pPauseInactive->IsChecked());
                SetOption(drivelights, m_pDriveLights->IsChecked());
                SetOption(status, m_pStatus->IsChecked());

                SetOption(profile, m_pProfile->GetSelected());

                Destroy();
            }
        }

    protected:
        CCheckBox *m_pSambus, *m_pDallas, *m_pClockSync;
        CCheckBox *m_pPauseInactive, *m_pDriveLights, *m_pStatus;
        CComboBox *m_pProfile;
        CTextButton *m_pOK, *m_pCancel;
};


void COptionsDialog::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pClose)
        Destroy();
    else if (pWindow_ == m_pOptions)
    {
        const CListViewItem* pItem = m_pOptions->GetItem();
        if (pItem)
        {
            // Save the current options for change comparisons
            g_opts = Options::s_Options;

            if (!strcasecmp(pItem->m_pszLabel, "system"))
            {
                m_pStatus->SetText("Main/external memory configuration and ROM image paths.");
                if (nParam_) new CSystemOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "display"))
            {
                m_pStatus->SetText("Display settings for mode, depth, view size, etc.");
                if (nParam_) new CDisplayOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "sound"))
            {
                m_pStatus->SetText("Sound quality settings for SAA chip and beeper.");
                if (nParam_) new CSoundOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "midi"))
            {
                m_pStatus->SetText("MIDI settings for music and network.");
                if (nParam_) new CMidiOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "input"))
            {
                m_pStatus->SetText("Keyboard mapping and mouse settings.");
                if (nParam_) new CInputOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "drives"))
            {
                m_pStatus->SetText("Floppy drive setup, including Atom hard disk.");
                if (nParam_) new CDriveOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "parallel"))
            {
                m_pStatus->SetText("Parallel port settings for printer and DACs).");
                if (nParam_) new CParallelOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "misc"))
            {
                m_pStatus->SetText("Clock settings and miscellaneous front-end options.");
                if (nParam_) new CMiscOptions(this);
            }
            else if (!strcasecmp(pItem->m_pszLabel, "about"))
            {
                m_pStatus->SetText("Display SimCoupe version number and credits.");
                if (nParam_) new CAboutDialog(this);
            }
        }
    }
}
