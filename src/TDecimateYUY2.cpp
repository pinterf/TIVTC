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

void TDecimate::drawBoxYUY2(PVideoFrame &dst, int blockN, int xblocks)
{
	unsigned char *dstp = dst->GetWritePtr();
	int pitch = dst->GetPitch();
	int width = dst->GetRowSize();
	int height = dst->GetHeight();
	int cordy, cordx, x, y, temp, xlim, ylim;
	cordy = blockN / xblocks;
	cordx = blockN - (cordy*xblocks);
	temp = cordx%4;
	cordx = (cordx>>2);
	cordy *= blocky;
	cordx *= (blockx<<1);
	if (temp == 1) cordx -= blockx;
	else if (temp == 2) cordy -= (blocky>>1);
	else if (temp == 3) { cordx -= blockx; cordy -= (blocky>>1); }
	xlim = cordx + 2*blockx;
	if (xlim > width) xlim = width;
	ylim = cordy + blocky;
	if (ylim > height) ylim = height;
	for (y=max(cordy,0), temp=cordx+2*(blockx-1); y<ylim; ++y)
	{
		if (cordx >= 0) (dstp+y*pitch)[cordx] = (dstp+y*pitch)[cordx] <= 128 ? 255 : 0;
		if (temp < width) (dstp+y*pitch)[temp] = (dstp+y*pitch)[temp] <= 128 ? 255 : 0;
	}
	for (x=max(cordx,0), temp=cordy+blocky-1; x<xlim; x+=4)
	{
		if (cordy >= 0)
		{
			(dstp+cordy*pitch)[x] = (dstp+cordy*pitch)[x] <= 128 ? 255 : 0;
			(dstp+cordy*pitch)[x+1] = 128;
			(dstp+cordy*pitch)[x+2] = (dstp+cordy*pitch)[x+2] <= 128 ? 255 : 0;
			(dstp+cordy*pitch)[x+3] = 128;
		}
		if (temp < height)
		{
			(dstp+temp*pitch)[x] = (dstp+temp*pitch)[x] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+1] = 128;
			(dstp+temp*pitch)[x+2] = (dstp+temp*pitch)[x+2] <= 128 ? 255 : 0;
			(dstp+temp*pitch)[x+3] = 128;
		}
	}
}

int TDecimate::DrawYUY2(PVideoFrame &dst, int x1, int y1, const char *s, int start) 
{
	int x, y = y1 * 20, num, pitch = dst->GetPitch();
	unsigned char *dp;
	unsigned int width = dst->GetRowSize();
	int height = dst->GetHeight();
	if (y+20 >= height) return -1;
	int count = 0;
	for (int xx = 0; *s; ++s, ++xx, ++count)
	{
		if (count < start) continue;
		x = (x1 + xx - start) * 10;
		if ((x+10)*2 >= (int)(width)) return -count-2;
		num = *s - ' ';
		for (int tx = 0; tx < 10; tx++) 
		{
			for (int ty = 0; ty < 20; ty++) 
			{
				dp = &dst->GetWritePtr()[(x + tx) * 2 + (y + ty) * pitch];
				if (font[num][ty] & (1 << (15 - tx))) 
				{
					if (tx & 1) 
					{
						dp[0] = 255;
						dp[-1] = 128;
						dp[1] = 128;
					} 
					else 
					{
						dp[0] = 255;
						dp[1] = 128;
						dp[3] = 128;
					}
				} 
				else 
				{
					if (tx & 1) 
					{
						dp[0] = (unsigned char) (dp[0] >> 1);
						dp[-1] = (unsigned char) ((dp[-1] + 128) >> 1);
						dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
					} 
					else 
					{
						dp[0] = (unsigned char) (dp[0] >> 1);
						dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
						dp[3] = (unsigned char) ((dp[3] + 128) >> 1);
					}
				}
			}
		}
	}
	return count;
}