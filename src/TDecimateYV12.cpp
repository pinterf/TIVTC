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

#include "TDecimate.h"

void TDecimate::drawBoxYV12(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr(PLANAR_Y);
	int width = dst->GetRowSize(PLANAR_Y);
	int height = dst->GetHeight(PLANAR_Y);
	int pitch = dst->GetPitch(PLANAR_Y);
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= blockx;
	if (temp == 1) cordx -= (blockx>>1);
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= (blockx>>1); cordy -= (blocky>>1); }
	xlim = cordx + blockx;
	if (xlim > width) xlim = width;
	ylim = cordy + blocky;
	if (ylim > height) ylim = height;
	for (y=max(cordy,0), temp=cordx+blockx-1; y<ylim; ++y)
	{
		if (cordx >= 0) (dstp+y*pitch)[cordx] = (dstp+y*pitch)[cordx] <= 128 ? 255 : 0;
		if (temp < width) (dstp+y*pitch)[temp] = (dstp+y*pitch)[temp] <= 128 ? 255 : 0;
	}
	for (x=max(cordx,0), temp=cordy+blocky-1; x<xlim; ++x)
	{
		if (cordy >= 0) (dstp+cordy*pitch)[x] = (dstp+cordy*pitch)[x] <= 128 ? 255 : 0;
		if (temp < height) (dstp+temp*pitch)[x] = (dstp+temp*pitch)[x] <= 128 ? 255 : 0;
	}
}

int TDecimate::DrawYV12(PVideoFrame &dst, int x1, int y1, const char *s, int start) 
{
	int x, y = y1 * 20, num, tx, ty, count = 0;
	int pitchY = dst->GetPitch(PLANAR_Y), pitchUV = dst->GetPitch(PLANAR_V);
	unsigned char *dpY, *dpU, *dpV;
	unsigned int width = dst->GetRowSize(PLANAR_Y);
	int height = dst->GetHeight(PLANAR_Y);
	if (y+20 >= height) return -1;
	for (int xx = 0; *s; ++s, ++xx, ++count)
	{
		if (count < start) continue;
		x = (x1 + xx - start) * 10;
		if (x+10 >= (int)(width)) return -count-2;
		num = *s - ' ';
		for (tx = 0; tx < 10; tx++)
		{
			for (ty = 0; ty < 20; ty++)
			{
				dpY = &dst->GetWritePtr(PLANAR_Y)[(x + tx) + (y + ty) * pitchY];
				if (font[num][ty] & (1 << (15 - tx))) *dpY = 255;
				else *dpY = (unsigned char) (*dpY >> 1);
			}
		}
		for (tx = 0; tx < 10; tx++)
		{
			for (ty = 0; ty < 20; ty++)
			{
				dpU = &dst->GetWritePtr(PLANAR_U)[((x + tx)/2) + ((y + ty)/2) * pitchUV];
				dpV = &dst->GetWritePtr(PLANAR_V)[((x + tx)/2) + ((y + ty)/2) * pitchUV];
				if (font[num][ty] & (1 << (15 - tx)))
				{
					*dpU = 128;
					*dpV = 128;
				} 
				else
				{
					*dpU = (unsigned char) ((*dpU + 128) >> 1);
					*dpV = (unsigned char) ((*dpV + 128) >> 1);
				}
			}
		}
	}
	return count;
}