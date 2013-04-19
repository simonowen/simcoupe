#include "SimCoupe.h"

#ifdef USE_OPENGL

#include "SDL_GL.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#define FULLSCREEN_DEPTH    16
#define glExtension(x)  !!strstr(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), (x))

static DWORD aulPalette[N_PALETTE_COLOURS];

int OpenGLVideo::s_nDesktopWidth, OpenGLVideo::s_nDesktopHeight;


OpenGLVideo::OpenGLVideo ()
    : g_glPixelFormat(0), g_glDataType(0), pFront(NULL), pIcon(NULL)
{
    m_rTarget.x = m_rTarget.y = m_rTarget.w = m_rTarget.h = 0;

    memset(auTextures, 0, sizeof(auTextures));
    memset(dwTextureData, 0, sizeof(dwTextureData));
}

OpenGLVideo::~OpenGLVideo ()
{
    if (auTextures[TEX_DISPLAY]) glDeleteTextures(NUM_TEXTURES, auTextures);
    auTextures[TEX_DISPLAY] = auTextures[TEX_SCANLINE] = 0;

    if (pFront) SDL_FreeSurface(pFront), pFront = NULL;
    if (pIcon) SDL_FreeSurface(pIcon), pIcon = NULL;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


int OpenGLVideo::GetCaps () const
{
    return VCAP_STRETCH | VCAP_FILTER | VCAP_SCANHIRES;
}

bool OpenGLVideo::Init (bool fFirstInit_)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        TRACE("SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
        return false;
    }

    pIcon = SDL_LoadBMP(OSD::MakeFilePath(MFP_EXE, "SimCoupe.bmp"));
    if (pIcon)
        SDL_WM_SetIcon(pIcon, NULL);

    // If not already set, store the native desktop resolution
    if (!s_nDesktopWidth)
    {
        const SDL_VideoInfo *pvi = SDL_GetVideoInfo();
        s_nDesktopWidth = pvi->current_w;
        s_nDesktopHeight = pvi->current_h;
    }

    return Reset() && UI::Init(fFirstInit_);
}

bool OpenGLVideo::Reset ()
{
    // Original frame
    int nWidth = Frame::GetWidth() / 2;
    int nHeight = Frame::GetHeight() / 2;

    // Apply window scaling
    if (!GetOption(scale)) SetOption(scale, 2);
    nWidth *= GetOption(scale);
    nHeight *= GetOption(scale);

    // Stretch width to 5:4 if enabled
    if (GetOption(ratio5_4)) nWidth = nWidth*5/4;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);

#ifndef WIN32
    // Request hardware acceleration support
    // Note: this gives a red screen with Catalyst ATI drivers on Win32
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#endif

    if (GetOption(fullscreen))
        pFront = SDL_SetVideoMode(s_nDesktopWidth, s_nDesktopHeight, FULLSCREEN_DEPTH, SDL_OPENGL|SDL_FULLSCREEN);
    else
        pFront = SDL_SetVideoMode(nWidth, nHeight, 0, SDL_OPENGL);

    if (!pFront)
    {
        TRACE("SDL_SetVideoMode() failed: %s", SDL_GetError());
        return false;
    }

#ifndef WIN32
    // Check the accleration variable is still set
    int accel = 0;
    if (SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &accel) == 0 && !accel)
    {
        TRACE("Not using OpenGL due to lack of hardware acceleration\n");
        return false;
    }
#endif

    // Use 16-bit packed pixel if available, otherwise 32-bit
    if (glExtension("GL_EXT_packed_pixels"))
        g_glPixelFormat = GL_RGBA, g_glDataType = GL_UNSIGNED_SHORT_5_5_5_1_EXT;
    else
        g_glPixelFormat = GL_RGBA, g_glDataType = GL_UNSIGNED_BYTE;

    // Calculate the scaled widths/heights and positions we might need
    int nFitWidth = nWidth * pFront->h / nHeight;
    int nFitHeight = nHeight * pFront->w / nWidth;
    int nFitX = (pFront->w - nFitWidth) / 2;
    int nFitY = (pFront->h - nFitHeight) / 2;

    // Scale to fill the width or the height, depending on which fits best
    if (nFitHeight > pFront->h)
        m_rTarget.x = nFitX, m_rTarget.y = 0, m_rTarget.w = nFitWidth, m_rTarget.h = pFront->h;
    else
        m_rTarget.x = 0, m_rTarget.y = nFitY, m_rTarget.w = pFront->w, m_rTarget.h = nFitHeight;

    // Set up a pixel units to avoid messing about when calculating positions
    glViewport(m_rTarget.x, m_rTarget.y, m_rTarget.w, m_rTarget.h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
//	glOrtho(0, Frame::GetWidth(), 0, Frame::GetHeight(), -1, 1);
    glOrtho(0, m_rTarget.w, 0, m_rTarget.h, -1, 1);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(NUM_TEXTURES, auTextures);

    // Determine the scanline alpha value for the current intensity
    Uint32 ulScanline = SDL_SwapLE32((GetOption(scanlevel) * 0xff / 100) << 24);

    // Create the scanline texture
    for (int i = 0 ; i < TEXTURE_SIZE ; i++)
        for (int j = 0 ; j < TEXTURE_SIZE ; j++)
            dwTextureData[TEX_SCANLINE][i][j] = (i&1) ? 0xff000000 : ulScanline;

    // Set the main display texture
    glBindTexture(GL_TEXTURE_2D, auTextures[TEX_DISPLAY]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_SIZE, TEXTURE_SIZE, 0, g_glPixelFormat, g_glDataType, dwTextureData[TEX_DISPLAY]);

    // Set the scanline texture
    glBindTexture(GL_TEXTURE_2D, auTextures[TEX_SCANLINE]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_SIZE, TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, dwTextureData[TEX_SCANLINE]);

    UpdatePalette();
    return true;
}

void OpenGLVideo::Render ()
{
    // Use the appropriate filter setting, depending on whether the GUI is active
    bool fFilter = GUI::IsActive() ? GetOption(filtergui) : GetOption(filter);
    GLint filter = fFilter ? GL_LINEAR : GL_NEAREST;

    glPushMatrix();
//	float flHeight = static_cast<float>(Frame::GetHeight());
    float flWidth = static_cast<float>(m_rTarget.w);
    float flHeight = static_cast<float>(m_rTarget.h);

    if (GUI::IsActive())
    {
        glScalef(1.0f, -1.0f, 1.0f);            // Flip vertically
        glTranslatef(0.0f, -flHeight, 0.0f);    // Centre image
    }
    else
    {
        glScalef(1.0f, -2.0f, 1.0f);            // Flip and double vertically
        glTranslatef(0.0f, -flHeight/2, 0.0f);  // Centre image
    }

    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, auTextures[TEX_DISPLAY]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);


    float tW = float(Frame::GetWidth()) / float(TEXTURE_SIZE);
    float tH = float(Frame::GetHeight()) / float(TEXTURE_SIZE);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f,   tH); glVertex2i(0,           m_rTarget.h);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(0,                     0);
    glTexCoord2f(  tW, 0.0f); glVertex2i(m_rTarget.w,           0);
    glTexCoord2f(  tW,   tH); glVertex2i(m_rTarget.w, m_rTarget.h);
    glEnd();

    glPopMatrix();

    if (GetOption(scanlines) && !GUI::IsActive())
    {
        glBindTexture(GL_TEXTURE_2D, auTextures[TEX_SCANLINE]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);

        if (GetOption(scanhires))
        {
            tW = float(m_rTarget.w) / float(TEXTURE_SIZE);
            tH = float(m_rTarget.h) / float(TEXTURE_SIZE);
        }

        // Stretch the texture over the full display width
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f,   tH); glVertex2i(0,           m_rTarget.h);
        glTexCoord2f(0.0f, 0.0f); glVertex2i(0,                     0);
        glTexCoord2f(  tW, 0.0f); glVertex2i(m_rTarget.w,           0);
        glTexCoord2f(  tW,   tH); glVertex2i(m_rTarget.w, m_rTarget.h);
        glEnd();

        glDisable(GL_BLEND);
    }

    glFlush();
    SDL_GL_SwapBuffers();
}

void OpenGLVideo::Update (CScreen* pScreen_, bool *pafDirty_)
{
    // Draw any changed lines to the back buffer
    if (!DrawChanges(pScreen_, pafDirty_))
        return;

    Render();
}

// Create whatever's needed for actually displaying the SAM image
void OpenGLVideo::UpdatePalette ()
{
    // Determine the scanline brightness level adjustment, in the range -100 to +100
    int nScanAdjust = GetOption(scanlines) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    const COLOUR *pSAM = IO::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR *p = &pSAM[i];
        BYTE r = p->bRed, g = p->bGreen, b = p->bBlue;

        // Set alpha to fully opaque
        BYTE a = 0xff;

        // 32-bit RGBA?
        if (g_glDataType == GL_UNSIGNED_BYTE)
            aulPalette[i] = SDL_SwapLE32((a << 24) | (b << 16) | (g << 8) | r);

        // 16-bit
        else
        {
            DWORD dwRMask, dwGMask, dwBMask, dwAMask;

            // The component masks depend on the data type
            if (g_glDataType == GL_UNSIGNED_SHORT_5_5_5_1_EXT)
                dwRMask = 0xf800, dwGMask = 0x07c0, dwBMask = 0x003e, dwAMask = 0x0001;
            else
                dwAMask = 0x8000, dwRMask = 0x7c00, dwGMask = 0x03e0, dwBMask = 0x001f;

            // Set native pixel value
            aulPalette[i] = SDL_SwapLE16(RGB2Native(r,g,b,a, dwRMask,dwGMask,dwBMask,dwAMask));
        }
    }

    // Ensure the display is redrawn to reflect the changes
    Video::SetDirty();
}


// OpenGL version of DisplayChanges
bool OpenGLVideo::DrawChanges (CScreen* pScreen_, bool *pafDirty_)
{
    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    int nRightHi = nWidth >> 3;
    int nRightLo = nRightHi >> 1;

    bool fInterlace = GetOption(scanlines) && !GUI::IsActive();
    if (fInterlace) nHeight >>= 1;

    bool *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    long lPitchDW = static_cast<long>(reinterpret_cast<DWORD*>(&dwTextureData[0][1]) - reinterpret_cast<DWORD*>(&dwTextureData[0][0]));

    DWORD *pdwBack, *pdw;
    pdw = pdwBack = reinterpret_cast<DWORD*>(&dwTextureData[TEX_DISPLAY]);

    // 16-bit?
    if (g_glDataType != GL_UNSIGNED_BYTE)
    {
        // Halve the pitch since we're dealing in WORD-sized pixels
        lPitchDW >>= 1;

        for (int y = 0 ; y < nHeight ; pb = pbSAM += lPitch, y++)
        {
            int x;

            if (!pafDirty_[y])
                ;
            else if (pfHiRes[y])
            {
                for (x = 0 ; x < nRightHi ; x++)
                {
                    pdw[0] = SDL_SwapLE32((aulPalette[pb[1]] << 16) | aulPalette[pb[0]]);
                    pdw[1] = SDL_SwapLE32((aulPalette[pb[3]] << 16) | aulPalette[pb[2]]);
                    pdw[2] = SDL_SwapLE32((aulPalette[pb[5]] << 16) | aulPalette[pb[4]]);
                    pdw[3] = SDL_SwapLE32((aulPalette[pb[7]] << 16) | aulPalette[pb[6]]);

                    pdw += 4;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < nRightLo ; x++)
                {
                    pdw[0] = aulPalette[pb[0]] * 0x00010001UL;
                    pdw[1] = aulPalette[pb[1]] * 0x00010001UL;
                    pdw[2] = aulPalette[pb[2]] * 0x00010001UL;
                    pdw[3] = aulPalette[pb[3]] * 0x00010001UL;
                    pdw[4] = aulPalette[pb[4]] * 0x00010001UL;
                    pdw[5] = aulPalette[pb[5]] * 0x00010001UL;
                    pdw[6] = aulPalette[pb[6]] * 0x00010001UL;
                    pdw[7] = aulPalette[pb[7]] * 0x00010001UL;

                    pdw += 8;
                    pb += 8;
                }
            }

            pdw = pdwBack += lPitchDW;
        }
    }
    else // 32-bit
    {
        for (int y = 0 ; y < nHeight ; pb = pbSAM += lPitch, y++)
        {
            int x;

            if (!pafDirty_[y])
                ;
            else if (pfHiRes[y])
            {
                for (x = 0 ; x < nRightHi ; x++)
                {
                    pdw[0] = aulPalette[pb[0]];
                    pdw[1] = aulPalette[pb[1]];
                    pdw[2] = aulPalette[pb[2]];
                    pdw[3] = aulPalette[pb[3]];
                    pdw[4] = aulPalette[pb[4]];
                    pdw[5] = aulPalette[pb[5]];
                    pdw[6] = aulPalette[pb[6]];
                    pdw[7] = aulPalette[pb[7]];

                    pdw += 8;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < nRightLo ; x++)
                {
                    pdw[0]  = pdw[1]  = aulPalette[pb[0]];
                    pdw[2]  = pdw[3]  = aulPalette[pb[1]];
                    pdw[4]  = pdw[5]  = aulPalette[pb[2]];
                    pdw[6]  = pdw[7]  = aulPalette[pb[3]];
                    pdw[8]  = pdw[9]  = aulPalette[pb[4]];
                    pdw[10] = pdw[11] = aulPalette[pb[5]];
                    pdw[12] = pdw[13] = aulPalette[pb[6]];
                    pdw[14] = pdw[15] = aulPalette[pb[7]];

                    pdw += 16;
                    pb += 8;
                }
            }

            pdw = pdwBack += lPitchDW;
        }
    }

    // Find the first changed display line
    int nChangeFrom = 0;
    for ( ; nChangeFrom < nHeight && !pafDirty_[nChangeFrom] ; nChangeFrom++);

    if (nChangeFrom < nHeight)
    {
        // Find the last change display line
        int nChangeTo = nHeight-1;
        for ( ; nChangeTo && !pafDirty_[nChangeTo] ; nChangeTo--);

        // Clear the dirty flags for the changed block
        for (int i = nChangeFrom ; i <= nChangeTo ; pafDirty_[i++] = false);

        // Offset and length of the change block
        int y = nChangeFrom, w = Frame::GetWidth(), h = nChangeTo-nChangeFrom+1;

        // Bind to the display texture
        glBindTexture(GL_TEXTURE_2D, auTextures[TEX_DISPLAY]);

        // Set up the data adjustments for the sub-image
        glPixelStorei(GL_UNPACK_ROW_LENGTH, TEXTURE_SIZE);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

        // Update the changed block
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, h, g_glPixelFormat, g_glDataType, dwTextureData[TEX_DISPLAY][0]);

        // Restore defaults, just in case
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }

    return true;
}

void OpenGLVideo::UpdateSize ()
{
    Reset();
}


// Map a native size/offset to SAM view port
void OpenGLVideo::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth;

    *pnX_ = *pnX_ * Frame::GetWidth()  / (m_rTarget.w << nHalfWidth);
    *pnY_ = *pnY_ * Frame::GetHeight() / (m_rTarget.h << nHalfHeight);
}

// Map a native client point to SAM view port
void OpenGLVideo::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= m_rTarget.x;
    *pnY_ -= m_rTarget.y;
    DisplayToSamSize(pnX_, pnY_);
}

#endif // USE_OPENGL
