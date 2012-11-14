#ifndef SDL12_H
#define SDL12_H

#include "Video.h"

class SDLVideo : public VideoBase
{
	public:
		SDLVideo ();
		~SDLVideo ();

	public:
		int GetCaps () const;
		bool Init (bool fFirstInit_);

		void Update (CScreen* pScreen_, bool *pafDirty_);
		void UpdateSize ();
		void UpdatePalette ();

		void DisplayToSamSize (int* pnX_, int* pnY_);
		void DisplayToSamPoint (int* pnX_, int* pnY_);

	protected:
		bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);

	private:
		SDL_Surface *pFront, *pBack, *pIcon;
		int nDesktopWidth, nDesktopHeight;

		SDL_Rect m_rTarget;
};

#endif // SDL12_H
