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

PVideoFrame TDecimate::GetFrameMode7(int n, IScriptEnvironment *env, int np)
{
	double ratio = fps/rate;
	int prev_f = int(double(n-1)*ratio+1.0);
	if (prev_f < 0) prev_f = 0;
	int curr1_f = int(double(n)*ratio);
	if (curr1_f > nfrms) mode2_decA[n] = nfrms;
	int curr2_f = int(double(n)*ratio+1.0);
	if (curr2_f > nfrms) mode2_decA[n] = nfrms;
	int next_f = int(double(n+1)*ratio);
	if (next_f > nfrms) next_f = nfrms;
	int curr_real = mode2_decA[n];
	if (curr_real != -20) { goto finish7; }
	int prev_real = n == 0 ? -20 : mode2_decA[n-1];
	if (prev_real != -20) prev_f = prev_real;
	bool rup = double(n)*ratio-double(curr1_f) >= 0.5 ? true : false;
	for (int i=max(prev_f-3,1); i<=min(next_f+2,nfrms); ++i)
	{
		if (metricsOutArray[i<<1] == ULLONG_MAX)
		{
			if (metricsArray != NULL && metricsArray[i<<1] != ULLONG_MAX)
				metricsOutArray[i<<1] = metricsArray[i<<1];
			else
			{
				int blockNI, blocksI;
				unsigned __int64 metricF;
				metricsOutArray[i<<1] = 
					calcMetric(child->GetFrame(i-1, env), child->GetFrame(i, env), 
						np, blockNI, blocksI, metricF, env, false);
			}
		}
	}
	int chosen = 0;
	if (same_group(curr1_f, curr2_f, env)) 
	{
		if (next_f-curr2_f > 1 && similar_group(prev_f, curr2_f, env) &&
			diff_group(next_f, next_f+1, env) && diff_group(curr2_f, curr2_f+1, env))
			chosen = 4;
		else chosen = 2;
	}
	else if (same_group(prev_f, curr1_f, env)) chosen = 1;
	else if (similar_group(prev_f, curr1_f, env))
	{
		if (similar_group(curr1_f, curr2_f, env) && !same_group(curr2_f, next_f, env)) 
			chosen = 3;
		else if (diff_group(curr1_f, curr2_f, env)) 
			chosen = 1;
	} 
	else if (diff_group(prev_f, curr1_f, env))
	{
		if (diff_group(curr2_f, next_f, env)) chosen = 3;
		else if (diff_group(curr1_f, curr2_f, env) && same_group(curr1_f-1, curr1_f, env) &&
			same_group(curr2_f, next_f, env) && diff_group(next_f, next_f+1, env) && 
			curr1_f-prev_f == 2 && diff_group(prev_f-1, prev_f, env))
			chosen = 1;
	}
	if (chosen == 4) mode2_decA[n] = curr2_f+1;
	else if (chosen >= 2) // either
	{
		if ((chosen == 2 && rup) || (chosen == 3 && 
			((metricsOutArray[curr2_f<<1]*2 > metricsOutArray[curr1_f<<1]*3) ||
			 (rup && metricsOutArray[curr2_f<<1]*3 >= metricsOutArray[curr1_f<<1]*2)))) 
			mode2_decA[n] = curr2_f;
		else mode2_decA[n] = curr1_f;
	}
	else if (chosen == 0) mode2_decA[n] = curr1_f;
	else mode2_decA[n] = curr2_f;
finish7:
	int ret = mode2_decA[n];
	if (ret < 0 || ret > nfrms)
		env->ThrowError("TDecimate:  mode 7 internal error!");
	if (debug)
	{
		sprintf(buf,"TDecimate:  ------------------------------------------\n");
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  inframe = %d  useframe = %d  chosen = %d\n", n, ret, chosen);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  prev = %d  curr1 = %d  curr2 = %d  next = %d\n", prev_f,
			curr1_f, curr2_f, next_f);
		OutputDebugString(buf);
		for (int i=max(0,ret-3); i<=min(ret+3,nfrms); ++i)
		{
			sprintf(buf,"TDecimate:  %d:  %3.2f  %I64u%s%s\n", i, double(metricsOutArray[i<<1])*100.0/double(MAX_DIFF),
				metricsOutArray[i<<1], metricsOutArray[i<<1] < same_thresh ? "  (D)" : 
				metricsOutArray[i<<1] > diff_thresh ? "  (N)" :
				aLUT[i] == 2 ? "  (N)" : aLUT[i] == 1 ? "  (S)" : 
				aLUT[i] == 0 ? "  (D)" : "", wasChosen(i, n) ? "  *" : "");
			OutputDebugString(buf);
		}
	}
	if (display)
	{
		PVideoFrame dst;
		if (!useclip2) dst = child->GetFrame(ret, env);
		else 
		{
			dst = clip2->GetFrame(ret, env);
			np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
		}
		env->MakeWritable(&dst);
		sprintf(buf, "TDecimate %s by tritical", VERSION);
		Draw(dst, 0, 0, buf, np);
		sprintf(buf, "Mode: 7  Rate = %3.6f", rate);
		Draw(dst, 0, 1, buf, np);
		sprintf(buf,"inframe = %d  useframe = %d  chosen = %d", n, ret, chosen);
		Draw(dst, 0, 2, buf, np);
		sprintf(buf,"p = %d  c1 = %d  c2 = %d  n = %d", prev_f,
			curr1_f, curr2_f, next_f);
		Draw(dst, 0, 3, buf, np);
		sprintf(buf,"dt = %3.2f  %I64u  vt = %3.2f  %I64u", dupThresh, same_thresh,
			vidThresh, diff_thresh);
		Draw(dst, 0, 4, buf, np);
		int pt = 5;
		for (int i=max(0,ret-3); i<=min(ret+3,nfrms); ++i)
		{
			sprintf(buf,"%d:  %3.2f  %I64u%s%s", i, double(metricsOutArray[i<<1])*100.0/double(MAX_DIFF),
				metricsOutArray[i<<1], metricsOutArray[i<<1] < same_thresh ? "  (D)" : 
				metricsOutArray[i<<1] > diff_thresh ? "  (N)" :
				aLUT[i] == 2 ? "  (N)" : aLUT[i] == 1 ? "  (S)" : 
				aLUT[i] == 0 ? "  (D)" : "", wasChosen(i, n) ? "  *" : "");
			Draw(dst, 0, pt++, buf, np);
		}
		return dst;
	}
	if (!useclip2) return child->GetFrame(ret, env);
	return clip2->GetFrame(ret, env);
}

bool TDecimate::wasChosen(int i, int n)
{
	for (int p=max(n-5,0); p<min(n+5,nfrmsN); ++p)
	{
		if (mode2_decA[p] == i) return true;
	}
	return false;
}

bool TDecimate::same_group(int f1, int f2, IScriptEnvironment *env) 
{
	return diff_f(f1, f2, env) <= 0;
}
  
bool TDecimate::similar_group(int f1, int f2, IScriptEnvironment *env)
{
	return diff_f(f1, f2, env) <= 1;
}

bool TDecimate::diff_group(int f1, int f2, IScriptEnvironment *env)
{
	return diff_f(f1, f2, env) == 2;
}

int TDecimate::diff_f(int f1, int f2, IScriptEnvironment *env) 
{
	int mx = 0;
	if (f2 < f1 || f2 < 0 || f1 > nfrms)
		env->ThrowError("TDecimate:  mode 7 internal error (f2 < f1)!");
	if (f1 < 0) f1 = 0;
	if (f2 > nfrms) f2 = nfrms;
	if (f1 == f2) 
	{
		if (aLUT[f2] == -20) 
			aLUT[f2] = mode7_analysis(f2, env);
		return 0;
	}
	for (int i=f1+1; i <= f2; ++i) 
	{
		if (aLUT[i] == -20) aLUT[i] = mode7_analysis(i, env);
		mx = max(mx, aLUT[i]);
	}
	return mx;
}

int TDecimate::mode7_analysis(int n, IScriptEnvironment *env)
{
	unsigned __int64 vals[3] = { ULLONG_MAX, ULLONG_MAX, ULLONG_MAX };
	if (n == 0) return 2;
	vals[0] = metricsOutArray[(n-1)<<1];
	vals[1] = metricsOutArray[n<<1];
	if (n != nfrms) vals[2] = metricsOutArray[(n+1)<<1];
	if (n == nfrms)
	{
		if (vals[0] == ULLONG_MAX || vals[1] == ULLONG_MAX)
			env->ThrowError("TDecimate:  mode 7 internal error (uncalculated metrics)!");
		if (vals[1] > diff_thresh || vals[1]*2 > vals[0]*3) return 2;
		else if (vals[1] < same_thresh || vals[1]*4 < vals[0] ||
			(vals[1]*2 < vals[0] && vals[0] > diff_thresh)) return 0;
		return 1;
	}
	if (vals[0] == ULLONG_MAX || vals[1] == ULLONG_MAX || vals[2] == ULLONG_MAX)
		env->ThrowError("TDecimate:  mode 7 internal error (uncalculated metrics)!");
	if (vals[1] > diff_thresh) return 2; // definitely different
	else if (vals[1] < same_thresh) return 0; // definitely the same
	else if (vals[1] < vals[0] && vals[1] < vals[2]) // local minimum difference
	{
		unsigned __int64 minn = min(vals[0],vals[2]);
		if (vals[1]*2 < minn && vals[0] > diff_thresh && vals[2] > diff_thresh) return 0;
		else if (vals[1]*4 < minn) return 0;
	}
	else if (vals[1] > vals[0] && vals[1] > vals[2]) // local maximum difference
	{
		unsigned __int64 maxn = max(vals[0],vals[2]);
		if (vals[1]*2 > maxn*3) return 2;
	}
	return 1;
}