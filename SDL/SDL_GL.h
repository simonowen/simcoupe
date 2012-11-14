#ifndef SDL_GL_H
#define SDL_GL_H

#ifdef USE_OPENGL

#include "Video.h"

#define TEXTURE_SIZE    1024

enum { TEX_DISPLAY, TEX_SCANLINE, NUM_TEXTURES };


class OpenGLVideo : public VideoBase
{
	public:
		OpenGLVideo ();
		~OpenGLVideo ();

	public:
		int GetCaps () const;
		bool Init (bool fFirstInit_);

		void Update (CScreen* pScreen_, bool *pafDirty_);
		void UpdateSize ();
		void UpdatePalette ();

		void DisplayToSamSize (int* pnX_, int* pnY_);
		void DisplayToSamPoint (int* pnX_, int* pnY_);

	protected:
		bool Reset ();
		bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);
		void Render ();

	private:
		static int s_nDesktopWidth, s_nDesktopHeight;

	private:
		GLuint auTextures[NUM_TEXTURES];
		DWORD dwTextureData[NUM_TEXTURES][TEXTURE_SIZE][TEXTURE_SIZE];
		GLenum g_glPixelFormat, g_glDataType;

		SDL_Surface *pFront, *pIcon;

		SDL_Rect m_rTarget;
};

#endif // USE_OPENGL

#endif // SDL_GL_H
