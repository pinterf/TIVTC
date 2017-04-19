/*
**                    TIVTC v1.0.5 for Avisynth 2.5.x
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports YV12 and 
**   YUY2 colorspaces.
**   
**   Copyright (C) 2004-2008 Kevin Stone
**
**   This program is free software; you can redistribute it and/or modify
**   it under the terms of the GNU General Public License as published by
**   the Free Software Foundation; either version 2 of the License, or
**   (at your option) any later version.
**
**   This program is distributed in the hope that it will be useful,
**   but WITHOUT ANY WARRANTY; without even the implied warranty of
**   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**   GNU General Public License for more details.
**
**   You should have received a copy of the GNU General Public License
**   along with this program; if not, write to the Free Software
**   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
** This class stores all the individual cycle
** info for TDecimate and provides some useful methods.
** 
** For all of this class setting an int to -20 = nothing 
** (not set), except for type where -1 = nothing
**
**		  VIDEO TYPES
**	-1 = nothing (not set)
**   0 = film
**   1 = film by ovr
**   2 = video by matches
**   3 = video by metrics
**   4 = video by matches/metrics
**   5 = video by ovr
**
**        Blend Codes
**  -20 = not set
**    0 = no blending
**    1 = cvr - blend video cycle down
**    2 = cvr - video cycle w/ scenechange
**    3 = cvr/vfr - 2 dup cycle workaround
**
*/

#include <windows.h>
#include <stdio.h>
#include <limits.h>
#include "profUtil.h"

class IScriptEnvironment;

class Cycle
{
private:
	int cycleSize;
	bool Cycle::allocSpace();
	bool Cycle::checkMatchDup(int mp, int mc);

public:
	int sdlim;
	int length;		// length of cycle
	int maxFrame;	// nfrms
	int frame;		// first frame of cycle
	int frameE;		// last frame of cycle (frame + length)
	int offE;		// end offset
	int cycleS;		// 0 + start offset
	int cycleE;		// length - offE
	int frameSO;	// frame + cycleS
	int frameEO;	// frame + cycleE
	int type;		// video or film and how
	double *diffMetricsN;			// normalized metrics
	unsigned __int64 *diffMetricsU;	// unnormalized metrics
	unsigned __int64 *diffMetricsUF;	// frame metrics (scenechange detection)
	unsigned __int64 *tArray;			// used as temp storage when sorting
	int *dupArray;	// duplicate marking
	int *lowest;	// sorted list of metrics
	int *decimate;	// position of frames to drop
	int *decimate2;	// needed for some parts of longest string decimation
	int *match;		// frame matches (used for 30p identification)
	int *filmd2v;	// d2v trf flags indicate duplicate
	bool dupsSet;	// dups set
	bool mSet;		// metrics set
	bool lowSet;	// list sorted
	bool decSet;	// decimate array filled in
	bool isfilmd2v;	// d2v indicates duplicate in cycle
	int dupCount;	// tracks # of dups for longest string decimation
	int blend;		// 0, 1 (blending), 2 (mkv), others are hijacked for special handling
	int *dect, *dect2;
	void Cycle::setFrame(int frameIn);
	void Cycle::setDecimateLow(int num, IScriptEnvironment *env);
	void Cycle::setLowest(bool exludeD);
	void Cycle::setDups(double thresh);
	void Cycle::setDupsMatches(Cycle &p, unsigned char *marray);
	void Cycle::setDecimateLowP(int num, IScriptEnvironment *env);
	void Cycle::setIsFilmD2V();
	int Cycle::sceneDetect(unsigned __int64 thresh);
	int Cycle::sceneDetect(Cycle &prev, Cycle &next, unsigned __int64 thresh);
	int Cycle::getNonDec(int n);
	void Cycle::clearAll();
	void Cycle::debugOutput();
	void Cycle::debugMetrics(int length);
	Cycle::Cycle(int _size, int _sdlim);
	void Cycle::setSize(int _size);
	Cycle::~Cycle();
	Cycle& Cycle::operator=(Cycle& ob2);
};