#ifndef DIRECTDRAW_H
#define DIRECTDRAW_H

#define DIRECTDRAW_VERSION 0x0300
#include <ddraw.h>

#include "Video.h"

class DirectDrawVideo : public VideoBase
{
	public:
		DirectDrawVideo ();
		~DirectDrawVideo ();

	public:
		int GetCaps () const;
		bool Init (bool fFirstInit_);

		void Update (CScreen* pScreen_, bool *pafDirty_);
		void UpdateSize () { }
		void UpdatePalette ();

		void DisplayToSamSize (int* pnX_, int* pnY_);
		void DisplayToSamPoint (int* pnX_, int* pnY_);

	protected:
		LPDIRECTDRAWSURFACE CreateSurface (DWORD dwCaps_, DWORD dwWidth_=0, DWORD dwHeight_=0, DWORD dwRequiredCaps_=0);
		bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);

	private:
		LPDIRECTDRAW m_pdd;
		LPDIRECTDRAWSURFACE m_pddsPrimary, m_pddsBack;
		LPDIRECTDRAWCLIPPER m_pddClipper;

		int m_nWidth, m_nHeight;
		RECT m_rTarget;
};

#endif // DIRECTDRAW_H
