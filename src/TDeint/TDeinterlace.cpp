/*
**                TDeinterlace v1.1 for Avisynth 2.5.x
**
**   TDeinterlace is a bi-directionally motion adaptive deinterlacer.
**   It also uses a couple modified forms of ela interpolation which 
**   help to reduce "jaggy" edges in places where interpolation must 
**   be used. TDeinterlace currently supports YV12 and YUY2 colorspaces.
**   
**   Copyright (C) 2004-2007 Kevin Stone
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

#include "TDeinterlace.h"

PVideoFrame __stdcall TDeinterlace::GetFrame(int n, IScriptEnvironment* env)
{
	if (n < 0) n = 0;
	else if (mode == 0 && n > nfrms) n = nfrms;
	else if (mode == 1 && n > nfrms2) n = nfrms2;
	PVideoFrame dst;
	bool wdtd = false;
	if (vi.IsYV12()) dst = GetFrameYV12(n, env, wdtd);
	else dst = GetFrameYUY2(n, env, wdtd);
	if (tshints && map != 1 && map != 2) 
	{
		env->MakeWritable(&dst);
		putHint2(dst, wdtd);
	}
	return dst;
}

void TDeinterlace::insertCompStats(int n, int norm1, int norm2, int mtn1, int mtn2)
{
	if (sa)
	{
		int pos = sa_pos*5;
		sa[pos+0] = n;
		sa[pos+1] = norm1;
		sa[pos+2] = norm2;
		sa[pos+3] = mtn1;
		sa[pos+4] = mtn2;
		sa_pos = (sa_pos+1)%500;
	}
}

int TDeinterlace::getMatch(int norm1, int norm2, int mtn1, int mtn2)
{
	float c1 = float(max(norm1,norm2))/float(max(min(norm1,norm2),1));
	float c2 = float(max(mtn1,mtn2))/float(max(min(mtn1,mtn2),1));
	float mr = float(max(mtn1,mtn2))/float(max(max(norm1,norm2),1));
	if (slow == 0)
	{
		if (((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
			((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
			((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
			((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
		{
			if (mtn1 > mtn2) return 1;
			return 0;
		}
	}
	else if (slow == 1)
	{
		if (((mtn1 >= 375  || mtn2 >= 375)  && (mtn1*3 < mtn2*1 || mtn2*3 < mtn1*1)) ||
			((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
			((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
			((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
			((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
		{
			if (mtn1 > mtn2) return 1;
			return 0;
		}
	}
	else
	{
		if (((mtn1 >= 250  || mtn2 >= 250)  && (mtn1*4 < mtn2*1 || mtn2*4 < mtn1*1)) ||
			((mtn1 >= 375  || mtn2 >= 375)  && (mtn1*3 < mtn2*1 || mtn2*3 < mtn1*1)) ||
			((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
			((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
			((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
			((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
		{
			if (mtn1 > mtn2) return 1;
			return 0;
		}
	}
	if (mr > 0.005 && max(mtn1,mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
	{
		if (mtn1 > mtn2) return 1;
		return 0;
	}
	else
	{
		if (norm1 > norm2) return 1;
		return 0;
	}
}

int TDeinterlace::getHint(PVideoFrame &src, unsigned int &storeHint, int &hintField)
{
	hintField = -1;
	const unsigned char *p = src->GetReadPtr(PLANAR_Y);
	unsigned int i, magic_number = 0, hint = 0;
	storeHint = 0xFFFFFFFF;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*p++ & 1) << i);
	}
	if (magic_number != 0xdeadbeef && 
		magic_number != 0xdeadfeed) return -1;
	for (i=0; i<32; ++i)
	{
		hint |= ((*p++ & 1) << i);
	}
	if (hint&0xFFFF0000) return -1;
	storeHint = hint;
	if (magic_number == 0xdeadbeef)
	{
		storeHint |= 0x00100000;
		if (hint&0x01) return 0;
		return 1;
	}
	if (hint&0x08) hintField = 1;
	else hintField = 0;
	if (hint&0x10) return 1;
	return 0;
}

void TDeinterlace::putHint(PVideoFrame &dst, unsigned int hint, int fieldt)
{
	int htype = (hint&0x00100000) ? 0 : 1;
	hint &= ~0x00100000;
	if (hint&0xFFFF0000) return;
	if (htype == 1) // tfm hint, modify it for tdecimate
	{
		hint &= 0xFFA0;
		if (fieldt == 1) hint |= 0x0E; // top + 'h'
		else hint |= 0x05; // bot + 'l'
	}
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	unsigned int i;
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		if (htype == 0) *p++ |= ((0xdeadbeef & (1 << i)) >> i);
		else *p++ |= ((0xdeadfeed & (1 << i)) >> i);
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((hint & (1 << i)) >> i);
	}
}

void TDeinterlace::putHint2(PVideoFrame &dst, bool wdtd)
{
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	unsigned int i, magic_number = 0, hint = 0;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*p++ & 1) << i);
	}
	if (magic_number == 0xdeadbeef)
	{
		for (i=0; i<32; ++i)
		{
			hint |= ((*p++ & 1) << i);
		}
		hint <<= 8;
		hint |= 0x80;
		if (wdtd) hint |= 0x40;
		magic_number = 0xdeadbead;
	}
	else if (magic_number == 0xdeadfeed) 
	{
		for (i=0; i<32; ++i)
		{
			hint |= ((*p++ & 1) << i);
		}
		if (wdtd) hint |= 0x40;
	}
	else
	{
		magic_number = 0xdeaddeed;
		if (wdtd) hint |= 0x40;
	}
	p = dst->GetWritePtr(PLANAR_Y);
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((magic_number & (1 << i)) >> i);
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((hint & (1 << i)) >> i);
	}
}

void TDeinterlace::InsertDiff(PVideoFrame &p1, PVideoFrame &p2, int n, int pos, IScriptEnvironment *env)
{
	if (db->fnum[pos] == n) return;
	absDiff(p1, p2, (PVideoFrame)NULL, pos, env);
	db->fnum[pos] = n;
}

void TDeinterlace::stackVertical(PVideoFrame &dst2, PVideoFrame &p1, PVideoFrame &p2,
		IScriptEnvironment *env)
{
	const int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	const int stop = vi.IsYV12() ? 3 : 1;
	for (int b=0; b<stop; ++b)
	{
		env->BitBlt(dst2->GetWritePtr(plane[b]),dst2->GetPitch(plane[b]),
			p1->GetReadPtr(plane[b]),p1->GetPitch(plane[b]),p1->GetRowSize(plane[b]),
			p1->GetHeight(plane[b]));
		env->BitBlt(dst2->GetWritePtr(plane[b])+dst2->GetPitch(plane[b])*p1->GetHeight(plane[b]),
			dst2->GetPitch(plane[b]),p2->GetReadPtr(plane[b]),p2->GetPitch(plane[b]),
			p2->GetRowSize(plane[b]),p2->GetHeight(plane[b]));
	}
}

TDeinterlace::TDeinterlace(PClip _child, int _mode, int _order, int _field, int _mthreshL, 
	int _mthreshC, int _map, const char* _ovr, int _ovrDefault, int _type, bool _debug, 
	int _mtnmode, bool _sharp, bool _hints, PClip _clip2, bool _full, int _cthresh, 
	bool _chroma, int _MI, bool _tryWeave, int _link, bool _denoise, int _AP, 
	int _blockx, int _blocky, int _APType, PClip _edeint, PClip _emask, int _metric,
	int _expand, int _slow, PClip _emtn, bool _tshints, int _opt, IScriptEnvironment* env) :
GenericVideoFilter(_child), mode(_mode), order(_order), field(_field), mthreshL(_mthreshL), 
	mthreshC(_mthreshC), map(_map), ovr(_ovr), ovrDefault(_ovrDefault), type(_type), 
	debug(_debug), mtnmode(_mtnmode), sharp(_sharp), hints(_hints), clip2(_clip2), full(_full),
	cthresh(_cthresh), chroma(_chroma), MI(_MI), tryWeave(_tryWeave), link(_link), 
	denoise(_denoise), AP(_AP), blockx(_blockx), blocky(_blocky), APType(_APType),
	edeint(_edeint), emask(_emask), metric(_metric), expand(_expand), slow(_slow), 
	emtn(_emtn), tshints(_tshints), opt(_opt)
{
	int z, w, q, b, i, track, count;
	char linein[1024];
	char *linep;
	FILE *f = NULL;
	input = cArray = NULL;
	tbuffer = NULL;
	db = NULL;
	sa = NULL;
	if (mode == 2)
	{
		mode = 1;
		if (type == 2 || mtnmode > 1 || tryWeave)
		{
			sa = (int *)malloc(5*500*sizeof(int));
			if (!sa) env->ThrowError("TDeint:  malloc failure (sa)!");
			for (int i=0; i<500; ++i) sa[i] = -1;
		}
	}
	if (!vi.IsYV12() && !vi.IsYUY2()) 
		env->ThrowError("TDeint:  YV12 and YUY2 data only!");
	if (mode != 0 && mode != 1 && mode != -1 && mode != -2)
		env->ThrowError("TDeint:  mode must be set to -2, -1, 0, or 1!");
	if (order != 0 && order != 1 && order != -1)
		env->ThrowError("TDeint:  order must be set to 0, 1, or -1!");
	if (field != 0 && field != 1 && field != -1)
		env->ThrowError("TDeint:  field must be set to 0, 1, or -1!");
	if (map < 0 || map > 4)
		env->ThrowError("TDeint:  map option must be set to 0, 1, 2, 3, or 4!");
	if (ovrDefault != 0 && ovrDefault != 1)
		env->ThrowError("TDeint:  ovrDefault must be set to either 0 or 1!");
	if (type < 0 || type > 5)
		env->ThrowError("TDeint:  type must be set to either 0, 1, 2, 3, 4, or 5!");
	if (mtnmode < 0 || mtnmode > 3)
		env->ThrowError("TDeint:  mtnmode must be set to either 0, 1, 2, or 3!");
	if (vi.height&1 || vi.width&1)
		env->ThrowError("TDeint:  width and height must be multiples of 2!");
	if (link < 0 || link > 3)
		env->ThrowError("TDeint:  link must be set to 0, 1, 2, or 3!");
	if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 && 
		blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
		env->ThrowError("TDeint:  illegal blockx size!");
	if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 && 
		blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
		env->ThrowError("TDeint:  illegal blocky size!");
	if (APType < 0 || APType > 2)
		env->ThrowError("TDeint:  APType must be set to 0, 1, or 2!");
	if (opt < 0 || opt > 4)
		env->ThrowError("TDeint:  opt must be set to 0, 1, 2, 3, or 4!");
	if (metric != 0 && metric != 1)
		env->ThrowError("TDeint:  metric must be set to 0 or 1!");
	if (expand < 0)
		env->ThrowError("TDeint:  expand must be greater than or equal to 0!");
	if (slow < 0 || slow > 2)
		env->ThrowError("TDeint:  slow must be set to 0, 1, or 2!");
	child->SetCacheHints(CACHE_RANGE, 5);
	useClip2 = false;
	if ((hints || !full) && mode == 0 && clip2)
	{
		const VideoInfo& vi1 = clip2->GetVideoInfo();
		if (vi1.height != vi.height || vi1.width != vi.width)
			env->ThrowError("TDeint:  width and height of clip2 must equal that of the input clip!");
		if (!vi1.IsYV12() && !vi1.IsYUY2())
			env->ThrowError("TDeint:  YV12 and YUY2 data only (clip2)!");
		if ((vi.IsYV12() && vi1.IsYUY2()) || (vi.IsYUY2() && vi1.IsYV12()))
			env->ThrowError("TDeint:  colorspace of clip2 doesn't match that of the input clip!");
		if (vi.num_frames != vi1.num_frames)
			env->ThrowError("TDeint:  number of frames in clip2 doesn't match that of the input clip!");
		useClip2 = true;
	}
	if (edeint)
	{
		const VideoInfo& vi1 = edeint->GetVideoInfo();
		if (vi1.height != vi.height || vi1.width != vi.width)
			env->ThrowError("TDeint:  width and height of edeint clip must equal that of the input clip!");
		if (!vi1.IsYV12() && !vi1.IsYUY2())
			env->ThrowError("TDeint:  YV12 and YUY2 data only (edeint)!");
		if ((vi.IsYV12() && vi1.IsYUY2()) || (vi.IsYUY2() && vi1.IsYV12()))
			env->ThrowError("TDeint:  colorspace of edeint clip doesn't match that of the input clip!");
		if ((mode == 0 && vi.num_frames != vi1.num_frames) || 
			(mode == 1 && vi.num_frames*2 != vi1.num_frames))
			env->ThrowError("TDeint:  number of frames in edeint clip doesn't match that of the input clip!");
		edeint->SetCacheHints(CACHE_NOTHING, 0);
	}
	if (emask)
	{
		const VideoInfo& vi1 = emask->GetVideoInfo();
		if (vi1.height != vi.height || vi1.width != vi.width)
			env->ThrowError("TDeint:  width and height of emask clip must equal that of the input clip!");
		if (!vi1.IsYV12() && !vi1.IsYUY2())
			env->ThrowError("TDeint:  YV12 and YUY2 data only (emask)!");
		if ((vi.IsYV12() && vi1.IsYUY2()) || (vi.IsYUY2() && vi1.IsYV12()))
			env->ThrowError("TDeint:  colorspace of emask clip doesn't match that of the input clip!");
		if ((mode == 0 && vi.num_frames != vi1.num_frames) || 
			(mode == 1 && vi.num_frames*2 != vi1.num_frames))
			env->ThrowError("TDeint:  number of frames in emask clip doesn't match that of the input clip!");
		emask->SetCacheHints(CACHE_NOTHING, 0);
	}
	if (emtn)
	{
		const VideoInfo& vi1 = emtn->GetVideoInfo();
		if (vi1.height != vi.height || vi1.width != vi.width)
			env->ThrowError("TDeint:  width and height of emtn clip must equal that of the input clip!");
		if (!vi1.IsYV12() && !vi1.IsYUY2())
			env->ThrowError("TDeint:  YV12 and YUY2 data only (emtn)!");
		if ((vi.IsYV12() && vi1.IsYUY2()) || (vi.IsYUY2() && vi1.IsYV12()))
			env->ThrowError("TDeint:  colorspace of emtn clip doesn't match that of the input clip!");
		if ((mode == 0 && vi.num_frames != vi1.num_frames) || 
			(mode == 1 && vi.num_frames*2 != vi1.num_frames))
			env->ThrowError("TDeint:  number of frames in emtn clip doesn't match that of the input clip!");
		emtn->SetCacheHints(CACHE_RANGE, 5);
	}
	sa_pos = 0;
	xhalf = blockx >> 1;
	yhalf = blocky >> 1;
	xshift = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
		blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 : 
		blockx == 1024 ? 10 : 11;
	yshift = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
		blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 : 
		blocky == 1024 ? 10 : 11;
	if (((!full && mode == 0) || tryWeave) && mode >= 0)
	{
		cArray = (int *)_aligned_malloc((((vi.width+xhalf)>>xshift)+1)*(((vi.height+yhalf)>>yshift)+1)*4*sizeof(int), 32);
		if (cArray == NULL) env->ThrowError("TDeint:  malloc failure!");
	}
	db = new TDBuf((mtnmode&1) ? 7 : mode==1 ? 4 : 3, vi.width, vi.height, vi.IsYV12() ? 3 : 1); 
	if (vi.IsYUY2())
	{
		xhalf *= 2;
		++xshift;
	}
	if (slow > 0)
	{
		if (vi.IsYV12())
		{
			tpitchy = (vi.width&15) ? vi.width+16-(vi.width&15) : vi.width;
			tpitchuv = ((vi.width>>1)&15) ? (vi.width>>1)+16-((vi.width>>1)&15) : (vi.width>>1);
		}
		else tpitchy = ((vi.width<<1)&15) ? (vi.width<<1)+16-((vi.width<<1)&15) : (vi.width<<1);
		tbuffer = (unsigned char*)_aligned_malloc((vi.height>>1)*tpitchy, 16);
		if (tbuffer == NULL)
			env->ThrowError("TDeinterlace:  malloc failure (tbuffer)!");
	}
	vi.SetFieldBased(false);
	nfrms = nfrms2 = vi.num_frames - 1;
	accumPn = accumNn = 0;
	accumPm = accumNm = 0;
	rmatch = -1;
	cthresh6 = cthresh * 6;
	passHint = 0xFFFFFFFF;
	autoFO = false;
	if (mode < 0) 
	{
		vi.height *= 2;
		field = 1;
	}
	if (order == -1) autoFO = true;
	if (mode == 1)
	{
		vi.num_frames *= 2;
		nfrms2 = vi.num_frames - 1;
		vi.SetFPS(vi.fps_numerator*2, vi.fps_denominator);
	}
	else if (field == -1)
	{
		// telecide matches off the bottom field so we want field=0 in that case.
		// tfm can match off top or bottom, but it will indicate which in its hints
		// and field is adjusted appropriately then... so we use field=0 by default
		// if hints=true.  Otherwise, if hints=false, we default to field = order.
		if (hints) field = 0;
		else field = order;
	}
	vi_saved = vi;
	if (map > 2) vi.height *= 2;
	orderS = order; 
	fieldS = field; 
	mthreshLS = mthreshL;
	mthreshCS = mthreshC;
	typeS = type;
	if (debug)
	{
		sprintf(buf,"TDeint:  %s (%s) by tritical\n", VERSION, DATE);
		OutputDebugString(buf);
		sprintf(buf,"TDeint:  mode = %d (%s)\n", mode, mode == 0 ? "normal - same rate" : 
				mode == 1 ? "bob - double rate" : mode == -2 ? "upsize - ELA" : "upsize - ELA-2");
		OutputDebugString(buf);
	}
	if (*ovr && mode >= 0)
	{
		countOvr = i = 0;
		if ((f = fopen(ovr, "r")) != NULL)
		{
			while(fgets(linein, 1024, f) != 0)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || 
					linein[0] == '#') continue;
				linep = linein;
				while (*linep != '-' && *linep != '+' && *linep != 0) *linep++;
				if (*linep == 0) ++countOvr;
				else if (*(linep+1) == '-' || *(linep+1) == '+') 
				{
					linep = linein;
					while (*linep != ',' && *linep != 0) *linep++;
					if (*linep == ',')
					{
						sscanf(linein, "%d,%d", &z, &w);
						if (z<0 || z>nfrms || w<0 || w>nfrms || w < z) 
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDeint: ovr input error (invalid frame range)!");
						}
						countOvr += (w - z + 1);
					}
					else 
					{
						fclose(f);
						f = NULL;
						env->ThrowError("TDeint:  ovr input error (invalid entry)!");
					}
				}
				else ++countOvr;
			}
			fclose(f);
			f = NULL;
			if (countOvr <= 0) return;
			++countOvr;
			countOvr *= 4;
			input = (int *)malloc(countOvr*sizeof(int));
			if (input == NULL) 
				env->ThrowError("TDeint: ovr input error (malloc failure)!");
			memset(input,255,countOvr*sizeof(int));
			if ((f = fopen(ovr, "r")) != NULL)
			{
				while (fgets(linein, 80, f) != NULL)
				{
					if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' ||
						linein[0] == ';' || linein[0] == '#') continue;
					linep = linein;
					while (*linep != ',' && *linep != 0 && *linep != ' ') *linep++;
					if (*linep == ',')
					{
						sscanf(linein, "%d,%d", &z, &w);
						if (w == 0) w = nfrms;
						if (z<0 || z>nfrms || w<0 || w>nfrms || w < z) 
						{
							free(input);
							input = NULL;
							fclose(f);
							f = NULL;
							env->ThrowError("TDeint: ovr input error (invalid frame range)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							if (*linep == 'f' || *linep == 'o' || *linep == 'l' || *linep == 'c' || *linep == 't')
							{
								q = *linep;
								linep += 2;
								if (*linep == 0) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (no change value specified)!");
								}
								sscanf(linep, "%d", &b);
								if (q == 102 && b != 0 && b != 1) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad field value)!");
								}
								else if (q == 111 && b != 0 && b != 1) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad order value)!");
								}
								else if (q == 116 && (b < 0 || b > 5)) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad type value)!");
								}
								input[i] = q; ++i;
								input[i] = z; ++i;
								input[i] = w; ++i;
								input[i] = b; ++i;
							}
							else if (*linep == '+' || *linep == '-') 
							{
								if (*(linep+1) == '+' || *(linep+1) == '-')
								{
									track = z; count = 0;
									while ((*linep == '+' || *linep == '-') && (track <= w))
									{
										q = *linep;
										input[i] = q; ++i;
										input[i] = track; ++i;
										input[i] = track; ++i; ++i;
										++count; ++track;
										*linep++;
									}
									while (track <= w)
									{
										input[i] = input[i-(count*4)]; ++i;
										input[i] = track; ++i;
										input[i] = track; ++i; ++i;
										++track;
									}
								}
								else
								{
									q = *linep;
									input[i] = q; ++i;
									input[i] = z; ++i;
									input[i] = w; ++i; ++i;
								}
							}
							else 
							{
								free(input);
								input = NULL;
								fclose(f);
								f = NULL;
								env->ThrowError("TDeint:  ovr input error (bad specifier)!");
							}
						}
						else 
						{
							free(input);
							input = NULL;
							fclose(f);
							f = NULL;
							env->ThrowError("TDeint:  ovr input error (no space after frame range)!");
						}
					}
					else if (*linep == ' ')
					{
						sscanf(linein, "%d", &z);
						if (z<0 || z>nfrms) 
						{
							free(input);
							input = NULL;
							fclose(f);
							f = NULL;
							env->ThrowError("TDeint: ovr input error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							q = *linep;
							input[i] = q; ++i;
							input[i] = z; ++i;
							input[i] = z; ++i;
							if (*linep == 'f' || *linep == 'o' || *linep == 'l' || *linep == 'c' || *linep == 't')
							{
								linep += 2;
								if (*linep == 0) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (no change value specified)!");
								}
								sscanf(linep, "%d", &b);
								if (q == 102 && b != 0 && b != 1) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad field value)!");
								}
								else if (q == 111 && b != 0 && b != 1) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad order value)!");
								}
								else if (q == 116 && (b < 0 || b > 5)) 
								{
									free(input);
									input = NULL;
									fclose(f);
									f = NULL;
									env->ThrowError("TDeint:  ovr input error (bad type value)!");
								}
								input[i] = b; ++i;
							}
							else if (*linep == '+' || *linep == '-') ++i;
							else 
							{
								free(input);
								input = NULL;
								fclose(f);
								f = NULL;
								env->ThrowError("TDeint:  ovr input error (bad specifier)!");
							}
						}
						else 
						{
							free(input);
							input = NULL;
							fclose(f);
							f = NULL;
							env->ThrowError("TDeint:  ovr input error (no space after frame number)!");
						}
					}
					else
					{
						free(input);
						input = NULL;
						fclose(f);
						f = NULL;
						env->ThrowError("TDeint:  ovr input error (invalid line)!");
					}
				}
				fclose(f);
				f = NULL;
			}
			else 
			{
				free(input);
				input = NULL;
				env->ThrowError("TDeint: ovr input error (cannot open file)!");
			}
		}
		else env->ThrowError("TDeint: ovr input error (cannot open file)!");
	}
}

TDeinterlace::~TDeinterlace()
{
	if (db) delete db;
	if (input != NULL) free(input);
	if (cArray != NULL) _aligned_free(cArray);
	if (sa) free(sa);
	if (tbuffer) _aligned_free(tbuffer);
}

AVSValue __cdecl Create_TDeinterlace(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	int mode = args[1].AsInt(0);
	if (mode < -2 || mode > 2)
		env->ThrowError("TDeint:  mode must be set to -2, -1, 0, 1, or 2!");
	int order = -1;
	int field = -1;
	int mthreshL = 6;
	int mthreshC = 6;
	int map = 0;
	char* ovr = "";
	int ovrDefault = 0;
	int type = 2;
	bool debug = false;
	int mtnmode = 1;
	bool sharp = true;
	bool hints = false;
	bool full = true;
	int cthresh = 6;
	bool chroma = false;
	int MI = 64;
	bool tryWeave = false;
	int link = 2;
	bool denoise = false;
	int AP = -1;
	int blockx = 16, blocky = 16;
	int APType = 1;
	if (args[0].IsClip())
	{
		unsigned int temp;
		int tfieldHint;
		if (!args[13].IsBool() &&
			TDeinterlace::getHint(args[0].AsClip()->GetFrame(0,env), temp, tfieldHint) != -1)
			hints = true;
	}
	PClip v;
	if (args[14].IsClip())
	{
		v = args[14].AsClip();
		try
		{ 
			v = env->Invoke("InternalCache", v).AsClip();
			v->SetCacheHints(CACHE_RANGE, 5);
		} 
		catch (IScriptEnvironment::NotFound) {  }
	}
	TDeinterlace *tdptr = new TDeinterlace(args[0].AsClip(),mode,args[2].AsInt(order),
		args[3].AsInt(field),args[4].AsInt(mthreshL),args[5].AsInt(mthreshC),args[6].AsInt(map),
		args[7].AsString(ovr),args[8].AsInt(ovrDefault),args[9].AsInt(type),args[10].AsBool(debug),
		args[11].AsInt(mtnmode),args[12].AsBool(sharp),args[13].AsBool(hints),args[14].IsClip() ? v : NULL,
		args[15].AsBool(full),args[16].AsInt(cthresh),args[17].AsBool(chroma),args[18].AsInt(MI),
		args[19].AsBool(tryWeave),args[20].AsInt(link),args[21].AsBool(denoise),args[22].AsInt(AP),
		args[23].AsInt(blockx),args[24].AsInt(blocky),args[25].AsInt(APType),
		args[26].IsClip()?args[26].AsClip():NULL,args[27].IsClip()?args[27].AsClip():NULL,
		args[29].AsInt(0),args[30].AsInt(0),args[31].AsInt(1),args[32].IsClip()?args[32].AsClip():NULL,
		args[33].AsBool(false),args[34].AsInt(4),env);
	AVSValue ret = tdptr;
	if (mode == 2)
	{
		try
		{ 
			ret = env->Invoke("InternalCache", ret.AsClip()).AsClip();
			ret.AsClip()->SetCacheHints(CACHE_RANGE, 3);
		} 
		catch (IScriptEnvironment::NotFound) {  }
		ret = new TDHelper(ret.AsClip(),args[2].AsInt(order),args[3].AsInt(field),
			args[28].AsFloat(-2.0),args[10].AsBool(debug),args[34].AsInt(4),tdptr->sa,
			args[31].AsInt(1),tdptr,env);
	}
	return ret;
}

AVSValue __cdecl Create_TSwitch(AVSValue args, void* user_data, IScriptEnvironment* env);

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env) 
{
    env->AddFunction("TDeint", "c[mode]i[order]i[field]i[mthreshL]i[mthreshC]i[map]i[ovr]s" \
		"[ovrDefault]i[type]i[debug]b[mtnmode]i[sharp]b[hints]b[clip2]c[full]b[cthresh]i" \
		"[chroma]b[MI]i[tryWeave]b[link]i[denoise]b[AP]i[blockx]i[blocky]i[APType]i[edeint]c" \
		"[emask]c[blim]f[metric]i[expand]i[slow]i[emtn]c[tshints]b[opt]i", Create_TDeinterlace, 0);
	env->AddFunction("TSwitch", "c[c1]c[c2]c[debug]b", Create_TSwitch, 0);
	return 0;
}