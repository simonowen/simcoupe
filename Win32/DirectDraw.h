// Part of SimCoupe - A SAM Coupe emulator
//
// DirectDraw.h: DirectDraw display
//
//  Copyright (c) 2012-2015 Simon Owen
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
		LPDIRECTDRAW m_pdd = nullptr;
		LPDIRECTDRAWSURFACE m_pddsPrimary = nullptr;
        LPDIRECTDRAWSURFACE m_pddsBack = nullptr;
		LPDIRECTDRAWCLIPPER m_pddClipper = nullptr;

		int m_nWidth = 0, m_nHeight = 0;
		RECT m_rTarget {};
};

#endif // DIRECTDRAW_H
