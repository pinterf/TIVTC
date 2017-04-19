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

PVideoFrame __stdcall TDecimate::GetFrame(int n, IScriptEnvironment *env)
{
	if (n<0) n=0;
	else if (n>nfrmsN) n=nfrmsN;
	int np = child->GetVideoInfo().IsYV12() ? 3 : 1;
	PVideoFrame dst;
	if (mode < 2 ) dst = GetFrameMode01(n, env, np);     // most similar/longest string
	else if (mode == 2) dst = GetFrameMode2(n, env, np); // arbitrary framerate
	else if (mode == 3) dst = GetFrameMode3(n, env, np); // single pass mkv-vfr
	else if (mode == 4) dst = GetFrameMode4(n, env, np); // metrics output
	else if (mode == 5) dst = GetFrameMode5(n, env, np); // second pass of two pass hybrid
	else if (mode == 6) dst = GetFrameMode6(n, env, np); // second pass for 120fps to vfr
	else if (mode == 7) dst = GetFrameMode7(n, env, np); // arbitrary framerate v2
	else env->ThrowError("TDecimate:  unknown error (no such mode)!");
	if (usehints) restoreHint(dst, env);
	return dst;
}

void TDecimate::restoreHint(PVideoFrame &dst, IScriptEnvironment *env)
{
	const unsigned char *srcp = dst->GetReadPtr(PLANAR_Y);
	unsigned int i, hint = 0, magic_number = 0;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*srcp++ & 1) << i);
	}
	if (magic_number != MAGIC_NUMBER)
		return;
	for (i=0; i<32; ++i)
	{
		hint |= ((*srcp++ & 1) << i);
	}
	env->MakeWritable(&dst);
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	if (hint&0x80)
	{
		hint >>= 8;
		for (i=0; i<32; ++i)
		{
			*p &= ~1;
			*p++ |= ((MAGIC_NUMBER_2 & (1 << i)) >> i);
		}
		for (i=0; i<32; ++i)
		{
			*p &= ~1;
			*p++ |= ((hint & (1 << i)) >> i);
		}
	}
	else
	{
		for (int i=0; i<64; ++i) 
			*p++ &= ~1;
	}
}

PVideoFrame TDecimate::GetFrameMode01(int n, IScriptEnvironment* env, int np)
{
	int EvalGroup;
	if (hybrid != 3) EvalGroup = ((int)(n / (cycle-cycleR))) * cycle;
	else EvalGroup = ((int)(n / cycle)) * cycle;
	if (n != lastn+1 && EvalGroup >= cycle && fullInfo && (EvalGroup != curr.frame || 
		EvalGroup-cycle != prev.frame || EvalGroup+cycle != next.frame)) 
		rerunFromStart(EvalGroup, np, env);
	lastn = n;
	if (ecf) child->SetCacheHints(EvalGroup, -20);
	if (curr.frame != EvalGroup)
	{
		prev = curr;
		if (prev.frame != EvalGroup - cycle)
		{
			prev.setFrame(EvalGroup - cycle);
			getOvrCycle(prev, false);
			calcMetricCycle(prev, env, np, true, true);
			if (hybrid > 0)
			{
				checkVideoMatches(prev, prev);
				checkVideoMetrics(prev, vidThresh);
			}
			if (*output) addMetricCycle(prev);
		}
		curr = next;
		if (curr.frame != EvalGroup)
		{
			curr.setFrame(EvalGroup);
			getOvrCycle(curr, false);
			calcMetricCycle(curr, env, np, true, true);
			if (hybrid > 0)
			{
				checkVideoMatches(prev, curr);
				checkVideoMetrics(curr, vidThresh);
			}
			if (*output) addMetricCycle(curr);
		}
		next = nbuf;
		if (next.frame != EvalGroup + cycle) 
			next.setFrame(EvalGroup + cycle);
		getOvrCycle(next, false);
		calcMetricCycle(next, env, np, true, true);
		if (hybrid > 0)
		{
			checkVideoMatches(curr, next);
			checkVideoMetrics(next, vidThresh);
		}
		if (*output) addMetricCycle(next);
		nbuf.setFrame(EvalGroup+cycle*2);
		getOvrCycle(nbuf, false);
		if (hybrid > 0 && curr.type > 1)
		{
			int scenetest = curr.sceneDetect(prev, next, sceneThreshU);
			bool isVid = ((curr.type == 2 || curr.type == 4) && !curr.isfilmd2v && // matches
						  (prev.type == 5 || (prev.type == 2 && (vidDetect == 0 || vidDetect == 2)) || prev.type == 4 ||
						   next.type == 5 || (next.type == 2 && (vidDetect == 0 || vidDetect == 2)) || next.type == 4 || 
						   conCycle == 1 || scenetest != -20));
			bool isVid2 = ((curr.type == 3 || curr.type == 4) && !curr.isfilmd2v && // metrics
						  (prev.type == 5 || (prev.type == 3 && (vidDetect == 1 || vidDetect == 2)) || prev.type == 4 ||
						   next.type == 5 || (next.type == 3 && (vidDetect == 1 || vidDetect == 2)) || next.type == 4 || 
						   conCycle == 1 || scenetest != -20));
			if (curr.type == 5 || (vidDetect == 0 && isVid) || (vidDetect == 1 && isVid2) ||
			    (vidDetect == 2 && (isVid2 || isVid)) || (vidDetect == 3 && (isVid2 && isVid)))
			{
				int temp = curr.sceneDetect(prev, next, sceneThreshU);
				if (temp != -20 && hybrid != 3)
				{
					for (int p=curr.cycleS; p<curr.cycleE; ++p) curr.decimate[p] = curr.decimate2[p] = 0;
					curr.decimate[temp] = curr.decimate2[temp] = 1;
					curr.blend = 2;
					curr.decSet = true;
				}
				else curr.blend = 1;
			}
			else { goto novidjump; }
		}
		else
		{
novidjump:
			if (mode == 0)
			{
				mostSimilarDecDecision(prev, curr, next, env);
			}
			else
			{
				prev.setDups(dupThresh);
				curr.setDups(dupThresh);
				next.setDups(dupThresh);
				findDupStrings(prev, curr, next, env);
			}
			if (curr.blend == 3)
			{
				int tscene = curr.sceneDetect(prev, next, sceneThreshU);
				if (tscene != -20 && curr.decimate[tscene] == 1 && hybrid != 3)
				{
					curr.decimate[tscene] = curr.decimate2[tscene] = 0;
					curr.blend = 0;
				}
			}
			if (curr.blend != 3) curr.blend = 0;
		}
		if (debug) debugOutput1(n, curr.blend == 1 ? false : true, curr.blend);
	}
	for (int j=nbuf.cycleS; j<nbuf.cycleE; ++j)
	{
		if (nbuf.diffMetricsU[j] == ULLONG_MAX || nbuf.diffMetricsUF[j] == ULLONG_MAX ||
			nbuf.match[j] == -20)
		{
			calcMetricPreBuf(next.frameEO-1+j, next.frameEO+j, j, np, true, true, env);
			break;
		}
	}
	if (curr.blend == 3)  // 2 dups detected
	{
		if (hybrid == 3)  // blend up-convert (hybrid=3 leaves video untouched)
		{
			PVideoFrame dst;
			bool tsc = false;
			int tscene = curr.sceneDetect(prev, next, sceneThreshU);
			if (tscene == -20) 
			{
				tscene = next.sceneDetect(sceneThreshU);
				if (tscene == 0 && next.diffMetricsUF[next.cycleS] > sceneThreshU &&
					curr.sceneDetect(sceneThreshU) == -20) 
				{ 
					tscene = curr.length; 
					tsc = true;
				}
				else tscene = -20;
			}
			else if (tscene == 0 && curr.diffMetricsUF[curr.cycleS] > sceneThreshU) tsc = true;
			double a1, a2;
			int f1, f2;
			calcBlendRatios2(a1, a2, f1, f2, n, prev, curr, next, 2);
			if (a1 >= 1.0) 
			{
				if (!useclip2) dst = child->GetFrame(f1, env);
				else dst = clip2->GetFrame(f1, env);
				if (display) env->MakeWritable(&dst);
			}
			else if (a2 >= 1.0) 
			{
				if (!useclip2) dst = child->GetFrame(f2, env);
				else dst = clip2->GetFrame(f2, env);
				if (display) env->MakeWritable(&dst);
			}
			else if (tscene >= 0 && 
				((!tsc && (f1 == curr.frame+tscene || f2 == curr.frame+tscene+1)) || 
				 (tsc && (f1 == curr.frame+tscene-1 || f2 == curr.frame+tscene))))
			{
				if (!tsc)
				{
					f1 = curr.frame+tscene;
					f2 = curr.frame+tscene+1;
				}
				else
				{
					f1 = curr.frame+tscene-1;
					f2 = curr.frame+tscene;
				}
				a1 = 1.0;
				a2 = 0.0;
				if (!useclip2) dst = child->GetFrame(f1, env);
				else dst = clip2->GetFrame(f1, env);
				if (display) env->MakeWritable(&dst);
			}
			else 
			{
				if (!useclip2) dst = env->NewVideoFrame(child->GetVideoInfo());
				else dst = env->NewVideoFrame(clip2->GetVideoInfo());
				if (!useclip2)
					blendFrames(child->GetFrame(f1,env),child->GetFrame(f2,env),dst,a1,a2,np,env);
				else
					blendFrames(clip2->GetFrame(f1,env),clip2->GetFrame(f2,env),dst,a1,a2,
						clip2->GetVideoInfo().IsYV12()?3:1,env);
			}
			if (debug) debugOutput2(n, 0, true, f1, f2, a1, a2);
			if (display) displayOutput(env, dst, n, 0, true, a1, a2, f1, f2, 
				useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np);
			return dst;
		}
		// drop one dup and replace the other with a blend of its neighbors
		// (if noblend=false)... or if one is next to a scenechange then just
		// leave it (will be much less noticeable than a blend).
		int ret = n%(cycle-cycleR), y, f1 = 0, f2 = 0, jk;
		double a1 = 0.0, a2 = 0.0;
		int tscene = curr.sceneDetect(prev, next, sceneThreshU);
		if (tscene != -20)
		{
			for (jk=-1, y=curr.cycleS; y<curr.cycleE; ++y)
			{
				if (curr.decimate[y] == 0) ++jk;
				if (y == tscene && jk < ret) ++jk;
				if (ret == jk)
				{
					ret = y;
					break;
				}
			}
		}
		else
		{
			int d1 = -20, d2 = -20;
			for (y=curr.cycleS; y<curr.cycleE; ++y)
			{
				if (curr.decimate[y] == 1 && d1 == -20) d1 = y;
				else if (curr.decimate[y] == 1 && d2 == -20) { d2 = y; break; }
			}
			if (curr.diffMetricsU[d1] > curr.diffMetricsU[d2]) d1 = d2;
			for (jk=0, y=curr.cycleS; y<curr.cycleE; ++y)
			{
				if (ret == jk && y != d1)
				{
					if (curr.decimate[y] == 1)
					{
						f1 = curr.frameSO+y-1;
						f2 = curr.frameSO+y+1;
						a1 = a2 = 0.5;
					}
					else ret = y;
					break;
				}
				if (y != d1) ++jk;
			}
		}
		if (f1 != 0)
		{
			PVideoFrame dst;
			if (!useclip2) 
			{
				dst = env->NewVideoFrame(child->GetVideoInfo());
				blendFrames(child->GetFrame(f1,env),child->GetFrame(f2,env),dst,a1,a2,np,env);
			}
			else 
			{
				dst = env->NewVideoFrame(clip2->GetVideoInfo());
				np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
				blendFrames(clip2->GetFrame(f1,env),clip2->GetFrame(f2,env),dst,a1,a2,np,env);
			}
			if (display) displayOutput(env, dst, n, 0, true, a1, a2, f1, f2, np);
			if (debug) debugOutput2(n, 0, true, f1, f2, a1, a2);
			return dst;
		}
		if (debug) debugOutput2(n, curr.frame+ret, true, f1, f2, a1, a2);
		if (display) 
		{
			PVideoFrame dst;
			if (!useclip2) dst = child->GetFrame(curr.frame+ret, env);
			else 
			{
				dst = clip2->GetFrame(curr.frame+ret, env);
				np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
			}
			env->MakeWritable(&dst);
			displayOutput(env, dst, n, curr.frame+ret, true, a1, a2, f1, f2, np);
			return dst;
		}
		if (!useclip2) return child->GetFrame(curr.frame+ret, env);
		return clip2->GetFrame(curr.frame+ret, env);
	}
	else if (curr.blend != 1)  // normal film (1 dup)
	{
		if (hybrid == 3)  // blend up-convert (hybrid=3 leaves video untouched)
		{
			bool tsc = false;
			int tscene = curr.sceneDetect(prev, next, sceneThreshU);
			if (tscene == -20) 
			{
				tscene = next.sceneDetect(sceneThreshU);
				if (tscene == 0 && next.diffMetricsUF[next.cycleS] > sceneThreshU &&
					curr.sceneDetect(sceneThreshU) == -20) 
				{ 
					tscene = curr.length; 
					tsc = true;
				}
				else tscene = -20;
			}
			else if (tscene == 0 && curr.diffMetricsUF[curr.cycleS] > sceneThreshU) tsc = true;
			PVideoFrame dst;
			double a1, a2;
			int f1, f2;
			calcBlendRatios2(a1, a2, f1, f2, n, prev, curr, next, 1);
			if (a1 >= 1.0) 
			{
				if (!useclip2) dst = child->GetFrame(f1, env);
				else dst = clip2->GetFrame(f1, env);
				if (display) env->MakeWritable(&dst);
			}
			else if (a2 >= 1.0) 
			{
				if (!useclip2) dst = child->GetFrame(f2, env);
				else dst = clip2->GetFrame(f2, env);
				if (display) env->MakeWritable(&dst);
			}
			else if (tscene >= 0 && 
				((!tsc && (f1 == curr.frame+tscene || f2 == curr.frame+tscene+1)) || 
				(tsc && (f1 == curr.frame+tscene-1 || f2 == curr.frame+tscene))))
			{
				if (!tsc)
				{
					f1 = curr.frame+tscene;
					f2 = curr.frame+tscene+1;
				}
				else
				{
					f1 = curr.frame+tscene-1;
					f2 = curr.frame+tscene;
				}
				a1 = 1.0;
				a2 = 0.0;
				if (!useclip2) dst = child->GetFrame(f1, env);
				else dst = clip2->GetFrame(f1, env);
				if (display) env->MakeWritable(&dst);
			}
			else 
			{
				if (!useclip2) dst = env->NewVideoFrame(child->GetVideoInfo());
				else dst = env->NewVideoFrame(clip2->GetVideoInfo());
				if (!useclip2)
					blendFrames(child->GetFrame(f1,env),child->GetFrame(f2,env),dst,a1,a2,np,env);
				else
					blendFrames(clip2->GetFrame(f1,env),clip2->GetFrame(f2,env),dst,a1,a2,
						useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np,env);
			}
			if (debug) debugOutput2(n, 0, true, f1, f2, a1, a2);
			if (display) displayOutput(env, dst, n, 0, true, a1, a2, f1, f2,
				useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np);
			return dst;
		}
		// normal drop operation
		int ret = curr.getNonDec(n%(cycle-cycleR));
		if (ret == -1)
		{
			curr.debugOutput();
			curr.debugMetrics(curr.length);
			env->ThrowError("TDecimate:  major internal error.  Please report this to tritical ASAP!");
		}
		if (debug) debugOutput2(n, curr.frame+ret, curr.blend==2?false:true, 0, 0, 0.0, 0.0);
		if (display)
		{
			PVideoFrame dst;
			if (!useclip2) dst = child->GetFrame(curr.frame+ret, env);
			else 
			{
				dst = clip2->GetFrame(curr.frame+ret, env);
				np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
			}
			env->MakeWritable(&dst);
			displayOutput(env, dst, n, curr.frame+ret, curr.blend==2?false:true, 0.0, 0.0, 0, 0, np);
			return dst;
		}
		if (!useclip2) return child->GetFrame(curr.frame+ret, env);
		return clip2->GetFrame(curr.frame+ret, env);
	}
	else  // video (no dups)
	{
		if (hybrid == 3) // return source frame (hybrid=3 leaves video untouched)
		{
			if (debug) debugOutput2(n, n, false, 0, 0, 0.0, 0.0);
			if (display)
			{
				PVideoFrame dst;
				if (!useclip2) dst = child->GetFrame(n, env);
				else 
				{
					dst = clip2->GetFrame(n, env);
					np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
				}
				env->MakeWritable(&dst);
				displayOutput(env, dst, n, n, false, 0.0, 0.0, 0, 0, np);
				return dst;
			}
			if (!useclip2) return child->GetFrame(n, env);
			return clip2->GetFrame(n, env);
		}
		// blend down-convert (hybrid=1 leaves film untouched)
		PVideoFrame dst;
		double a1, a2;
		int f1, f2;
		calcBlendRatios(a1, a2, f1, f2, n, curr.frame, curr.cycleE-curr.cycleS);
		if (a1 >= 1.0) 
		{
			if (!useclip2) dst = child->GetFrame(f1, env);
			else dst = clip2->GetFrame(f1, env);
			if (display) env->MakeWritable(&dst);
		}
		else if (a2 >= 1.0) 
		{
			if (!useclip2) dst = child->GetFrame(f2, env);
			else dst = clip2->GetFrame(f2, env);
			if (display) env->MakeWritable(&dst);
		}
		else 
		{
			if (!useclip2) dst = env->NewVideoFrame(child->GetVideoInfo());
			else dst = env->NewVideoFrame(clip2->GetVideoInfo());
			if (!useclip2) 
				blendFrames(child->GetFrame(f1,env),child->GetFrame(f2,env),dst,a1,a2,np,env);
			else
				blendFrames(clip2->GetFrame(f1,env),clip2->GetFrame(f2,env),dst,a1,a2,
					clip2->GetVideoInfo().IsYV12() ? 3 : 1, env);
		}
		if (debug) debugOutput2(n, 0, false, f1, f2, a1, a2);
		if (display) displayOutput(env, dst, n, 0, false, a1, a2, f1, f2, 
			useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np);
		return dst;
	}
}

PVideoFrame TDecimate::GetFrameMode3(int n, IScriptEnvironment *env, int np)
{
	static int vidC = 0;
	static int filmC = 0;
	static int longestT = 0;
	static int longestV = 0;
	static int countVT = 0;
	static double timestamp = 0.0;
	if (n == 0) 
	{
		vidC = filmC = longestT = longestV = countVT = 0;
		timestamp = 0.0;
	}
	if (linearCount != n) env->ThrowError("TDecimate:  non-linear access detected in mode 3!");
	++linearCount;
	if (n == 0 || n-lastGroup == retFrames)
	{
		lastGroup = n;
		lastCycle += cycle;
		if (ecf) child->SetCacheHints(lastCycle, -20);
		prev = curr;
		if (prev.frame != lastCycle - cycle)
		{
			prev.setFrame(lastCycle - cycle);
			getOvrCycle(prev, false);
			calcMetricCycle(prev, env, np, true, true);
			checkVideoMatches(prev, prev);
			checkVideoMetrics(prev, vidThresh);
			if (*output) addMetricCycle(prev);
		}
		curr = next;
		if (curr.frame != lastCycle)
		{
			curr.setFrame(lastCycle);
			getOvrCycle(curr, false);
			calcMetricCycle(curr, env, np, true, true);
			checkVideoMatches(prev, curr);
			checkVideoMetrics(curr, vidThresh);
			if (*output) addMetricCycle(curr);
		}
		next = nbuf;
		if (next.frame != lastCycle + cycle)
			next.setFrame(lastCycle + cycle);
		getOvrCycle(next, false);
		calcMetricCycle(next, env, np, true, true);
		checkVideoMatches(curr, next);
		checkVideoMetrics(next, vidThresh);
		if (*output) addMetricCycle(next);
		nbuf.setFrame(lastCycle + cycle*2);
		getOvrCycle(nbuf, false);
		int scenetest = curr.sceneDetect(prev, next, sceneThreshU);
		bool isVid = ((curr.type == 2 || curr.type == 4) && !curr.isfilmd2v && // matches
					  (prev.type == 5 || (prev.type == 2 && (vidDetect == 0 || vidDetect == 2)) || prev.type == 4 ||
					   next.type == 5 || (next.type == 2 && (vidDetect == 0 || vidDetect == 2)) || next.type == 4 || 
					   conCycle == 1 || scenetest != -20));
		bool isVid2 = ((curr.type == 3 || curr.type == 4) && !curr.isfilmd2v && // metrics
					  (prev.type == 5 || (prev.type == 3 && (vidDetect == 1 || vidDetect == 2)) || prev.type == 4 ||
					   next.type == 5 || (next.type == 3 && (vidDetect == 1 || vidDetect == 2)) || next.type == 4 || 
					   conCycle == 1 || scenetest != -20));
		if (curr.type == 5 || (vidDetect == 0 && isVid) || (vidDetect == 1 && isVid2) ||
		    (vidDetect == 2 && (isVid2 || isVid)) || (vidDetect == 3 && (isVid2 && isVid)))
		{
			retFrames = cycle;
			vidC += (curr.frame+cycle <= nfrms ? cycle : nfrms-curr.frame+1);
			longestT += (curr.frame+cycle <= nfrms ? cycle : nfrms-curr.frame+1);
			if (!tcfv1)
			{
				int stop = (lastCycle+cycle <= nfrms ? cycle : nfrms-lastCycle+1);
				for (int u=0; u<stop; ++u) 
				{
					fprintf(mkvOutF, "%3.6f\n", timestamp);
					timestamp += 1000.0/fps;
				}
			}
		}
		else
		{
			if (vfrDec != 1)
			{
				mostSimilarDecDecision(prev, curr, next, env);
			}
			else
			{
				prev.setDups(dupThresh);
				curr.setDups(dupThresh);
				next.setDups(dupThresh);
				findDupStrings(prev, curr, next, env);
			}
			filmC += (curr.frame+cycle <= nfrms ? cycle : nfrms-curr.frame+1);
			if (retFrames == cycle)
			{
				if (longestT > longestV) longestV = longestT;
				++countVT;
				longestT = 0;
			}
			if (curr.blend != 3) 
			{
				if (!tcfv1)
				{
					int stop = (lastCycle+cycle <= nfrms ? cycle-cycleR : nfrms-lastCycle+1-cycleR);
					for (int u=0; u<stop; ++u) 
					{
						fprintf(mkvOutF, "%3.6f\n", timestamp);
						timestamp += 1000.0/mkvfps;
					}
				}
				retFrames = cycle-cycleR;
			}
			else 
			{
				if (lastType > 0 && tcfv1)
					fprintf(mkvOutF, "%d,%d,%4.6f\n", lastGroup-(lastType*(cycle-cycleR)),lastGroup-1,mkvfps);
				if (!tcfv1)
				{
					int stop = (lastCycle+cycle <= nfrms ? cycle-cycleR-1 : nfrms-lastCycle+1-cycleR-1);
					for (int u=0; u<stop; ++u) 
					{
						fprintf(mkvOutF, "%3.6f\n", timestamp);
						timestamp += 1000.0/mkvfps2;
					}
				}
				else fprintf(mkvOutF, "%d,%d,%4.6f\n", lastGroup, lastGroup+cycle-cycleR-2, mkvfps2);
				retFrames = cycle-cycleR-1;
			}
		}
		if (retFrames == cycle && lastType > 0 && tcfv1)
			fprintf(mkvOutF, "%d,%d,%4.6f\n", lastGroup-(lastType*(cycle-cycleR)),lastGroup-1,mkvfps);
		if (retFrames == cycle-cycleR) ++lastType;
		else lastType = 0;
		if (debug) debugOutput1(n, retFrames == cycle ? false : true, curr.blend);
	}
	for (int j=nbuf.cycleS; j<nbuf.cycleE; ++j)
	{
		if (nbuf.diffMetricsU[j] == ULLONG_MAX || nbuf.diffMetricsUF[j] == ULLONG_MAX ||
			nbuf.match[j] == -20)
		{
			calcMetricPreBuf(next.frameEO-1+j,next.frameEO+j,j,np,true,true,env);
			break;
		}
	}
	if (retFrames == cycle)
	{
		if (lastCycle+(n-lastGroup) > nfrms) 
		{ 
			retFrames = -1; 
			lastFrame = n-1;
			fprintf(mkvOutF,"# TDecimate Mode 3:  Last Frame = %d\n", lastFrame);
		}
		else
		{
			if (debug) debugOutput2(n, lastCycle+(n-lastGroup), false, 0, 0, 0.0, 0.0);
			if (display) 
			{
				PVideoFrame dst;
				if (!useclip2) dst = child->GetFrame(lastCycle+(n-lastGroup), env);
				else 
				{
					dst = clip2->GetFrame(lastCycle+(n-lastGroup), env);
					np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
				}
				env->MakeWritable(&dst);
				displayOutput(env, dst, n, lastCycle+(n-lastGroup), false, 0.0, 0.0, 0, 0, 
					useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np);
				return dst;
			}
			if (!useclip2) return child->GetFrame(lastCycle+(n-lastGroup), env);
			return clip2->GetFrame(lastCycle+(n-lastGroup), env);
		}
	}
	else if (retFrames == cycle-cycleR || (curr.blend == 3 && retFrames == cycle-cycleR-1))
	{
		int ret = curr.getNonDec(n-lastGroup);
		if ((ret >= 0 && curr.frame+ret > nfrms) || ret < 0) 
		{ 
			retFrames = -1; 
			lastFrame = n-1;
			if (lastType > 0)
			{
				if (tcfv1) fprintf(mkvOutF, "%d,%d,%4.6f\n", lastGroup-((lastType-1)*(cycle-cycleR)),lastFrame,mkvfps);
				lastType = 0;
			}
			fprintf(mkvOutF,"# TDecimate Mode 3:  Last Frame = %d\n", lastFrame);
		}
		else
		{
			if (debug) debugOutput2(n, curr.frame+ret, true, 0, 0, 0.0, 0.0);
			if (display) 
			{
				PVideoFrame dst;
				if (!useclip2) dst = child->GetFrame(curr.frame+ret, env);
				else 
				{
					dst = clip2->GetFrame(curr.frame+ret, env);
					np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
				}
				env->MakeWritable(&dst);
				displayOutput(env, dst, n, curr.frame+ret, true, 0.0, 0.0, curr.blend == 3 ? 5 : 0, 0, 
					useclip2 ? (clip2->GetVideoInfo().IsYV12() ? 3 : 1) : np);
				return dst;
			}
			if (!useclip2) return child->GetFrame(curr.frame+ret, env);
			return clip2->GetFrame(curr.frame+ret, env);
		}
	}
	if (retFrames == -1 && mkvOutF != NULL)
	{
		double filmCf = ((double)(filmC)/(double)(nfrms+1))*100.0;
		double videoCf = ((double)(vidC)/(double)(nfrms+1))*100.0;
		fprintf(mkvOutF, "# vfr stats:  %05.2f%c film  %05.2f%c video\n", filmCf, '%', videoCf, '%');
		fprintf(mkvOutF, "# vfr stats:  %d - film  %d - video  %d - total\n", filmC, vidC, nfrms+1);
		fprintf(mkvOutF, "# vfr stats:  longest vid section - %d frames\n", longestV);
		fprintf(mkvOutF, "# vfr stats:  # of detected vid sections - %d", countVT);
		fclose(mkvOutF);
		mkvOutF = NULL;
	}
	PVideoFrame dst = env->NewVideoFrame(vi);
	setBlack(dst, vi.IsYV12() ? 3 : 1);
	if (retFrames <= -305)
	{
		if (retFrames <= -306 && se)
			env->ThrowError("TDecimate:  mode 3 finished (early termination)!");
		sprintf(buf, "Mode 3:  Last Actual Frame = %d", lastFrame);
		Draw(dst, 2, 1, buf, vi.IsYV12() ? 3 : 1);
	}
	--retFrames;
	return dst;
}

PVideoFrame TDecimate::GetFrameMode4(int n, IScriptEnvironment *env, int np)
{
	PVideoFrame prv = child->GetFrame(n>0?n-1:0,env);
	PVideoFrame src = child->GetFrame(n, env);
	int blockN = -20, xblocks;
	unsigned __int64 metricU = ULLONG_MAX, metricF = ULLONG_MAX;
	getOvrFrame(n, metricU, metricF);
	if (metricU == ULLONG_MAX || metricF == ULLONG_MAX || display) 
		metricU = calcMetric(prv, src, np, blockN, xblocks, metricF, env, true);
	double metricN = (metricU*100.0)/MAX_DIFF;
	if (debug)
	{
		sprintf(buf,"TDecimate:  frame %d  metric = %3.2f  metricF = %I64u (%3.2f)", n, metricN, metricF,
			(double)metricF*100.0/(double)sceneDivU);
		OutputDebugString(buf);
	}
	if (*output && metricsOutArray != NULL) 
	{
		metricsOutArray[n<<1] = metricU;
		metricsOutArray[(n<<1)+1] = metricF;
	}
	if (display)
	{
		if (useclip2) 
		{
			src = clip2->GetFrame(n, env);
			np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
		}
		env->MakeWritable(&src);
		if (blockN != -20) drawBox(src, blockN, xblocks, np);
		sprintf(buf, "TDecimate %s by tritical", VERSION);
		Draw(src, 0, 0, buf, np);
		sprintf(buf, "Mode: 4 (metrics output)");
		Draw(src, 0, 1, buf, np);
		sprintf(buf, "chroma = %s  denoise = %s", chroma ? "true" : "false",
			predenoise ? "true" : "false");
		Draw(src, 0, 2, buf, np);
		sprintf(buf,"Frame %d:  %3.2f  %3.2f", n, metricN, (double)metricF*100.0/(double)sceneDivU);
		Draw(src, 0, 3, buf, np);
		return src;
	}
	if (!useclip2) return src;
	return clip2->GetFrame(n, env);
}

PVideoFrame TDecimate::GetFrameMode5(int n, IScriptEnvironment *env, int np)
{
	int frame = aLUT[n];
	if (debug)
	{
		sprintf(buf,"TDecimate:  inframe = %d  useframe = %d  (mode = %d)", n, frame, mode);
		OutputDebugString(buf);
	}
	if (display)
	{
		PVideoFrame dst;
		if (!useclip2) dst = child->GetFrame(frame, env);
		else 
		{
			dst = clip2->GetFrame(frame, env);
			np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
		}
		env->MakeWritable(&dst);
		sprintf(buf, "TDecimate %s by tritical", VERSION);
		Draw(dst, 0, 0, buf, np);
		sprintf(buf, "Mode: %d (vfr)  Hybrid = %d", mode, hybrid);
		Draw(dst, 0, 1, buf, np);
		sprintf(buf,"inframe = %d  useframe = %d", n, frame);
		Draw(dst, 0, 2, buf, np);
		return dst;
	}
	if (!useclip2) return child->GetFrame(frame, env);
	return clip2->GetFrame(frame, env);
}

PVideoFrame TDecimate::GetFrameMode6(int n, IScriptEnvironment *env, int np)
{
	int frame = aLUT[n];
	if (debug)
	{
		sprintf(buf,"TDecimate:  inframe = %d  useframe = %d  (mode = %d)", n, frame, mode);
		OutputDebugString(buf);
	}
	if (display)
	{
		PVideoFrame dst;
		if (!useclip2) dst = child->GetFrame(frame, env);
		else 
		{
			dst = clip2->GetFrame(frame, env);
			np = clip2->GetVideoInfo().IsYV12() ? 3 : 1;
		}
		env->MakeWritable(&dst);
		sprintf(buf, "TDecimate %s by tritical", VERSION);
		Draw(dst, 0, 0, buf, np);
		sprintf(buf, "Mode: %d (120fps -> vfr)", mode);
		Draw(dst, 0, 1, buf, np);
		sprintf(buf,"inframe = %d  useframe = %d", n, frame);
		Draw(dst, 0, 2, buf, np);
		return dst;
	}
	if (!useclip2) return child->GetFrame(frame, env);
	return clip2->GetFrame(frame, env);
}

void TDecimate::rerunFromStart(int s, int np, IScriptEnvironment *env)
{
	int EvalGroup = 0;
	while (EvalGroup < s)
	{
		prev = curr;
		if (prev.frame != EvalGroup - cycle)
		{
			prev.setFrame(EvalGroup - cycle);
			getOvrCycle(prev, false);
			calcMetricCycle(prev, env, np, true, true);
			if (hybrid > 0)
			{
				checkVideoMatches(prev, prev);
				checkVideoMetrics(prev, vidThresh);
			}
		}
		curr = next;
		if (curr.frame != EvalGroup)
		{
			curr.setFrame(EvalGroup);
			getOvrCycle(curr, false);
			calcMetricCycle(curr, env, np, true, true);
			if (hybrid > 0)
			{
				checkVideoMatches(prev, curr);
				checkVideoMetrics(curr, vidThresh);
			}
		}
		next.setFrame(EvalGroup + cycle);
		getOvrCycle(next, false);
		calcMetricCycle(next, env, np, true, true);
		if (hybrid > 0)
		{
			checkVideoMatches(curr, next);
			checkVideoMetrics(next, vidThresh);
		}
		if (hybrid > 0 && curr.type > 1)
		{
			int scenetest = curr.sceneDetect(prev, next, sceneThreshU);
			bool isVid = ((curr.type == 2 || curr.type == 4) && !curr.isfilmd2v && // matches
						  (prev.type == 5 || (prev.type == 2 && (vidDetect == 0 || vidDetect == 2)) || prev.type == 4 ||
						   next.type == 5 || (next.type == 2 && (vidDetect == 0 || vidDetect == 2)) || next.type == 4 || 
						   conCycle == 1 || scenetest != -20));
			bool isVid2 = ((curr.type == 3 || curr.type == 4) && !curr.isfilmd2v && // metrics
						  (prev.type == 5 || (prev.type == 3 && (vidDetect == 1 || vidDetect == 2)) || prev.type == 4 ||
						   next.type == 5 || (next.type == 3 && (vidDetect == 1 || vidDetect == 2)) || next.type == 4 || 
						   conCycle == 1 || scenetest != -20));
			if (curr.type == 5 || (vidDetect == 0 && isVid) || (vidDetect == 1 && isVid2) ||
			    (vidDetect == 2 && (isVid2 || isVid)) || (vidDetect == 3 && (isVid2 && isVid)))
			{
				int temp = curr.sceneDetect(prev, next, sceneThreshU);
				if (temp != -20 && hybrid != 3)
				{
					for (int p=curr.cycleS; p<curr.cycleE; ++p) curr.decimate[p] = curr.decimate2[p] = 0;
					curr.decimate[temp] = curr.decimate2[temp] = 1;
					curr.blend = 2;
					curr.decSet = true;
				}
				else curr.blend = 1;
			}
			else { goto novidjump; }
		}
		else
		{
novidjump:
			if (mode == 0) mostSimilarDecDecision(prev, curr, next, env);
			else
			{
				prev.setDups(dupThresh);
				curr.setDups(dupThresh);
				next.setDups(dupThresh);
				findDupStrings(prev, curr, next, env);
			}
			if (curr.blend == 3)
			{
				int tscene = curr.sceneDetect(prev, next, sceneThreshU);
				if (tscene != -20 && curr.decimate[tscene] == 1 && hybrid != 3)
				{
					curr.decimate[tscene] = curr.decimate2[tscene] = 0;
					curr.blend = 0;
				}
			}
			if (curr.blend != 3) curr.blend = 0;
		}
		EvalGroup += cycle;
	}
}

void TDecimate::calcMetricPreBuf(int n1, int n2, int pos, int np, bool scene, 
						bool gethint, IScriptEnvironment *env)
{
	if (n2 > nbuf.maxFrame || n2 < 0) return;
	if (n2 < nbuf.frameSO || n2 >= nbuf.frameEO || n1 != n2-1 ||
		nbuf.frameSO+pos != n2) 
		env->ThrowError("TDecimate:  internal error during pre-buffering (%d,%d,%d)!", 
			n1, n2, pos);
	if (n2 == 0) n1 = 0;
	int blockNI, xblocksI;
	unsigned __int64 metricF;
	PVideoFrame src = NULL;
	if (nbuf.diffMetricsU[pos] == ULLONG_MAX || 
		(nbuf.diffMetricsUF[pos] == ULLONG_MAX && scene))
	{
		src = child->GetFrame(n2, env);
		nbuf.diffMetricsU[pos] = calcMetric(child->GetFrame(n1, env), src, 
			np, blockNI, xblocksI, metricF, env, scene);
		nbuf.diffMetricsN[pos] = (nbuf.diffMetricsU[pos] * 100.0) / MAX_DIFF;
		if (scene) nbuf.diffMetricsUF[pos] = metricF;
	}
	if (gethint && nbuf.match[pos] == -20)
	{
		if (!usehints) nbuf.match[pos] = -200;
		else
		{
			if (!src) nbuf.match[pos] = getHint(child->GetFrame(n2, env), nbuf.filmd2v[pos]);
			else nbuf.match[pos] = getHint(src, nbuf.filmd2v[pos]);
		}
	}
}

unsigned __int64 TDecimate::calcMetric(PVideoFrame &prevt, PVideoFrame &currt, int np, int &blockNI, 
		int &xblocksI, unsigned __int64 &metricF, IScriptEnvironment *env, bool scene)
{
	PVideoFrame prev, curr;
	VideoInfo vit = child->GetVideoInfo();
	if (predenoise)
	{
		prev = env->NewVideoFrame(vit);
		curr = env->NewVideoFrame(vit);
		blurFrame(prevt, prev, np, 2, chroma, env, vit, opt);
		blurFrame(currt, curr, np, 2, chroma, env, vit, opt);
	}
	else
	{
		prev = prevt;
		curr = currt;
	}
	const unsigned char *prvp, *curp, *prvpT, *curpT;
	int prv_pitch, cur_pitch, width, height, y, x, difft;
	int xblocks = ((vit.width+xhalfS)>>xshiftS) + 1;
	int xblocks4 = xblocks<<2;
	int yblocks = ((vit.height+yhalfS)>>yshiftS) + 1;
	int arraysize = (xblocks*yblocks)<<2;
	int temp1, temp2, box1, box2, stop, inc;
	int yhalf, xhalf, yshift, xshift, b, plane;
	int widtha, heighta, u, v, diffs;
	unsigned __int64 highestDiff = 0;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	memset(diff,0,arraysize*sizeof(unsigned __int64));
	stop = chroma ? np : 1;
	inc = np == 3 ? 1 : chroma ? 1 : 2;
	for (b=0; b<stop; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_U;
		else plane = PLANAR_V;
		prvp = prev->GetReadPtr(plane);
		prv_pitch = prev->GetPitch(plane);
		width = prev->GetRowSize(plane);
		height = prev->GetHeight(plane);
		curp = curr->GetReadPtr(plane);
		cur_pitch = curr->GetPitch(plane);
		if (b == 0)
		{
			yshift = yshiftS;
			yhalf = yhalfS;
			if (np == 3)
			{
				xshift = xshiftS;
				xhalf = xhalfS;
			}
			else
			{
				xshift = xshiftS+1;
				xhalf = xhalfS<<1;
			}
		}
		else
		{
			yshift = yshiftS-1;
			yhalf = yhalfS>>1;
			xshift = xshiftS-1;
			xhalf = xhalfS>>1;
		}
		if (blockx == 32 && blocky == 32 && nt <= 0)
		{
			if (ssd && (cpu&CPUF_SSE2))
				calcDiffSSD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, true);
			else if (ssd && (cpu&CPUF_MMX)) 
				calcDiffSSD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, false);
			else if (!ssd && (cpu&CPUF_SSE2))
				calcDiffSAD_32x32_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, true);
			else if (!ssd && (cpu&CPUF_INTEGER_SSE))
				calcDiffSAD_32x32_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, false);
			else if (!ssd && (cpu&CPUF_MMX))
				calcDiffSAD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
			else { goto use_c; }
		}
		else if (((np == 3 && blockx >= 16 && blocky >= 16) || (np == 1 && blockx >= 8 && blocky >= 8)) && nt <= 0)
		{
			if (ssd && (cpu&CPUF_MMX)) 
				calcDiffSSD_Generic_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
			else if (!ssd && (cpu&CPUF_INTEGER_SSE))
				calcDiffSAD_Generic_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
			else if (!ssd && (cpu&CPUF_MMX))
				calcDiffSAD_Generic_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
			else { goto use_c; }
		}
		else
		{
use_c:
			heighta = (height>>(yshift-1))<<(yshift-1);
			widtha = (width>>(xshift-1))<<(xshift-1);
			if (ssd)
			{
				for (y=0; y<heighta; y+=yhalf)
				{
					temp1 = (y>>yshift)*xblocks4;
					temp2 = ((y+yhalf)>>yshift)*xblocks4;
					for (x=0; x<widtha; x+=xhalf)
					{
						prvpT = prvp;
						curpT = curp;
						for (diffs=0, u=0; u<yhalf; ++u)
						{
							for (v=0; v<xhalf; v+=inc)
							{
								difft = prvpT[x+v]-curpT[x+v];
								difft *= difft;
								if (difft > nt) diffs += difft;
							}
							prvpT += prv_pitch;
							curpT += cur_pitch;
						}
						if (diffs > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += diffs;
							diff[temp1+box2+1] += diffs;
							diff[temp2+box1+2] += diffs;
							diff[temp2+box2+3] += diffs;
						}
					}
					for (x=widtha; x<width; x+=inc)
					{
						prvpT = prvp;
						curpT = curp;
						for (diffs=0, u=0; u<yhalf; ++u)
						{
							difft = prvpT[x]-curpT[x];
							difft *= difft;
							if (difft > nt) diffs += difft;
							prvpT += prv_pitch;
							curpT += cur_pitch;
						}
						if (diffs > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += diffs;
							diff[temp1+box2+1] += diffs;
							diff[temp2+box1+2] += diffs;
							diff[temp2+box2+3] += diffs;
						}
					}
					prvp += prv_pitch*yhalf;
					curp += cur_pitch*yhalf;
				}
				for (y=heighta; y<height; ++y)
				{
					temp1 = (y>>yshift)*xblocks4;
					temp2 = ((y+yhalf)>>yshift)*xblocks4;
					for (x=0; x<width; x+=inc)
					{
						difft = prvp[x]-curp[x];
						difft *= difft;
						if (difft > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += difft;
							diff[temp1+box2+1] += difft;
							diff[temp2+box1+2] += difft;
							diff[temp2+box2+3] += difft;
						}
					}
					prvp += prv_pitch;
					curp += cur_pitch;
				}
			}
			else
			{
				for (y=0; y<heighta; y+=yhalf)
				{
					temp1 = (y>>yshift)*xblocks4;
					temp2 = ((y+yhalf)>>yshift)*xblocks4;
					for (x=0; x<widtha; x+=xhalf)
					{
						prvpT = prvp;
						curpT = curp;
						for (diffs=0, u=0; u<yhalf; ++u)
						{
							for (v=0; v<xhalf; v+=inc)
							{
								difft = abs(prvpT[x+v]-curpT[x+v]);
								if (difft > nt) diffs += difft;
							}
							prvpT += prv_pitch;
							curpT += cur_pitch;
						}
						if (diffs > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += diffs;
							diff[temp1+box2+1] += diffs;
							diff[temp2+box1+2] += diffs;
							diff[temp2+box2+3] += diffs;
						}
					}
					for (x=widtha; x<width; x+=inc)
					{
						prvpT = prvp;
						curpT = curp;
						for (diffs=0, u=0; u<yhalf; ++u)
						{
							difft = abs(prvpT[x]-curpT[x]);
							if (difft > nt) diffs += difft;
							prvpT += prv_pitch;
							curpT += cur_pitch;
						}
						if (diffs > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += diffs;
							diff[temp1+box2+1] += diffs;
							diff[temp2+box1+2] += diffs;
							diff[temp2+box2+3] += diffs;
						}
					}
					prvp += prv_pitch*yhalf;
					curp += cur_pitch*yhalf;
				}
				for (y=heighta; y<height; ++y)
				{
					temp1 = (y>>yshift)*xblocks4;
					temp2 = ((y+yhalf)>>yshift)*xblocks4;
					for (x=0; x<width; x+=inc)
					{
						difft = abs(prvp[x]-curp[x]);
						if (difft > nt)
						{
							box1 = (x>>xshift)<<2;
							box2 = ((x+xhalf)>>xshift)<<2;
							diff[temp1+box1+0] += difft;
							diff[temp1+box2+1] += difft;
							diff[temp2+box1+2] += difft;
							diff[temp2+box2+3] += difft;
						}
					}
					prvp += prv_pitch;
					curp += cur_pitch;
				}
			}
		}
		if (b == 0)
		{
			metricF = 0;
			if (scene)
			{
				if (np == 3 || !chroma)
				{
					for (x=0; x<arraysize; x+=4) metricF += diff[x];
				}
				else
				{
					if (ssd)
						metricF = calcLumaDiffYUY2SSD(prev->GetReadPtr(),curr->GetReadPtr(),
							prev->GetRowSize(),prev->GetHeight(),prev->GetPitch(),curr->GetPitch(),env);
					else
						metricF = calcLumaDiffYUY2SAD(prev->GetReadPtr(),curr->GetReadPtr(),
							prev->GetRowSize(),prev->GetHeight(),prev->GetPitch(),curr->GetPitch(),env);
				}
			}
		}
	}
	blockNI = -20;
	xblocksI = xblocks4;
	for (x=0; x<arraysize; ++x)
	{
		if (diff[x] > highestDiff)
		{
			highestDiff = diff[x];
			blockNI = x;
		}
	}
	if (blockNI == -20) blockNI = 0;
	if (ssd)
	{
		highestDiff = (unsigned __int64)(sqrt((double)(highestDiff)));
		metricF = (unsigned __int64)(sqrt((double)(metricF)));
	}
	return highestDiff;
}

void TDecimate::calcMetricCycle(Cycle &current, IScriptEnvironment *env, int np,
								bool scene, bool hnt)
{
	if (current.mSet || current.cycleS == current.cycleE) return;
	VideoInfo vit = child->GetVideoInfo();
	int xblocks = ((vit.width+xhalfS)>>xshiftS) + 1;
	int xblocks4 = xblocks<<2;
	int yblocks = ((vit.height+yhalfS)>>yshiftS) + 1;
	int arraysize = (xblocks*yblocks)<<2;
	int temp1, temp2, box1, box2;
	int yhalf, xhalf, yshift, xshift, b, plane;
	int prv_pitch, cur_pitch, width, height, difft, diffs;
	int i, y, x, w, stop, inc, u, v, widtha, heighta;
	unsigned __int64 highestDiff;
	int next_num = -20, next_numd = -20;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	PVideoFrame prev, next, prevt, nextt;
	if (predenoise)
	{
		prev = env->NewVideoFrame(vit);
		next = env->NewVideoFrame(vit);
	}
	const unsigned char *prvp, *curp, *prvpT, *curpT;
	for (w=current.frameSO, i=current.cycleS; i<current.cycleE; ++i, ++w)
	{
		if ((current.match[i] != -20 || !hnt) && current.diffMetricsU[i] != ULLONG_MAX &&
			(current.diffMetricsUF[i] != ULLONG_MAX || !scene)) continue;
		if (predenoise)
		{
			if (current.diffMetricsU[i] != ULLONG_MAX && 
				(current.diffMetricsUF[i] != ULLONG_MAX || !scene))
			{
				if (current.match[i] == -20 && hnt) 
				{
					if (!usehints) current.match[i] = -200;
					else 
					{
						nextt = child->GetFrame(w, env);
						next_num = w;
						current.match[i] = getHint(nextt, current.filmd2v[i]);
					}
				}
				continue;
			}
			if (next_num == w-1) prevt = nextt;
			else prevt = child->GetFrame(w>0?w-1:0,env);
			nextt = child->GetFrame(w, env);
			next_num = w;
			if (current.match[i] == -20 && hnt) 
			{
				if (!usehints) current.match[i] = -200;
				else current.match[i] = getHint(nextt, current.filmd2v[i]);
			}
			if (next_numd == w-1) copyFrame(prev, next, env, np);
			else blurFrame(prevt, prev, np, 2, chroma, env, vit, opt);
			blurFrame(nextt, next, np, 2, chroma, env, vit, opt);
			next_numd = w;
		}
		else
		{
			if (current.diffMetricsU[i] != ULLONG_MAX && 
				(current.diffMetricsUF[i] != ULLONG_MAX || !scene))
			{
				if (current.match[i] == -20 && hnt) 
				{
					if (!usehints) current.match[i] = -200;
					else 
					{
						next = child->GetFrame(w, env);
						next_num = w;
						current.match[i] = getHint(next, current.filmd2v[i]);
					}
				}
				continue;
			}
			if (next_num == w-1) prev = next;
			else prev = child->GetFrame(w>0?w-1:0,env);
			next = child->GetFrame(w, env);
			next_num = w;
			if (current.match[i] == -20 && hnt) 
			{
				if (!usehints) current.match[i] = -200;
				else current.match[i] = getHint(next, current.filmd2v[i]);
			}
		}
		memset(diff,0,arraysize*sizeof(unsigned __int64));
		stop = chroma ? np : 1;
		inc = np == 3 ? 1 : chroma ? 1 : 2;
		for (b=0; b<stop; ++b)
		{
			if (b == 0) plane = PLANAR_Y;
			else if (b == 1) plane = PLANAR_U;
			else plane = PLANAR_V;
			prvp = prev->GetReadPtr(plane);
			prv_pitch = prev->GetPitch(plane);
			width = prev->GetRowSize(plane);
			height = prev->GetHeight(plane);
			curp = next->GetReadPtr(plane);
			cur_pitch = next->GetPitch(plane);
			if (b == 0)
			{
				yshift = yshiftS;
				yhalf = yhalfS;
				if (np == 3)
				{
					xshift = xshiftS;
					xhalf = xhalfS;
				}
				else
				{
					xshift = xshiftS+1;
					xhalf = xhalfS<<1;
				}
			}
			else
			{
				yshift = yshiftS-1;
				yhalf = yhalfS>>1;
				xshift = xshiftS-1;
				xhalf = xhalfS>>1;
			}
			if (blockx == 32 && blocky == 32 && nt <= 0)
			{
				if (ssd && (cpu&CPUF_SSE2))
					calcDiffSSD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, true);
				else if (ssd && (cpu&CPUF_MMX)) 
					calcDiffSSD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, false);
				else if (!ssd && (cpu&CPUF_SSE2))
					calcDiffSAD_32x32_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, true);
				else if (!ssd && (cpu&CPUF_INTEGER_SSE))
					calcDiffSAD_32x32_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np, false);
				else if (!ssd && (cpu&CPUF_MMX))
					calcDiffSAD_32x32_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
				else { goto use_c; }
			}
			else if (((np == 3 && blockx >= 16 && blocky >= 16) || (np == 1 && blockx >= 8 && blocky >= 8)) && nt <= 0)
			{
				if (ssd && (cpu&CPUF_MMX)) 
					calcDiffSSD_Generic_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
				else if (!ssd && (cpu&CPUF_INTEGER_SSE))
					calcDiffSAD_Generic_iSSE(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
				else if (!ssd && (cpu&CPUF_MMX))
					calcDiffSAD_Generic_MMX(prvp, curp, prv_pitch, cur_pitch, width, height, b, xblocks4, np);
				else { goto use_c; }
			}
			else
			{
use_c:
				heighta = (height>>(yshift-1))<<(yshift-1);
				widtha = (width>>(xshift-1))<<(xshift-1);
				if (ssd)
				{
					for (y=0; y<heighta; y+=yhalf)
					{
						temp1 = (y>>yshift)*xblocks4;
						temp2 = ((y+yhalf)>>yshift)*xblocks4;
						for (x=0; x<widtha; x+=xhalf)
						{
							prvpT = prvp;
							curpT = curp;
							for (diffs=0, u=0; u<yhalf; ++u)
							{
								for (v=0; v<xhalf; v+=inc)
								{
									difft = prvpT[x+v]-curpT[x+v];
									difft *= difft;
									if (difft > nt) diffs += difft;
								}
								prvpT += prv_pitch;
								curpT += cur_pitch;
							}
							if (diffs > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += diffs;
								diff[temp1+box2+1] += diffs;
								diff[temp2+box1+2] += diffs;
								diff[temp2+box2+3] += diffs;
							}
						}
						for (x=widtha; x<width; x+=inc)
						{
							prvpT = prvp;
							curpT = curp;
							for (diffs=0, u=0; u<yhalf; ++u)
							{
								difft = prvpT[x]-curpT[x];
								difft *= difft;
								if (difft > nt) diffs += difft;
								prvpT += prv_pitch;
								curpT += cur_pitch;
							}
							if (diffs > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += diffs;
								diff[temp1+box2+1] += diffs;
								diff[temp2+box1+2] += diffs;
								diff[temp2+box2+3] += diffs;
							}
						}
						prvp += prv_pitch*yhalf;
						curp += cur_pitch*yhalf;
					}
					for (y=heighta; y<height; ++y)
					{
						temp1 = (y>>yshift)*xblocks4;
						temp2 = ((y+yhalf)>>yshift)*xblocks4;
						for (x=0; x<width; x+=inc)
						{
							difft = prvp[x]-curp[x];
							difft *= difft;
							if (difft > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += difft;
								diff[temp1+box2+1] += difft;
								diff[temp2+box1+2] += difft;
								diff[temp2+box2+3] += difft;
							}
						}
						prvp += prv_pitch;
						curp += cur_pitch;
					}
				}
				else
				{
					for (y=0; y<heighta; y+=yhalf)
					{
						temp1 = (y>>yshift)*xblocks4;
						temp2 = ((y+yhalf)>>yshift)*xblocks4;
						for (x=0; x<widtha; x+=xhalf)
						{
							prvpT = prvp;
							curpT = curp;
							for (diffs=0, u=0; u<yhalf; ++u)
							{
								for (v=0; v<xhalf; v+=inc)
								{
									difft = abs(prvpT[x+v]-curpT[x+v]);
									if (difft > nt) diffs += difft;
								}
								prvpT += prv_pitch;
								curpT += cur_pitch;
							}
							if (diffs > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += diffs;
								diff[temp1+box2+1] += diffs;
								diff[temp2+box1+2] += diffs;
								diff[temp2+box2+3] += diffs;
							}
						}
						for (x=widtha; x<width; x+=inc)
						{
							prvpT = prvp;
							curpT = curp;
							for (diffs=0, u=0; u<yhalf; ++u)
							{
								difft = abs(prvpT[x]-curpT[x]);
								if (difft > nt) diffs += difft;
								prvpT += prv_pitch;
								curpT += cur_pitch;
							}
							if (diffs > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += diffs;
								diff[temp1+box2+1] += diffs;
								diff[temp2+box1+2] += diffs;
								diff[temp2+box2+3] += diffs;
							}
						}
						prvp += prv_pitch*yhalf;
						curp += cur_pitch*yhalf;
					}
					for (y=heighta; y<height; ++y)
					{
						temp1 = (y>>yshift)*xblocks4;
						temp2 = ((y+yhalf)>>yshift)*xblocks4;
						for (x=0; x<width; x+=inc)
						{
							difft = abs(prvp[x]-curp[x]);
							if (difft > nt)
							{
								box1 = (x>>xshift)<<2;
								box2 = ((x+xhalf)>>xshift)<<2;
								diff[temp1+box1+0] += difft;
								diff[temp1+box2+1] += difft;
								diff[temp2+box1+2] += difft;
								diff[temp2+box2+3] += difft;
							}
						}
						prvp += prv_pitch;
						curp += cur_pitch;
					}
				}
			}
			if (b == 0)
			{
				current.diffMetricsUF[i] = 0;
				if (scene)
				{
					if (np == 3 || !chroma)
					{
						for (x=0; x<arraysize; x+=4) current.diffMetricsUF[i] += diff[x];
					}
					else 
					{
						if (ssd)
							current.diffMetricsUF[i] = calcLumaDiffYUY2SSD(prev->GetReadPtr(),next->GetReadPtr(),
								prev->GetRowSize(),prev->GetHeight(),prev->GetPitch(),next->GetPitch(), env);
						else
							current.diffMetricsUF[i] = calcLumaDiffYUY2SAD(prev->GetReadPtr(),next->GetReadPtr(),
								prev->GetRowSize(),prev->GetHeight(),prev->GetPitch(),next->GetPitch(), env);
					}
				}
			}
		}
		highestDiff = 0;
		for (x=0; x<arraysize; ++x)
		{
			if (diff[x] > highestDiff) highestDiff = diff[x];
		}
		if (ssd)
		{
			highestDiff = (unsigned __int64)(sqrt((double)(highestDiff)));
			current.diffMetricsUF[i] = (unsigned __int64)(sqrt((double)(current.diffMetricsUF[i])));
		}
		current.diffMetricsU[i] = highestDiff;
		current.diffMetricsN[i] = (highestDiff * 100.0) / MAX_DIFF;
	}
	current.mSet = true;
	current.setIsFilmD2V();
}

unsigned __int64 TDecimate::calcLumaDiffYUY2SAD(const unsigned char *prvp, const unsigned char *nxtp,
	int width, int height, int prv_pitch, int nxt_pitch, IScriptEnvironment *env)
{
	unsigned __int64 diff = 0;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_INTEGER_SSE) && nt == 0)
	{
		if (!(width&15))
		{
			if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|prv_pitch|nxt_pitch)&15)) 
				calcLumaDiffYUY2SAD_SSE2_16(prvp, nxtp, width, height, prv_pitch, nxt_pitch, diff);
			else
				calcLumaDiffYUY2SAD_ISSE_16(prvp, nxtp, width, height, prv_pitch, nxt_pitch, diff);
		}
		else
		{
			int widtha = (width>>3)<<3;
			calcLumaDiffYUY2SAD_ISSE_8(prvp, nxtp, widtha, height, prv_pitch, nxt_pitch, diff);
			for (int y=0; y<height; ++y)
			{
				for (int x=widtha; x<width; x+=2)
				{
					int temp = abs(prvp[x]-nxtp[x]);
					if (temp > nt) diff += temp;
				}
				prvp += prv_pitch;
				nxtp += nxt_pitch;
			}
		}
	}
	else if ((cpu&CPUF_MMX) && nt == 0)
	{
		if (!(width&15))
			calcLumaDiffYUY2SAD_MMX_16(prvp, nxtp, width, height, prv_pitch, nxt_pitch, diff);
		else
		{
			int widtha = (width>>3)<<3;
			calcLumaDiffYUY2SAD_MMX_8(prvp, nxtp, widtha, height, prv_pitch, nxt_pitch, diff);
			for (int y=0; y<height; ++y)
			{
				for (int x=widtha; x<width; x+=2)
				{
					int temp = abs(prvp[x]-nxtp[x]);
					if (temp > nt) diff += temp;
				}
				prvp += prv_pitch;
				nxtp += nxt_pitch;
			}
		}
	}
	else
	{
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; x+=2)
			{
				const int temp = abs(prvp[x]-nxtp[x]);
				if (temp > nt) diff += temp;
			}
			prvp += prv_pitch;
			nxtp += nxt_pitch;
		}
	}
	return diff;
}

unsigned __int64 TDecimate::calcLumaDiffYUY2SSD(const unsigned char *prvp, const unsigned char *nxtp,
	int width, int height, int prv_pitch, int nxt_pitch, IScriptEnvironment *env)
{
	unsigned __int64 diff = 0;
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_MMX) && nt == 0)
	{
		if (!(width&15))
		{
			if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|prv_pitch|nxt_pitch)&15))
				calcLumaDiffYUY2SSD_SSE2_16(prvp, nxtp, width, height, prv_pitch, nxt_pitch, diff);
			else
				calcLumaDiffYUY2SSD_MMX_16(prvp, nxtp, width, height, prv_pitch, nxt_pitch, diff);
		}
		else
		{
			int widtha = (width>>3)<<3;
			calcLumaDiffYUY2SSD_MMX_8(prvp, nxtp, widtha, height, prv_pitch, nxt_pitch, diff);
			for (int y=0; y<height; ++y)
			{
				for (int x=widtha; x<width; x+=2)
				{
					int temp = prvp[x]-nxtp[x];
					temp *= temp;
					if (temp > nt) diff += temp;
				}
				prvp += prv_pitch;
				nxtp += nxt_pitch;
			}
		}
	}
	else
	{
		for (int y=0; y<height; ++y)
		{
			for (int x=0; x<width; x+=2)
			{
				int temp = prvp[x]-nxtp[x];
				temp *= temp;
				if (temp > nt) diff += temp;
			}
			prvp += prv_pitch;
			nxtp += nxt_pitch;
		}
	}
	return diff;
}

void TDecimate::calcDiffSAD_32x32_iSSE(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, 
		bool use_sse2)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int widtha, heighta, heights = height, widths = width;
	const unsigned char *ptr1T, *ptr2T;
	bool use_sse2a = false;
	if (use_sse2 && !((int(ptr1)|int(ptr2)|pitch1|pitch2)&15)) use_sse2a = true;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>4)<<4;
			widtha = (width>>4)<<4;
			height >>= 4;
			width >>= 4;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_SSE2_16x16(ptr1+(x<<4),ptr2+(x<<4),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_iSSE_16x16(ptr1+(x<<4),ptr2+(x<<4),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_iSSE_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>4)*xblocks4;
				temp2 = ((y+8)>>4)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>4)<<4;
		widtha = (width>>5)<<5;
		height >>= 4;
		width >>= 5;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_SSE2_32x16(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_iSSE_32x16(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_SSE2_32x16_luma(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSAD_iSSE_32x16_luma(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::calcDiffSAD_32x32_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int widtha, heighta, heights = height, widths = width;
	const unsigned char *ptr1T, *ptr2T;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>4)<<4;
			widtha = (width>>4)<<4;
			height >>= 4;
			width >>= 4;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_16x16(ptr1+(x<<4),ptr2+(x<<4),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>4)*xblocks4;
				temp2 = ((y+8)>>4)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>4)<<4;
		widtha = (width>>5)<<5;
		height >>= 4;
		width >>= 5;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_32x16(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_32x16_luma(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::calcDiffSSD_32x32_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np, bool use_sse2)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int widtha, heighta, heights = height, widths = width;
	const unsigned char *ptr1T, *ptr2T;
	bool use_sse2a = false;
	if (use_sse2 && !((int(ptr1)|int(ptr2)|pitch1|pitch2)&15)) use_sse2a = true;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>4)<<4;
			widtha = (width>>4)<<4;
			height >>= 4;
			width >>= 4;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_SSE2_16x16(ptr1+(x<<4),ptr2+(x<<4),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_MMX_16x16(ptr1+(x<<4),ptr2+(x<<4),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>5)<<2;
					box2 = ((x+16)>>5)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSSD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>1)<<2;
					box2 = ((x+1)>>1)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>4)*xblocks4;
				temp2 = ((y+8)>>4)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>4)<<2;
					box2 = ((x+8)>>4)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>4)<<4;
		widtha = (width>>5)<<5;
		height >>= 4;
		width >>= 5;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_SSE2_32x16(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_MMX_32x16(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>1)*xblocks4;
				temp2 = ((y+1)>>1)*xblocks4;
				if (use_sse2a)
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_SSE2_32x16_luma(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				else
				{
					for (x=0; x<width; ++x)
					{
						calcSSD_MMX_32x16_luma(ptr1+(x<<5),ptr2+(x<<5),pitch1,pitch2,difft);
						box1 = (x>>1)<<2;
						box2 = ((x+1)>>1)<<2;
						diff[temp1+box1+0] += difft;
						diff[temp1+box2+1] += difft;
						diff[temp2+box1+2] += difft;
						diff[temp2+box2+3] += difft;
					}
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<16; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<4;
				ptr2 += pitch2<<4;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>5)*xblocks4;
				temp2 = ((y+16)>>5)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>6)<<2;
					box2 = ((x+32)>>6)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::calcDiffSAD_Generic_iSSE(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int yshift, yhalf, xshift, xhalf;
	int heighta, heights = height, widtha, widths = width;
	int yshifta, yhalfa, xshifta, xhalfa;
	const unsigned char *ptr1T, *ptr2T;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			yshifta = yshiftS;
			yhalfa = yhalfS;
			xshifta = xshiftS;
			xhalfa = xhalfS;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_iSSE_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>2)<<2;
			widtha = (width>>2)<<2;
			height >>= 2;
			width >>= 2;
			yshifta = yshiftS-1;
			yhalfa = yhalfS>>1;
			xshifta = xshiftS-1;
			xhalfa = xhalfS>>1;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_iSSE_4x4(ptr1+(x<<2),ptr2+(x<<2),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<4; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<2;
				ptr2 += pitch2<<2;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>3)<<3;
		widtha = (width>>3)<<3;
		height >>= 3;
		width >>= 3;
		yshifta = yshiftS;
		yhalfa = yhalfS;
		xshifta = xshiftS+1;
		xhalfa = xhalfS<<1;
		yshift = yshiftS-3;
		yhalf = yhalfS>>3;
		xshift = xshiftS-2;
		xhalf = xhalfS>>2;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_iSSE_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_iSSE_8x8_luma(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::calcDiffSAD_Generic_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int yshift, yhalf, xshift, xhalf;
	int heighta, heights = height, widtha, widths = width;
	int yshifta, yhalfa, xshifta, xhalfa;
	const unsigned char *ptr1T, *ptr2T;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			yshifta = yshiftS;
			yhalfa = yhalfS;
			xshifta = xshiftS;
			xhalfa = xhalfS;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>2)<<2;
			widtha = (width>>2)<<2;
			height >>= 2;
			width >>= 2;
			yshifta = yshiftS-1;
			yhalfa = yhalfS>>1;
			xshifta = xshiftS-1;
			xhalfa = xhalfS>>1;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_4x4(ptr1+(x<<2),ptr2+(x<<2),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<4; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<2;
				ptr2 += pitch2<<2;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>3)<<3;
		widtha = (width>>3)<<3;
		height >>= 3;
		width >>= 3;
		yshifta = yshiftS;
		yhalfa = yhalfS;
		xshifta = xshiftS+1;
		xhalfa = xhalfS<<1;
		yshift = yshiftS-3;
		yhalf = yhalfS>>3;
		xshift = xshiftS-2;
		xhalf = xhalfS>>2;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSAD_MMX_8x8_luma(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += abs(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = abs(ptr1[x]-ptr2[x]);
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::calcDiffSSD_Generic_MMX(const unsigned char *ptr1, const unsigned char *ptr2, 
		int pitch1, int pitch2, int width, int height, int plane, int xblocks4, int np)
{
	int temp1, temp2, y, x, u, difft, box1, box2;
	int yshift, yhalf, xshift, xhalf;
	int heighta, heights = height, widtha, widths = width;
	int yshifta, yhalfa, xshifta, xhalfa;
	const unsigned char *ptr1T, *ptr2T;
	if (np == 3) // YV12
	{
		if (plane == 0)
		{
			heighta = (height>>3)<<3;
			widtha = (width>>3)<<3;
			height >>= 3;
			width >>= 3;
			yshifta = yshiftS;
			yhalfa = yhalfS;
			xshifta = xshiftS;
			xhalfa = xhalfS;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSSD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			heighta = (height>>2)<<2;
			widtha = (width>>2)<<2;
			height >>= 2;
			width >>= 2;
			yshifta = yshiftS-1;
			yhalfa = yhalfS>>1;
			xshifta = xshiftS-1;
			xhalfa = xhalfS>>1;
			yshift = yshiftS-3;
			yhalf = yhalfS>>3;
			xshift = xshiftS-3;
			xhalf = xhalfS>>3;
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSSD_MMX_4x4(ptr1+(x<<2),ptr2+(x<<2),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<4; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<2;
				ptr2 += pitch2<<2;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	else // YUY2
	{
		heighta = (height>>3)<<3;
		widtha = (width>>3)<<3;
		height >>= 3;
		width >>= 3;
		yshifta = yshiftS;
		yhalfa = yhalfS;
		xshifta = xshiftS+1;
		xhalfa = xhalfS<<1;
		yshift = yshiftS-3;
		yhalf = yhalfS>>3;
		xshift = xshiftS-2;
		xhalf = xhalfS>>2;
		if (chroma)
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSSD_MMX_8x8(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; ++x)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; ++x)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				temp1 = (y>>yshift)*xblocks4;
				temp2 = ((y+yhalf)>>yshift)*xblocks4;
				for (x=0; x<width; ++x)
				{
					calcSSD_MMX_8x8_luma(ptr1+(x<<3),ptr2+(x<<3),pitch1,pitch2,difft);
					box1 = (x>>xshift)<<2;
					box2 = ((x+xhalf)>>xshift)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				for (x=widtha; x<widths; x+=2)
				{
					ptr1T = ptr1;
					ptr2T = ptr2;
					for (difft=0, u=0; u<8; ++u)
					{
						difft += (ptr1T[x]-ptr2T[x])*(ptr1T[x]-ptr2T[x]);
						ptr1T += pitch1;
						ptr2T += pitch2;
					}
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1<<3;
				ptr2 += pitch2<<3;
			}
			for (y=heighta; y<heights; ++y)
			{
				temp1 = (y>>yshifta)*xblocks4;
				temp2 = ((y+yhalfa)>>yshifta)*xblocks4;
				for (x=0; x<widths; x+=2)
				{
					difft = ptr1[x]-ptr2[x];
					difft *= difft;
					box1 = (x>>xshifta)<<2;
					box2 = ((x+xhalfa)>>xshifta)<<2;
					diff[temp1+box1+0] += difft;
					diff[temp1+box2+1] += difft;
					diff[temp2+box1+2] += difft;
					diff[temp2+box2+3] += difft;
				}
				ptr1 += pitch1;
				ptr2 += pitch2;
			}
		}
	}
	__asm emms;
}

void TDecimate::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np)
{
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	for (int b=0; b<np; ++b)
	{
		env->BitBlt(dst->GetWritePtr(plane[b]), dst->GetPitch(plane[b]), src->GetReadPtr(plane[b]),
			src->GetPitch(plane[b]), src->GetRowSize(plane[b]+8), src->GetHeight(plane[b]));
	}
}

int TDecimate::getHint(PVideoFrame &src, int &d2vfilm)
{
	const unsigned char *p = src->GetReadPtr(PLANAR_Y);
	unsigned int i, magic_number = 0, hint = 0;
	int match = -200, field = 0;
	d2vfilm = 0;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*p++ & 1) << i);
	}
	if (magic_number != MAGIC_NUMBER) return match;
	for (i=0; i<32; ++i)
	{
		hint |= ((*p++ & 1) << i);
	}
	if (hint&0xFFFF0000) return match;
	if (hint&TOP_FIELD) field = 1;
	if (hint&D2VFILM) d2vfilm = 1;
	hint &= 0x00000007;
	if (hint == ISC) match = ISC;
	else if (hint == ISP) match = ISP;
	else if (hint == ISN) match = ISN;
	else if (hint == ISB) match = ISB;
	else if (hint == ISU) match = ISU;
	else if (hint == ISDB) match = ISDB;
	else if (hint == ISDT) match = ISDT;
	if (match != -200 && field != 0)
	{
		if (match == 0) match = 3;
		else if (match == 2) match = 4;
		else if (match == 3) match = 0;
		else if (match == 4) match = 2;
	}
	return match;
}

/*
**  This function checks to see if there is a single match dup in the
**  current cycle and if that frame also has the lowest metric in the
**  cycle.  If those conditions are true, then it checks to see if
**  there is also such a frame in the previous or next cycle.  If
**  there is, and if it is in the same position as the one in the
**  current cycle, then the frame in the current cycle is chosen for
**  decimation.
**
**  This function is only used by longest string.
*/
bool TDecimate::checkForObviousDecFrame(Cycle &p, Cycle &c, Cycle &n)
{
	int i, v, dups=0, mc, mp, saved = -20, saved2 = -20;
	unsigned __int64 lowest_metric = ULLONG_MAX;
	for (i=c.cycleS; i<c.cycleE; ++i)
	{
		mp = i == c.cycleS ? (p.cycleE > 0 ? p.match[p.cycleE-1] : -20) : mc;
		mc = c.match[i];
		if (checkMatchDup(mp, mc)) ++dups;
		if (dups > 1) return false;
		if (dups == 1 && saved == -20) saved = i;
		if (c.diffMetricsU[i] < lowest_metric) 
		{
			lowest_metric = c.diffMetricsU[i];
			saved2 = i;
		}
	}
	if (dups != 1) return false;
	if (saved != saved2 || saved2 == -20) return false;
	lowest_metric = ULLONG_MAX;
	int cp = -20, cn = -20;
	for (dups=0, v=-1, i=p.cycleS; i<p.cycleE; ++i)
	{
		mp = i == p.cycleS ? -20 : mc;
		mc = p.match[i];
		if (checkMatchDup(mp, mc)) { ++dups; cp = i; }
		if (p.diffMetricsU[i] < lowest_metric) { v = i; lowest_metric = p.diffMetricsU[i]; }
	}
	if (dups == 0 && v == p.cycleS) { dups = 1; cp = v; }
	if (dups != 1 || cp != v) cp = -20;
	if (cp == -20)
	{
		lowest_metric = ULLONG_MAX;
		for (dups=0, v=-1, i=n.cycleS; i<n.cycleE; ++i)
		{
			mp = i == n.cycleS ? c.match[c.cycleE-1] : mc;
			mc = n.match[i];
			if (checkMatchDup(mp, mc)) { ++dups; cn = i; }
			if (n.diffMetricsU[i] < lowest_metric) { v = i; lowest_metric = n.diffMetricsU[i]; }
		}
		if (dups != 1 || cn != v) return false;
	}
	if (saved != cp && saved != cn) return false;
	if (debug)
	{
		sprintf(buf,"TDecimate:  obvious dec frame found  %d - %d!\n", saved, c.frameSO);
		OutputDebugString(buf);
	}
	c.decimate[saved] = c.decimate2[saved] = 1;
	c.decSet = true;
	return true;
}

/*
**  This function checks to see if there is a single frame in the
**  current cycle marked as a d2v duplicate and if that frame is
**  either a match dup or has the lowest metric.  If so, that frame
**  is chosen for decimation.  If mode=1 (or vfrDec=1), the prev and
**  next cycles must have a d2v duplicate in the same position.
**
**  This function is used by both longest string and most similar.
*/
int TDecimate::checkForD2VDecFrame(Cycle &p, Cycle &c, Cycle &n)
{
	int i, v = 0, mp, savedV = -20, savedL = -20, savedM = -20;
	unsigned __int64 lowest = ULLONG_MAX;
	for (i=c.cycleS; i<c.cycleE; ++i)
	{
		if (c.filmd2v[i] == 1 && (mode == 0 || (mode > 1 && vfrDec == 0) || 
			(p.filmd2v[i] == 1 && n.filmd2v[i] == 1))) 
		{
			++v;
			if (v > 1) return -20;
			savedV = i;
			mp = i == c.cycleS ? (p.cycleE > 0 ? p.match[p.cycleE-1] : -20) : c.match[i-1];
			if (checkMatchDup(mp, c.match[i])) savedM = i;
		}
		if (c.diffMetricsU[i] < lowest)
		{
			lowest = c.diffMetricsU[i];
			savedL = i;
		}
	}
	if (v != 1 || (savedV != savedM && savedV != savedL)) return -20;
	if (debug)
	{
		sprintf(buf,"TDecimate:  d2v dec frame found  %d - %d!\n", savedV, c.frameSO);
		OutputDebugString(buf);
	}
	return savedV;
}

/*  This function checks for the two duplicate due to ivtc pattern
**  change case. It checks the prev and next cycles for single match
**  duplicate with the lowest metric in different positions and the
**  current cycle for 2 match duplicates with the 2 lowest metrics.
**  The two dups must match the positions of dups in the prev and
**  next cycles.  Finally, it requires that no other frames in the
**  current cycle were detected as metric duplicates.
**
**  This function is only used by longest string.
*/
bool TDecimate::checkForTwoDropLongestString(Cycle &p, Cycle &c, Cycle &n)
{
	int dupsP = 0, savedp = -20, dupsC = 0, dupsN = 0, savedn = -20;
	int c1 = -20, c2 = -20, i, v, mp, mc;
	unsigned __int64 lowest = ULLONG_MAX;
	for (v=-1, i=p.cycleS; i<p.cycleE; ++i)
	{
		mp = i == p.cycleS ? -20 : mc;
		mc = p.match[i];
		if (checkMatchDup(mp, mc)) { ++dupsP; savedp = i; }
		if (p.diffMetricsU[i] < lowest) { lowest = p.diffMetricsU[i]; v = i; }
	}
	if (dupsP == 0 && v == p.cycleS) { dupsP = 1; savedp = v; }
	if (dupsP != 1 || savedp != v) return false;
	lowest = ULLONG_MAX;
	for (v=-1, i=n.cycleS; i<n.cycleE; ++i)
	{
		mp = i == n.cycleS ? c.match[c.cycleE-1] : mc;
		mc = n.match[i];
		if (checkMatchDup(mp, mc)) { ++dupsN; savedn = i; }
		if (n.diffMetricsU[i] < lowest) { lowest = n.diffMetricsU[i]; v = i; }
	}
	if (dupsN != 1 || savedn != v || savedn == savedp) return false;
	unsigned __int64 lowest1 = ULLONG_MAX;
	unsigned __int64 lowest2 = ULLONG_MAX;
	int cl1 = -20, cl2 = -20;
	for (v=0,c1=-1,c2=-1,i=c.cycleS; i<c.cycleE; ++i)
	{
		mp = i == c.cycleS ? (p.cycleE > 0 ? p.match[p.cycleE-1] : -20) : mc;
		mc = c.match[i];
		if (checkMatchDup(mp, mc))
		{
			++v;
			if (c1 == -1) c1 = i;
			else if (c2 == -1) c2 = i;
		}
		if (c.diffMetricsU[i] < lowest1)
		{
			lowest2 = lowest1;
			cl2 = cl1;
			lowest1 = c.diffMetricsU[i];
			cl1 = i;
		}
		else if (c.diffMetricsU[i] < lowest2)
		{
			lowest2 = c.diffMetricsU[i];
			cl2 = i;
		}
	}
	if (v != 2 || c1 == -1 || c2 == -1 || 
		(c1 != cl1 && c1 != cl2) || (c2 != cl1 && c2 != cl2))
		return false;
	if (c1 != savedp || c2 != savedn) return false;
	if (abs(c1-c2) <= 1) return false;
	for (i=c.cycleS; i<c.cycleE; ++i)
	{
		if (c.dupArray[i] == 1 && c1 != i && c2 != i)
			return false;
	}
	if ((c1 == c.cycleS && (p.dupArray[p.cycleE>0?p.cycleE-1:0] == 1 || c.dupArray[c1+1] == 1)) ||
		(c1 != c.cycleS && (c.dupArray[c1-1] == 1 || c.dupArray[c1+1] == 1))) return false;
	if ((c2 == c.cycleE-1 && (n.dupArray[n.cycleS] == 1 || c.dupArray[c2-1] == 1)) ||
		(c2 != c.cycleE-1 && (c.dupArray[c2-1] == 1 || c.dupArray[c2+1] == 1))) return false;
	if (hybrid == 0 && noblend)
	{
		if (c.diffMetricsU[c1] <= c.diffMetricsU[c2])
			c.decimate[c1] = c.decimate2[c1] = 1;
		else
			c.decimate[c2] = c.decimate2[c2] = 1;
		c.decSet = true;
	}
	else
	{
		c.blend = 3;
		c.decimate[c1] = c.decimate2[c1] = 1;
		c.decimate[c2] = c.decimate2[c2] = 1;
		c.decSet = true;
		if (debug)
		{
			sprintf(buf,"TDecimate:  drop two frames longest string  %d:%d - %d!\n", c1, c2, c.frameSO);
			OutputDebugString(buf);
		}
	}
	return true;
}

void TDecimate::mostSimilarDecDecision(Cycle &p, Cycle &c, Cycle &n, IScriptEnvironment *env)
{
	if (!c.lowSet) c.setLowest(false);
	if (cycleR != 1)
	{
		c.setDecimateLow(c.frameEO-c.frameSO == cycle ? cycleR : 
			max(int(cycleR*(c.frameEO-c.frameSO)/double(cycle)),1), env);
		return;
	}
	if (!p.dupsSet) p.setDupsMatches(p, ovrArray);
	if (!c.dupsSet) c.setDupsMatches(p, ovrArray);
	if (!n.dupsSet) n.setDupsMatches(c, ovrArray);
	int d2vdecf = checkForD2VDecFrame(p, c, n);
	if (c.dupCount <= 0 && d2vdecf == -20)
	{
		c.setDecimateLow(cycleR, env);
		return;
	}
	int i, ovrdups;
	for (ovrdups=0, i=c.cycleS; i<c.cycleE; ++i)
	{
		if (c.decimate[i] != 1) c.decimate[i] = c.decimate2[i] = 0;
		else ++ovrdups;
	}
	if (ovrdups != 0)
	{
		c.setDecimateLow(cycleR, env);
		return;
	}
	for (i=0; i<c.cycleS; ++i) c.decimate[i] = c.decimate2[i] = -20;
	for (i=c.cycleE; i<c.length; ++i) c.decimate[i] = c.decimate2[i] = -20;
	if (d2vdecf != -20)
	{
		c.decimate[d2vdecf] = c.decimate2[d2vdecf] = 1;
		c.decSet = true;
		return;
	}
	if (c.dupCount == 1)
	{
		unsigned __int64 lowest = ULLONG_MAX, lowest2 = ULLONG_MAX;
		int savedc, savedp, savedn, v;
		for (v=-1, i=c.cycleS; i<c.cycleE; ++i)
		{
			if (c.dupArray[i] == 1) savedc = i;
			if (c.diffMetricsU[i] < lowest) { v = i; lowest = c.diffMetricsU[i]; }
		}
		if (savedc == v || (double)(c.diffMetricsU[savedc]*0.9) <= (double)lowest ||
			fabs(((double)lowest*100.0/(double)MAX_DIFF)-c.diffMetricsN[savedc]) < 0.20)
		{
			c.decimate[savedc] = c.decimate2[savedc] = 1;
			c.decSet = true;
			return;
		}
		if (p.dupCount == 1 && n.dupCount == 1)
		{
			for (lowest2 = ULLONG_MAX, i=p.cycleS; i<p.cycleE; ++i)
			{
				if (p.dupArray[i] == 1) savedp = i;
				if (p.diffMetricsU[i] < lowest2) { v = i; lowest2 = p.diffMetricsU[i]; }
			}
			if (savedp != v)
			{
				c.setDecimateLow(cycleR, env);
				return;
			}
			for (lowest2 = ULLONG_MAX, i=n.cycleS; i<n.cycleE; ++i)
			{
				if (n.dupArray[i] == 1) savedn = i;
				if (n.diffMetricsU[i] < lowest2) { v = i; lowest2 = n.diffMetricsU[i]; }
			}
			if (savedn != v || savedp != savedn || savedp != savedc) 
			{
				c.setDecimateLow(cycleR, env);
				return;
			}
			c.decimate[savedc] = c.decimate2[savedc] = 1;
			c.decSet = true;
			return;
		}
		c.setDecimateLow(cycleR, env);
		return;
	}
	else
	{
		unsigned __int64 lowestp = ULLONG_MAX, lowestn = ULLONG_MAX;
		int savedp, savedn, savedc1, savedc2, v;
		if (c.dupCount == 2 && p.dupCount == 1 && n.dupCount == 1)
		{
			for (v=-1, i=p.cycleS; i<p.cycleE; ++i)
			{
				if (p.dupArray[i] == 1) savedp = i;
				if (p.diffMetricsU[i] < lowestp) { v = i; lowestp = p.diffMetricsU[i]; }
			}
			if (savedp != v || v == -1) goto tryother;
			for (v=-1, i=n.cycleS; i<n.cycleE; ++i)
			{
				if (n.dupArray[i] == 1) savedn = i;
				if (n.diffMetricsU[i] < lowestn) { v = i; lowestn = n.diffMetricsU[i]; }
			}
			if (savedn != v || v == -1 || savedn == savedp) goto tryother;
			for (savedc1=-1,savedc2=-1,i=c.cycleS; i<c.cycleE; ++i)
			{
				if (c.dupArray[i] == 1)
				{
					if (savedc1 == -1) savedc1 = i;
					else if (savedc2 == -1) savedc2 = i;
				}
			}
			if (savedc1 != savedp || savedc2 != savedn) goto tryother;
			if (savedc1 != c.lowest[0] && savedc1 != c.lowest[1]) goto tryother;
			if (savedc2 != c.lowest[0] && savedc2 != c.lowest[1]) goto tryother;
			if (abs(savedc1-savedc2) <= 1) goto tryother;
			if (hybrid == 0 && noblend)
			{
				if (c.diffMetricsU[savedc1] <= c.diffMetricsU[savedc2])
					c.decimate[savedc1] = c.decimate2[savedc1] = 1;
				else
					c.decimate[savedc2] = c.decimate2[savedc2] = 1;
				c.decSet = true;
			}
			else
			{
				c.blend = 3;
				c.decimate[savedc1] = c.decimate2[savedc1] = 1;
				c.decimate[savedc2] = c.decimate2[savedc2] = 1;
				c.decSet = true;
				if (debug)
				{
					sprintf(buf,"TDecimate:  drop two frames most similar  %d:%d - %d!\n", savedc1, savedc2, c.frameSO);
					OutputDebugString(buf);
				}
			}
			return;
		}
tryother:
		int savedc = -1;
		unsigned __int64 metricP, metricN, metricPt, metricNt;
		for (v=0, i=c.cycleS; i<c.cycleE; ++i)
		{
			if (c.dupArray[i] == 1)
			{
				if (((i == c.cycleS && p.dupArray[p.cycleE>0?p.cycleE-1:0] == 0) || (i != c.cycleS && c.dupArray[i-1] == 0)) &&
					((i == c.cycleE-1 && n.dupArray[n.cycleS] == 0) || (i != c.cycleE-1 && c.dupArray[i+1] == 0)))
				{
					++v;
					metricPt = i == c.cycleS ? p.diffMetricsU[p.cycleE>0?p.cycleE-1:0] : c.diffMetricsU[i-1];
					metricNt = i == c.cycleE-1 ? n.diffMetricsU[n.cycleS] : c.diffMetricsU[i+1];
					if (savedc == -1 || (metricPt+metricNt > metricP+metricN && 
						metricPt > c.diffMetricsU[i] && metricNt > c.diffMetricsU[i] &&
						fabs(c.diffMetricsN[i]-c.diffMetricsN[c.lowest[0]]) < 1.0))
					{
						savedc = i;
						metricP = metricPt;
						metricN = metricNt;
					}
				}
			}
		}
		bool check = false;
		for (i=0; i<v; ++i)
		{
			if (savedc == c.lowest[i]) { check = true; break; }
		}
		if (!check || savedc == -1)
		{
			c.setDecimateLow(cycleR, env);
			return;
		}
		c.decimate[savedc] = c.decimate2[savedc] = 1;
		c.decSet = true;
		return;
	}
	c.setDecimateLow(cycleR, env);
}

void TDecimate::findDupStrings(Cycle &p, Cycle &c, Cycle &n, IScriptEnvironment *env)
{
	if (!p.dupsSet) p.setDups(dupThresh);
	if (!c.dupsSet) c.setDups(dupThresh);
	if (!n.dupsSet) n.setDups(dupThresh);
	const int dcnt = (cycle+1)>>1;
	unsigned __int64 lowest;
	int temp, i, g, b, f, forward, back, v, w = 0, j;
	int temp1, temp2, temp3, y, dups, ovrdups = 0, d2vdecf = -20;
	if (cycleR == 1) d2vdecf = checkForD2VDecFrame(p, c, n);
	for (temp=0, i=c.cycleS; i<c.cycleE; ++i) temp += c.dupArray[i];
	if (temp == 0 && d2vdecf == -20) 
	{
		if (!c.lowSet) c.setLowest(false);
		c.setDecimateLow(c.frameEO-c.frameSO == cycle ? cycleR : 
			max(int(cycleR*(c.frameEO-c.frameSO)/double(cycle)),1), env);
		return;
	}
	for (ovrdups=0, i=c.cycleS; i<c.cycleE; ++i)
	{
		if (c.decimate[i] != 1) c.decimate[i] = c.decimate2[i] = 0;
		else ++ovrdups;
	}
	for (i=0; i<c.cycleS; ++i) c.decimate[i] = c.decimate2[i] = -20;
	for (i=c.cycleE; i<c.length; ++i) c.decimate[i] = c.decimate2[i] = -20;
	int cycleRt = c.frameEO-c.frameSO == cycle ? cycleR : 
		max(int(cycleR*(c.frameEO-c.frameSO)/double(cycle)),1);
	if (ovrdups >= cycleRt) { c.decSet = true; return; }
	if (cycleR == 1 && checkForObviousDecFrame(p, c, n)) return;
	if (cycleR == 1 && checkForTwoDropLongestString(p, c, n)) return;
	if (cycleR == 1 && cycle > 2 && ovrdups == 0 && c.dupCount > 1 && 
		(p.dupCount == 1 || p.dupCount == 0 || n.dupCount == 1 || n.dupCount == 0)) 
	{
		int p1 = -20, c1 = -20, c2 = -20, n1 = -20, dupcp = 0, usecp = 0;
		for (dupcp=0,i=c.cycleS; i<c.cycleE && dupcp==0; ++i)
		{
			if (c.dupArray[i] == 1 && 
				((i>c.cycleS && c.dupArray[i-1] == 0) || (i==c.cycleS && p.dupArray[p.cycleE>0?p.cycleE-1:0] == 0)) && 
				((i<c.cycleE-1 && c.dupArray[i+1] == 0) || ((i==c.cycleE-1 && n.dupArray[n.cycleS] == 0)))) 
			{ c1 = i; break; }
			if (c.dupArray[i] == 1) ++dupcp;
		}
		for (dupcp=0,i=c.cycleE-1; i>=c.cycleS && dupcp==0; --i)
		{
			if (c.dupArray[i] == 1 && 
				((i>c.cycleS && c.dupArray[i-1] == 0) || (i==c.cycleS && p.dupArray[p.cycleE>0?p.cycleE-1:0] == 0)) && 
				((i<c.cycleE-1 && c.dupArray[i+1] == 0) || ((i==c.cycleE-1 && n.dupArray[n.cycleS] == 0)))) 
			{ c2 = i; break; }
			if (c.dupArray[i] == 1) ++dupcp;
		}
		bool ct1 = false, ct2 = false;
		if ((p.dupCount == 1 || p.dupCount == 0) && c1 != -20)
		{
			if (p.dupCount == 0 && (p.cycleE-p.cycleS == p.length ||
				n.dupCount != c.dupCount)) { p1 = c1; ct1 = true; }
			else
			{
				for (i=p.cycleS; i<p.cycleE; ++i)
				{
					if (p.dupArray[i] == 1) { p1 = i; break; }
				}
			}
			if (p1 == c1) usecp += 1;
		}
		if ((n.dupCount == 1 || n.dupCount == 0) && c2 != -20)
		{
			if (n.dupCount == 0 && (n.cycleE-n.cycleS == n.length ||
				p.dupCount != c.dupCount)) { n1 = c2; ct2 = true; }
			else
			{
				for (i=n.cycleS; i<n.cycleE; ++i)
				{
					if (n.dupArray[i] == 1) { n1 = i; break; }
				}
			}
			if (n1 == c2) usecp += 5;
		}
		if (hybrid == 0 && noblend && usecp == 6)
		{
			if (ct1 && !ct2) usecp = 5;
			else if (!ct1 && ct2) usecp = 1;
			else
			{
				if (c.diffMetricsU[c1] <= c.diffMetricsU[c2]) usecp = 1;
				else usecp = 5;
			}
		}
		if (usecp == 1 || usecp == 5)
		{
			if (usecp == 5) c1 = c2;
			c.decimate[c1] = c.decimate2[c1] = 1;
			if (debug)
			{
				sprintf(buf,"TDecimate:  usecp case %d - %d!\n", usecp, c.frameSO);
				OutputDebugString(buf);
			}
			c.decSet = true;
			return;
		}
		else if (usecp == 6)
		{
			c.blend = 3;
			c.decimate[c1] = c.decimate2[c1] = 1;
			c.decimate[c2] = c.decimate2[c2] = 1;
			if (debug)
			{
				sprintf(buf,"TDecimate:  usecp case %d - %d!\n", usecp, c.frameSO);
				OutputDebugString(buf);
			}
			c.decSet = true;
			return;
		}
	}
	if (d2vdecf != -20)
	{
		c.decimate[d2vdecf] = c.decimate2[d2vdecf] = 1;
		c.decSet = true;
		return;
	}
	int **dupStrings = (int**)malloc(dcnt*sizeof(int*));
	for (int z=0; z<dcnt; ++z)
		dupStrings[z] = (int*)malloc(3*sizeof(int));
	for (i=0; i<dcnt; ++i) 
		dupStrings[i][0] = dupStrings[i][1] = dupStrings[i][2] = -20;
	for (w=0, i=c.cycleS; i<c.cycleE; ++i)
	{
		if (c.dupArray[i] == 0) continue;
		f = b = i;
		forward = back = 0;
		while (c.dupArray[f] == 1 && f < c.cycleE) 
		{
			++forward;
			++f;
		}
		if (f == c.cycleE)
		{
			g = n.cycleS;
			while (n.dupArray[g] == 1 && g < n.cycleE)
			{
				++g;
				++forward;
			}
		}
		while (c.dupArray[b] == 1 && b >= c.cycleS) 
		{
			++back;
			--b;
		}
		if (b < 0)
		{
			g = p.cycleE-1;
			while ((p.dupArray[g] == 1 && p.decimate2[g] != 1) && g >= p.cycleS) 
			{
				++back;
				--g;
			}
		}
		i = f;
		++b;
		dupStrings[w][0] = back + forward - 1;
		dupStrings[w][1] = b;
		dupStrings[w][2] = f;
		++w;
	}
	if (ovrArray != NULL)
	{
		for (i=c.cycleS; i<c.cycleE; ++i)
		{
			if (c.decimate[i] == 1)
			{
				for (v=0; v<w; ++v)
				{
					if (i >= dupStrings[v][1] && i < dupStrings[v][2]) 
					{
						if (dupStrings[v][2]-dupStrings[v][1]-1 <= 0) dupStrings[v][0] = -20;
						else --dupStrings[v][0];
					}
				}
			}
		}
	}
	for (i=1; i<w; ++i) 
	{
		j = i;
		temp1 = dupStrings[i][0];
		temp2 = dupStrings[i][1];
		temp3 = dupStrings[i][2];
		while (j>0 && (dupStrings[j-1][0]<temp1 || (dupStrings[j-1][0] == temp1 && 
			dupStrings[j-1][1]>temp2))) 
		{
			dupStrings[j][0] = dupStrings[j-1][0];
			dupStrings[j][1] = dupStrings[j-1][1];
			dupStrings[j][2] = dupStrings[j-1][2];
			--j;
		}
		dupStrings[j][0] = temp1;
		dupStrings[j][1] = temp2;
		dupStrings[j][2] = temp3;
	}
	for (v=0; v<c.dupCount && v<cycleRt-ovrdups; ++v)
	{
		if (dupStrings[0][0] < 1) break;
		lowest = ULLONG_MAX;
		f = dupStrings[0][1];
		for (dups=0, i=dupStrings[0][1]; i<dupStrings[0][2]; ++i)
		{
			if (c.diffMetricsU[i] < lowest && c.decimate[i] == 0)
			{
				lowest = c.diffMetricsU[i];
				f = i;
			}
			if (c.decimate[i] == 1) ++dups;
		}
		c.decimate[f] = 1;
		y = dupStrings[0][1];
		while (c.decimate2[y] == 1) ++y;
		c.decimate2[y] = 1;
		if (dupStrings[0][2]-dupStrings[0][1]-dups-1 <= 0) dupStrings[0][0] = -20;
		else --dupStrings[0][0];
		j = 0;
		temp1 = dupStrings[0][0];
		temp2 = dupStrings[0][1];
		temp3 = dupStrings[0][2];
		while (j<w-1 && (dupStrings[j+1][0]>temp1 || (dupStrings[j+1][0] == temp1 && 
			dupStrings[j+1][1]<temp2))) 
		{
			dupStrings[j][0] = dupStrings[j+1][0];
			dupStrings[j][1] = dupStrings[j+1][1];
			dupStrings[j][2] = dupStrings[j+1][2];
			++j;
		}
		dupStrings[j][0] = temp1;
		dupStrings[j][1] = temp2;
		dupStrings[j][2] = temp3;
	}
	c.decSet = true;
	if (v<cycleRt-ovrdups)
	{
		c.setLowest(true);
		c.setDecimateLowP(cycleRt-ovrdups-v, env);
	}
	for (int z=0; z<dcnt; ++z)
		free(dupStrings[z]);
	free(dupStrings);
}

void TDecimate::checkVideoMatches(Cycle &p, Cycle &c)
{
	if (!p.mSet || !c.mSet || (c.type != 3 && c.type > 0)) return;
	int dups = 0, mp, mc, i;
	for (i=c.cycleS; i<c.cycleE && dups<=0; ++i)
	{
		if (i == c.cycleS) 
		{
			if (p.frame != c.frame) mp = p.cycleE > 0 ? p.match[p.cycleE-1] : -20;
			else mp = -20;
		}
		else mp = c.match[i-1];
		mc = c.match[i];
		if (mp == 0 && mc == 3) ++dups;
		else if (mp == 1 && (mc == 0 || mc == 3)) ++dups;
		else if (mp == 2 && (mc == 1 || mc == 3 || mc == 4 || mc == 6)) ++dups;
		else if (mp == 3 && mc == 0) ++dups;
		else if (mp == 4 && (mc == 0 || mc == 1 || mc == 2 || mc == 5)) ++dups;
		else if (mp == 5 && mc == 3) ++dups;
		else if (mp == 6 && mc == 0) ++dups;
		else if (mc < 0) ++dups;
	}
	if (dups == 0)
	{
		if (c.type == -1) c.type = 2;
		else if (c.type == 0) c.type = 2;
		else if (c.type == 3) c.type = 4;
	}
	else if (c.type == -1) c.type = 0;
}

bool TDecimate::checkMatchDup(int mp, int mc)
{
	if (mp == 0 && mc == 3) return true;
	else if (mp == 1 && (mc == 0 || mc == 3)) return true;
	else if (mp == 2 && (mc == 1 || mc == 3 || mc == 4 || mc == 6)) return true;
	else if (mp == 3 && mc == 0) return true;
	else if (mp == 4 && (mc == 0 || mc == 1 || mc == 2 || mc == 5)) return true;
	else if (mp == 5 && mc == 3) return true;
	else if (mp == 6 && mc == 0) return true;
	else if (mc < 0) return true;
	return false;
}

void TDecimate::checkVideoMetrics(Cycle &c, double thresh)
{
	if (!c.mSet || (c.type > 0 && c.type != 2)) return;
	int dups = 0, f = c.cycleS, i;
	double min = 999999.0, max = -999999.0, temp;
	if (c.frame == 0) ++f;
	for (i=f; i<c.cycleE; ++i)
	{
		temp = c.diffMetricsN[i];
		if (temp <= thresh) ++dups;
		if (temp < min) min = temp;
		if (temp > max) max = temp;
	}
	if (min == 0.0) min = 0.0001;
	if (dups == 0 || (cve && max/min < 1.6 && max-min < 2.0 && max >= 0.3))
	{
		if (c.type == -1) c.type = 3;
		else if (c.type == 0) c.type = 3;
		else if (c.type == 2) c.type = 4;
	}
	else if (c.type == -1) c.type = 0;
}

void TDecimate::getOvrCycle(Cycle &current, bool mode2)
{
	if (mode2) current.dupCount = 0;
	if (ovrArray == NULL && metricsArray == NULL && metricsOutArray == NULL) return;
	int b = current.cycleS, v = 0, i, p = 0, d = 0, value;
	int numr = current.frameEO-current.frameSO == cycle ? cycleR : 
		max(int(cycleR*(current.frameEO-current.frameSO)/double(cycle)),1);
	for (i=current.frameSO; i<current.frameEO; ++i, ++b)
	{
		if (ovrArray != NULL)
		{
			value = ovrArray[i];
			if (value&DROP_FRAME && (d < numr || mode2))
			{
				if (mode2)
				{
					current.dupArray[b] = 1;
					++current.dupCount;
				}
				else
				{
					current.decimate[b] = current.decimate2[b] = 1;
					++d;
					current.type = 1;
				}
			}
			if (value&VIDEO) ++v;
			if (value&FILM) ++p;
			if (usehints)
			{
				if (value&ISD2VFILM) current.filmd2v[b] = 1;
				if ((value&ISMATCH) != 0x70)
				{
					value = (value&ISMATCH)>>4;
					if (value == ISC) current.match[b] = ISC;
					else if (value == ISP) current.match[b] = ISP;
					else if (value == ISN) current.match[b] = ISN;
					else if (value == ISB) current.match[b] = ISB;
					else if (value == ISU) current.match[b] = ISU;
					else if (value == ISDB) current.match[b] = ISDB;
					else if (value == ISDT) current.match[b] = ISDT;
				}
			}
		}
		bool foundM = false;
		if (metricsArray != NULL)
		{
			if (metricsArray[i<<1] != ULLONG_MAX) 
			{
				current.diffMetricsU[b] = metricsArray[i<<1];
				current.diffMetricsN[b] = (metricsArray[i<<1] * 100.0) / MAX_DIFF;
				foundM = true;
			}
			if (metricsArray[(i<<1)+1] != ULLONG_MAX) current.diffMetricsUF[b] = metricsArray[(i<<1)+1];
		}
		if (metricsOutArray != NULL && !foundM)
		{
			if (metricsOutArray[i<<1] != ULLONG_MAX) 
			{
				current.diffMetricsU[b] = metricsOutArray[i<<1];
				current.diffMetricsN[b] = (metricsOutArray[i<<1] * 100.0) / MAX_DIFF;
			}
			if (metricsOutArray[(i<<1)+1] != ULLONG_MAX) current.diffMetricsUF[b] = metricsOutArray[(i<<1)+1];
		}
	}
	if (v > 0 && v == current.cycleE-current.cycleS && current.type != 1) current.type = 5;
	if (p > 0 && p == current.cycleE-current.cycleS && current.type != 5) current.type = 1;
	current.setIsFilmD2V();
}

void TDecimate::getOvrFrame(int n, unsigned __int64 &metricU, unsigned __int64 &metricF)
{
	metricU = metricF = ULLONG_MAX;
	if (metricsArray != NULL)
	{
		if (metricsArray[n<<1] != ULLONG_MAX) metricU = metricsArray[n<<1];
		if (metricsArray[(n<<1)+1] != ULLONG_MAX) metricF = metricsArray[(n<<1)+1];
	}
	if (metricU != ULLONG_MAX && metricF != ULLONG_MAX) return;
	if (metricsOutArray != NULL)
	{
		if (metricU == ULLONG_MAX && metricsOutArray[n<<1] != ULLONG_MAX) metricU = metricsOutArray[n<<1];
		if (metricF == ULLONG_MAX && metricsOutArray[(n<<1)+1] != ULLONG_MAX) metricF = metricsOutArray[(n<<1)+1];
	}
}

void TDecimate::calcBlendRatios(double &amount1, double &amount2, int &frame1, int &frame2, int n,
								int bframe, int cycleI)
{
	double stepsize = ((double)cycleI)/((double)(cycleI-cycleR));
	double offset = ((cycleI-1)-(stepsize*(cycleI-cycleR-1)))*0.5;
	double pos = bframe + (n%(cycle-cycleR))*stepsize + offset;
	double posf = pos - (int)(pos);
	frame1 = (int)(pos);
	frame2 = (int)(pos+1.0);
	amount1 = 1.0 - posf;
	amount2 = posf;
}

void TDecimate::calcBlendRatios2(double &amount1, double &amount2, int &frame1, int &frame2, int tf,
		Cycle &p, Cycle &c, Cycle &n, int remove)
{
	int i, b, k;
	int cycleI = c.cycleE-c.cycleS;
	int cycleD = cycleI-remove;
	int *lutf = (int *)malloc((cycleI+2)*sizeof(int));
	for (i=0; i<cycleI+2; ++i) lutf[i] = -20;
	double stepsize = ((double)cycleD)/((double)(cycleI));
	double offset = (cycleI-1)*stepsize;
	offset = (offset-int(offset))*0.5;
	double pos = 1 + (tf%p.length)*stepsize - offset;
	double posf = pos - (int)(pos);
	for (b=p.frameEO-1,i=p.cycleE-1; i>=p.cycleS; --i, --b)
	{
		if (p.decimate[i] != 1) 
		{
			lutf[0] = b;
			break;
		}
	}
	for (k=1,b=c.frameSO,i=c.cycleS; i<c.cycleE; ++i, ++b)
	{
		if (c.decimate[i] != 1)
		{
			lutf[k] = b;
			++k;
		}
	}
	for (b=n.frameSO,i=c.cycleS; i<c.cycleE; ++i, ++b)
	{
		if (c.decimate[i] != 1) 
		{
			lutf[k] = b;
			break;
		}
	}
	for (i=0; i<cycleI+2; ++i)
	{
		if (lutf[i] < 0) lutf[i] = 0;
		else if (lutf[i] > nfrms) lutf[i] = nfrms;
	}
	frame1 = lutf[(int)(pos)];
	frame2 = lutf[(int)(pos+1.0)];
	amount1 = 1.0 - posf;
	amount2 = posf;
	free(lutf);
}

void TDecimate::blendFrames(PVideoFrame &src1, PVideoFrame &src2, PVideoFrame &dst,
			double amount1, double amount2, int np, IScriptEnvironment *env)
{
	const unsigned char *srcp1, *srcp2;
	unsigned char *dstp;
	int b, plane, x, y, width, height;
	int s1_pitch, dst_pitch, s2_pitch, widthM;
	bool useMMX = (env->GetCPUFlags()&CPUF_MMX) ? true : false;
	bool useISSE = (env->GetCPUFlags()&CPUF_INTEGER_SSE) ? true : false;
	bool useSSE2 = ((env->GetCPUFlags()&CPUF_SSE2) && IsIntelP4()) ? true : false;
	if (opt != 4)
	{
		if (opt == 0) useMMX = useISSE = useSSE2 = false;
		else if (opt == 1) { useMMX = true; useISSE = useSSE2 = false; }
		else if (opt == 2) { useMMX = useISSE = true; useSSE2 = false; }
		else if (opt == 3) useMMX = useISSE = useSSE2 = true;
	}
	for (b=0; b<np; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		srcp1 = src1->GetReadPtr(plane);
		s1_pitch = src1->GetPitch(plane);
		width = src1->GetRowSize(plane);
		height = src1->GetHeight(plane);
		srcp2 = src2->GetReadPtr(plane);
		s2_pitch = src2->GetPitch(plane);
		dstp = dst->GetWritePtr(plane);
		dst_pitch = dst->GetPitch(plane);
		if (useSSE2 && amount1 == 0.5 && amount2 == 0.5 && 
			!((int(dstp)|int(srcp1)|int(srcp2)|dst_pitch|s1_pitch|s2_pitch)&15))
		{
			widthM = width + ((width%16) == 0 ? 0 : 16-(width%16));
			blend_SSE2_5050(dstp, srcp1, srcp2, widthM, height, dst_pitch, s1_pitch, s2_pitch);
		}
		else if (useISSE && amount1 == 0.5 && amount2 == 0.5)
		{
			widthM = width + ((width%8) == 0 ? 0 : 8-(width%8));
			blend_iSSE_5050(dstp, srcp1, srcp2, widthM, height, dst_pitch, s1_pitch, s2_pitch);
		}
		else if (useSSE2 && !((int(dstp)|int(srcp1)|int(srcp2)|dst_pitch|s1_pitch|s2_pitch)&15))
		{
			widthM = width + ((width%16) == 0 ? 0 : 16-(width%16));
			blend_SSE2_16(dstp, srcp1, srcp2, widthM, height, dst_pitch, s1_pitch, s2_pitch, amount1, amount2);
		}
		else if (useMMX)
		{
			widthM = width + ((width%8) == 0 ? 0 : 8-(width%8));
			blend_MMX_8(dstp, srcp1, srcp2, widthM, height, dst_pitch, s1_pitch, s2_pitch, amount1, amount2);
		}
		else
		{
			for (y=0; y<height; ++y)
			{
				for (x=0; x<width; ++x)
				{
					dstp[x] = (int)(amount1*srcp1[x]+amount2*srcp2[x]+0.5);
				}
				srcp1 += s1_pitch;
				srcp2 += s2_pitch;
				dstp += dst_pitch;
			}
		}
	}
}

void TDecimate::drawBox(PVideoFrame &dst, int blockN, int xblocks, int np)
{
	if (np == 3) drawBoxYV12(dst, blockN, xblocks);
	else drawBoxYUY2(dst, blockN, xblocks);
}

int TDecimate::Draw(PVideoFrame &dst, int x1, int y1, const char *s, int np, int start)
{
	if (np == 3) return DrawYV12(dst, x1, y1, s, start);
	else return DrawYUY2(dst, x1, y1, s, start);
}

void TDecimate::setBlack(PVideoFrame &dst, int np)
{
	unsigned char *dstp;
	int b, plane, x, y, pitch, width, height;
	for (b=0; b<np; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		dstp = dst->GetWritePtr(plane);
		pitch = dst->GetPitch(plane);
		height = dst->GetHeight(plane);
		if (np == 3)
		{
			if (b == 0) memset(dstp, 0, pitch*height);
			else memset(dstp, 128, pitch*height);
		}
		else 
		{
			width = dst->GetRowSize(plane)>>1;
			for (y=0; y<height; ++y)
			{
				for (x=0; x<width; ++x) ((unsigned short *)dstp)[x] = 0x8000;
				dstp += pitch;
			}
		}
	}
}

/* 
** I've copied the following functions:  float_to_frac, reduce_float,
** and FloatToFPS from fps.cpp, which can be obtained from avisynth's
** cvs (avisynth2.cvs.sourceforge.net).
*/

static bool float_to_frac(float input, unsigned &num, unsigned &den)
{
  union { float f; unsigned i; } value;
  unsigned mantissa;
  int exponent;
  value.f = input;
  mantissa = (value.i & 0x7FFFFF) + 0x800000;  // add implicit bit on the left
  exponent = ((value.i & 0x7F800000) >> 23) - 127 - 23;  // remove decimal pt
  while (!(mantissa & 1)) { mantissa >>= 1; exponent += 1; }
  if (exponent < -31) {
    return float_to_frac(float(1.0/input), den, num);
  }
  while( (exponent > 0) && !(mantissa & 0x80000000) ) {
     mantissa <<= 1; exponent -= 1;
  }
  if (exponent > 0) {  // number too large
    num = 0xffffffff;
    den = 1;
    return true; // Out of range!
  }
  num = mantissa;
  den = 1 << (-exponent);
  return false;
}

static bool reduce_float(float value, unsigned &num, unsigned &den)
{
  if (float_to_frac(value, num, den)) return true;
  unsigned n0 = 0, n1 = 1, n2, nx=num;  // numerators
  unsigned d0 = 1, d1 = 0, d2, dx=den;  // denominators
  unsigned a2, ax, amin;  // integer parts of quotients
  unsigned f1 = 0, f2;    // fractional parts of quotients
  while (1)  // calculate convergents
  {
    a2 = nx / dx;
    f2 = nx % dx;
    n2 = n0 + n1 * a2;
    d2 = d0 + d1 * a2;
    if (f2 == 0) break;  // no more convergents (n2 / d2 == input)
    float f = (float)((double)n2/d2);
    if (f == value) break;
    n0 = n1; n1 = n2;
    d0 = d1; d1 = d2;
    nx = dx; dx = f1 = f2;
  }
  if (d2 == 1)
  {
    num = n2;
    den = d2;
  }
  else { // we have been through the loop at least twice
    if ((a2 % 2 == 0) && (d0 * f1 > f2 * d1))
       amin = a2 / 2;  // passed 1/2 a_k admissibility test
    else
       amin = a2 / 2 + 1;
    union { float f; unsigned i; } eps; eps.f = value;
    if (UInt32x32To64(n1, den) > UInt32x32To64(num, d1))
      eps.i -= 1;
    else
      eps.i += 1;
    double r2 = eps.f;
    r2 += value;
    r2 /= 2;
    double yn = n0 - r2*d0;
    double yd = r2*d1 - n1;
    ax = (unsigned)((yn + yd)/yd); // ceiling value
    if (ax < amin) ax = amin;
    num = n0 + n1 * ax;
    den = d0 + d1 * ax;
  }
  return false;
}

void FloatToFPS(double n, unsigned &num, unsigned &den, IScriptEnvironment* env)
{
  if (n <= 0)
    env->ThrowError("TDecimate:  rate must be greater than 0.\n");
  float x;
  unsigned u = (unsigned)(n*1001+0.5);
  x = float((u/30000*30000)/1001.0);
  if (x == (float)n) { num = u; den= 1001; return; }
  x = float((u/24000*24000)/1001.0);
  if (x == (float)n) { num = u; den= 1001; return; }
  if (n < 14.986) {
    u = (unsigned)(30000/n+0.5);
    x = float(30000.0/(u/1001*1001));
    if (x == (float)n) { num = 30000; den= u; return; }
    u = (unsigned)(24000/n+0.5);
    x = float(24000.0/(u/1001*1001));
    if (x == (float)n) { num = 24000; den= u; return; }
  }
  if (reduce_float(float(n), num, den))
    env->ThrowError("TDecimate:  rate value is out of range.\n");
}

AVSValue __cdecl Create_TDecimate(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	int hybrid = args[8].IsInt() ? args[8].AsInt() : 0;
	int vidDetect = args[9].IsInt() ? args[9].AsInt() : 3;
	int cc;
	if (vidDetect >= 3) cc = 1;
	else cc = 2;
	bool chroma = args[26].IsBool() ? args[26].AsBool() : true;
	double dup_thresh = (args[1].IsInt() && args[1].AsInt() == 7) ? (chroma ? 0.4 : 0.5) : (chroma ? 1.1 : 1.4);
	double vid_thresh = (args[1].IsInt() && args[1].AsInt() == 7) ? (chroma ? 3.5 : 4.0) : (chroma ? 1.1 : 1.4);
	int mode = args[1].IsInt() ? args[1].AsInt() : 0;
	int cycle = args[3].IsInt() ? args[3].AsInt() : 5;
	const char *input = args[14].IsString() ? args[14].AsString() : "";
	AVSValue v;
	if ((mode == 0 || mode == 1 || mode == 3) && cycle > 1 && cycle < 26 && !(*input))
		v = new CacheFilter(args[0].AsClip(),cycle*4+1,1,cycle,env);
	else v = args[0].AsClip();
	v = new TDecimate(v.AsClip(),args[1].AsInt(0),args[2].AsInt(1),args[3].AsInt(5),
		args[4].AsFloat(23.976),args[5].AsFloat(dup_thresh),args[6].AsFloat(vid_thresh),
		args[7].AsFloat(15),args[8].AsInt(0),args[9].AsInt(vidDetect),args[10].AsInt(cc),
		args[11].AsInt(cc),args[12].AsString(""),args[13].AsString(""),args[14].AsString(""),
		args[15].AsString(""),args[16].AsString(""),args[17].AsInt(0),args[18].AsInt(32),args[19].AsInt(32),
		args[20].AsBool(false),args[21].AsBool(false),args[22].AsInt(1),args[23].AsBool(false),
		args[24].AsBool(true),args[25].AsBool(false),args[26].AsBool(true),args[27].AsBool(false),
		args[28].AsInt(-200),args[29].AsBool(false),args[30].AsBool(false),args[31].AsBool(true),
		args[32].AsBool(false),args[33].IsBool()?(args[33].AsBool()?1:0):-1,
		args[34].IsClip()?args[34].AsClip():NULL,args[35].AsInt(0),args[36].AsInt(4),env);
	return v;
}

TDecimate::TDecimate(PClip _child, int _mode, int _cycleR, int _cycle, double _rate, 
	double _dupThresh, double _vidThresh, double _sceneThresh, int _hybrid,
	int _vidDetect, int _conCycle, int _conCycleTP, const char* _ovr, 
	const char* _output, const char* _input, const char* _tfmIn, const char* _mkvOut,
	int _nt, int _blockx, int _blocky, bool _debug, bool _display, int _vfrDec, 
	bool _batch, bool _tcfv1, bool _se, bool _chroma, bool _exPP, int _maxndl, bool _m2PA, 
	bool _predenoise, bool _noblend, bool _ssd, int _usehints, PClip _clip2, 
	int _sdlim, int _opt, IScriptEnvironment* env) : GenericVideoFilter(_child), mode(_mode), 
	cycleR(_cycleR), cycle(_cycle), rate(_rate), dupThresh(_dupThresh), vidThresh(_vidThresh), 
	sceneThresh(_sceneThresh), hybrid(_hybrid), vidDetect(_vidDetect), conCycle(_conCycle), 
	conCycleTP(_conCycleTP), ovr(_ovr), output(_output), input(_input), tfmIn(_tfmIn), 
	mkvOut(_mkvOut), nt(_nt), blockx(_blockx), blocky(_blocky), debug(_debug), 
	display(_display), vfrDec(_vfrDec), batch(_batch), tcfv1(_tcfv1), se(_se), 
	chroma(_chroma), exPP(_exPP), maxndl(_maxndl), m2PA(_m2PA), predenoise(_predenoise), 
	noblend(_noblend), ssd(_ssd), clip2(_clip2), sdlim(_sdlim), opt(_opt), 
	prev(5,0), curr(5,0), next(5,0), nbuf(5,0)
{
	diff = metricsArray = metricsOutArray = mode2_metrics = NULL;
	aLUT = mode2_decA = mode2_order = NULL;
	ovrArray = NULL;
	mkvOutF = NULL;
	FILE *f = NULL;
	char linein[1024], *linep, *linet;
	bool tfmFullInfo = false, metricsFullInfo = false;
	fps = (double)(vi.fps_numerator) / (double)(vi.fps_denominator);
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("TDecimate:  YV12 and YUY2 colorspaces only!");
	if (mode < 0 || mode > 7)
		env->ThrowError("TDecimate:  mode must be set to 0, 1, 2, 3, 4, 5, 6, or 7!");
	if (mode == 3 && !(*mkvOut))
		env->ThrowError("TDecimate:  an mkvOut file must be specified in mode 3!");
	if (mode == 5 && !(*mkvOut))
		env->ThrowError("TDecimate:  an mkvOut file must be specified in mode 5!");
	if (mode == 6 && !(*mkvOut))
		env->ThrowError("TDecimate:  an mkvOut file must be specified in mode 6!");
	if (hybrid < 0 || hybrid > 3)
		env->ThrowError("TDecimate:  hybrid must be set to 0, 1, 2, or 3!");
	if (mode == 3 && hybrid != 2)
		env->ThrowError("TDecimate:  mode 3 can only be used with hybrid = 2!");
	if (mode == 5 && hybrid != 2)
		env->ThrowError("TDecimate:  mode 5 can only be used with hybrid = 2!");
	if (mode == 6 && hybrid != 2)
		env->ThrowError("TDecimate:  mode 6 can only be used with hybrid = 2!");
	if (hybrid == 3 && mode > 1)
		env->ThrowError("TDecimate:  hybrid = 3 can only be used with modes 0 and 1!");
	if (hybrid == 1 && mode > 1)
		env->ThrowError("TDecimate:  hybrid = 1 can only be used with modes 0 and 1!");
	if (hybrid > 0 && cycleR > 1)
		env->ThrowError("TDecimate:  hybrid processing is currently limited to cycleR=1 cases only!");
	if (mode < 2 && hybrid > 1 && hybrid != 3)
		env->ThrowError("TDecimate:  only hybrid = 0, 1, or 3 is supported in modes 0 and 1!");
	if (cycleR >= cycle || cycleR <= 0)
		env->ThrowError("TDecimate:  cycleR must be greater than 0 and less than cycle!");
	if (cycle < 2 || cycle > vi.num_frames)
		env->ThrowError("TDecimate:  cycle must be at least 2 and less than or equal to the number of frames in the clip!");
	if (sceneThresh < 0.0 || sceneThresh > 100.0)
		env->ThrowError("TDecimate:  sceneThresh must be in the range 0 to 100!");
	if (rate >= fps && (mode == 2 || mode == 7))
		env->ThrowError("TDecimate:  mode 2 and 7 - new rate must be less than current rate!");
	if (vidDetect < 0 || vidDetect > 4)
		env->ThrowError("TDecimate:  vidDetect must be set to 0, 1, 2, 3, or 4!");
	if (conCycle > 2)
		env->ThrowError("TDecimate:  conCycle cannot be greater than 2!");
	if (mode == 4 && (*ovr || *tfmIn))
		env->ThrowError("TDecimate:  cannot use an ovr or tfmIn file when in mode 4!");
	if (vfrDec != 0 && vfrDec != 1)
		env->ThrowError("TDecimate:  vfrDec must be set to 0 or 1!");
	if (*output && (mode == 5 || mode == 6))
		env->ThrowError("TDecimate:  output not supported in mode 5 and 6 (you should already have the metrics)!");
	if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 && 
		blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
		env->ThrowError("TDecimate:  illegal blockx size!");
	if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 && 
		blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
		env->ThrowError("TDecimate:  illegal blocky size!");
	if (mode == 2 && maxndl != -200 && (maxndl < 1 || maxndl > 99))
		env->ThrowError("TDecimate:  maxndl must be set to a value between 1 and 99 inclusive!");
	if ((mode != 0 && mode != 1 && mode != 3) || cycleR == 1)
		sdlim = 0;
	if ((abs(sdlim)+1)*(cycleR-1) >= cycle)
		env->ThrowError("TDecimate:  invalid sdlim setting (%d through %d (inclusive) are allowed)!", 0, int(ceil(cycle/double(cycleR-1)))-2);
	if (opt < 0 || opt > 4)
		env->ThrowError("TDecimate:  opt must be set to 0, 1, 2, 3, or 4!");
	if (clip2 && vi.num_frames != clip2->GetVideoInfo().num_frames)
		env->ThrowError("TDecimate:  clip2 must have the same number of frames as the input clip!");
	if (clip2 && !clip2->GetVideoInfo().IsYV12() && !clip2->GetVideoInfo().IsYUY2())
		env->ThrowError("TDecimate:  clip2 must be in YV12 or YUY2 colorspace!");
	if (clip2) 
	{
		clip2->SetCacheHints(CACHE_RANGE, 2);
		useclip2 = true;
	}
	else useclip2 = false;
	if (debug)
	{
		sprintf(buf, "TDecimate:  %s by tritical\n", VERSION);
		OutputDebugString(buf);
	}
	ecf = false;
	if (cycle > 5 && mode != 4 && mode != 6 && mode != 7)
	{
		prev.setSize(cycle);
		curr.setSize(cycle);
		next.setSize(cycle);
		nbuf.setSize(cycle);
	}
	if (sdlim)
	{
		prev.sdlim = sdlim;
		curr.sdlim = sdlim;
		next.sdlim = sdlim;
		nbuf.sdlim = sdlim;
	}
	if (mode == 4 || mode == 5 || mode == 6) child->SetCacheHints(CACHE_RANGE, 3); // fixed to diameter (07/30/2005)
	else if (mode != 2 && mode != 7)
	{
		int cacheRange = cycle*4+1;
		if (cacheRange < 1) cacheRange = 1;
		if (*input || cycle >= 26)
		{
			if (cacheRange > 100) child->SetCacheHints(CACHE_ALL, 999);
			else child->SetCacheHints(CACHE_RANGE, cacheRange);
		}
		else 
		{
			ecf = true;
			child->SetCacheHints(0, -20);
		}
	}
	if (_usehints == 0) usehints = false;
	else if (_usehints == 1) usehints = true;
	else
	{
		int d2hg;
		PVideoFrame sthg = child->GetFrame(0, env);
		int mhg = getHint(sthg,d2hg);
		if (mhg != -200) usehints = true;
		else usehints = false;
	}
	if (vidDetect == 4)
	{
		vidDetect = 3;
		cve = true;
	}
	else cve = false;
	lastn = -1;
	fullInfo = false;
	same_thresh = diff_thresh = 0;
	linearCount = -342;
	mode2_num = mode2_den = mode2_numCycles = -20; 
	memset(mode2_cfs, 0, 10*sizeof(int));
	nfrms = nfrmsN = vi.num_frames - 1;
	prev.length = curr.length = next.length = nbuf.length = cycle;
	prev.maxFrame = curr.maxFrame = next.maxFrame = nbuf.maxFrame = nfrms;
	xshiftS = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
		blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 : 
		blockx == 1024 ? 10 : 11;
	yshiftS = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
		blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 : 
		blocky == 1024 ? 10 : 11;
	yhalfS = blocky >> 1;
	xhalfS = blockx >> 1;
	useTFMPP = false;
	try 
	{
		AVSValue getTFMPP = env->GetVar("TFMPPValue");
		if (getTFMPP.IsInt() && getTFMPP.AsInt() > 1) useTFMPP = true;
	}
	catch (IScriptEnvironment::NotFound) {  }
	if (exPP) useTFMPP = true;
	if (vi.IsYV12()) 
	{
		if (chroma)
		{
			if (ssd) MAX_DIFF = (unsigned __int64)(sqrt(219.0*219.0*blockx*blocky+224.0*224.0*xhalfS*yhalfS*2.0));
			else MAX_DIFF = (unsigned __int64)(219.0*blockx*blocky+224.0*xhalfS*yhalfS*2.0);
		}
		else
		{
			if (ssd) MAX_DIFF = (unsigned __int64)(sqrt(219.0*219.0*blockx*blocky));
			else MAX_DIFF = (unsigned __int64)(219.0*blockx*blocky);
		}
		if (ssd)
		{
			sceneThreshU = (unsigned __int64)((sceneThresh*sqrt(219.0*219.0*vi.height*vi.width))/100.0);
			sceneDivU = (unsigned __int64)(sqrt(219.0*219.0*vi.width*vi.height));
		}
		else
		{
			sceneThreshU = (unsigned __int64)((sceneThresh*219.0*vi.height*vi.width)/100.0);
			sceneDivU = (unsigned __int64)(219.0*vi.width*vi.height);
		}
	}
	else
	{
		if (chroma)
		{
			if (ssd) MAX_DIFF = (unsigned __int64)(sqrt(219.0*219.0*blockx*blocky+224.0*224.0*xhalfS*yhalfS*4.0*0.625));
			else MAX_DIFF = (unsigned __int64)(219.0*blockx*blocky+224.0*xhalfS*yhalfS*4.0*0.625);
		}
		else
		{
			if (ssd) MAX_DIFF = (unsigned __int64)(sqrt(219.0*219.0*blockx*blocky));
			else MAX_DIFF = (unsigned __int64)(219.0*blockx*blocky);
		}
		if (ssd)
		{
			sceneThreshU = (unsigned __int64)((sceneThresh*sqrt(219.0*219.0*vi.width*vi.height))/100.0);
			sceneDivU = (unsigned __int64)(sqrt(219.0*219.0*vi.width*vi.height));
		}
		else
		{
			sceneThreshU = (unsigned __int64)((sceneThresh*219.0*vi.width*vi.height)/100.0);
			sceneDivU = (unsigned __int64)(219.0*vi.width*vi.height);
		}
	}
	if (mode < 5 || mode == 7)
	{
		diff = (unsigned __int64 *)_aligned_malloc((((vi.width+xhalfS)>>xshiftS)+1)*(((vi.height+yhalfS)>>yshiftS)+1)*4*sizeof(unsigned __int64), 16);
		if (diff == NULL) env->ThrowError("TDecimate:  malloc failure (diff)!");
	}
	if (*output)
	{
		if ((f = fopen(output, "w")) != NULL) 
		{
			_fullpath(outputFull, output, MAX_PATH);
			calcCRC(child, 15, outputCrc, env);
			fclose(f);
			f = NULL;
			metricsOutArray = (unsigned __int64 *)malloc(vi.num_frames*2*sizeof(unsigned __int64));
			if (metricsOutArray == NULL)
				env->ThrowError("TDecimate:  malloc failure (metricsOutArray)!");
			for (int h=0; h<vi.num_frames*2; ++h) metricsOutArray[h] = ULLONG_MAX;
		}
		else env->ThrowError("TDecimate:  output error (cannot create output file)!");
	}
	if (*input)
	{
		metricsArray = (unsigned __int64 *)malloc(vi.num_frames*2*sizeof(unsigned __int64));
		if (metricsArray == NULL) env->ThrowError("TDecimate:  malloc failure (metricsArray)!");
		for (int h=0; h<vi.num_frames*2; ++h)
		{
			if (!batch || (mode != 5 && mode != 6)) metricsArray[h] = ULLONG_MAX;
			else metricsArray[h] = 0;
		}
		if ((f = fopen(input, "r")) != NULL)
		{
			unsigned __int64 metricU, metricF;
			int w;
			while (fgets(linein, 1024, f) != NULL)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == '#' || linein[0] == ';') 
					continue;
				linep = linein;
				while (*linep != ' ' && *linep != 0 && *linep != 'c') *linep++;
				if (*linep == 'c')
				{
					if (strnicmp(linein, "crc32 = ", 8) == 0)
					{
						linet = linein;
						while(*linet != ' ') *linet++;
						*linet++;
						while(*linet != ' ') *linet++;
						*linet++;
						unsigned int z, tempCrc;
						sscanf(linet, "%x", &z);
						calcCRC(child, 15, tempCrc, env);
						if (tempCrc != z && !batch)
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  crc32 in input file does not match that of the current clip (%#x vs %#x)!",
								z, tempCrc);
						}
						linep = linein;
						while (*linep != ',' && linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++; *linep++;
						int j;
						if (strnicmp(linep, "blockx = ", 9) == 0)
						{
							while(*linep != '=') *linep++;
							*linep++; *linep++;
							sscanf(linep, "%d", &j);
							if (j != blockx)
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  current blockx value does not match" \
										        " that which was used to create the given input file!");
							}
						}
						linep = linein;
						while (*linep != ',' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++;
						while (*linep != ',' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++; *linep++;
						if (strnicmp(linep, "blocky = ", 9) == 0)
						{
							while(*linep != '=') *linep++;
							*linep++; *linep++;
							sscanf(linep, "%d", &j);
							if (j != blocky)
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  current blocky value does not match" \
										        " that which was used to create the given input file!");
							}
						}
						linep = linein;
						while (*linep != ',' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++;
						while (*linep != ',' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++;
						while (*linep != ',' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++; *linep++;
						if (strnicmp(linep, "chroma = ", 9) == 0)
						{
							while(*linep != '=') *linep++;
							*linep++; *linep++;
							sscanf(linep, "%c", &j);
							if (((j == 'T' || j == 't') && !chroma) || ((j == 'F' || j == 'f') && chroma))
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  current chroma setting does not match" \
										        " that which was used to create the given input file!");
							}
						}
					}
				}
				else if (*linep == ' ' && *(linep+1) != 0 && *(linep+1) != ' ')
				{
					sscanf(linein, "%d %I64u %I64u", &w, &metricU, &metricF);
					if (w < 0 || w > nfrms) 
					{
						free(metricsArray);
						metricsArray = NULL;
						fclose(f);
						f = NULL;
						env->ThrowError("TDecimate:  input error (out of range frame #)!");
					}
					metricsArray[w*2] = metricU;
					metricsArray[w*2+1] = metricF;
				}
			}
			fclose(f);
			f = NULL;
			metricsFullInfo = true;
			for (int h=0; h<vi.num_frames*2; h+=2) 
			{
				if (metricsArray[h] == ULLONG_MAX)
				{
					metricsFullInfo = false;
					if ((mode == 5 || mode == 6) && !batch)
					{
						free(metricsArray);
						metricsArray = NULL;
						env->ThrowError("TDecimate:  input error (mode 5 and 6, all frames must have entries)!");
					}
				}
			}
		}
		else 
		{
			free(metricsArray);
			metricsArray = NULL;
			env->ThrowError("TDecimate:  input error (cannot open input file)!");
		}
	}
	else if (mode == 5)
	{
		metricsArray = (unsigned __int64 *)malloc(vi.num_frames*2*sizeof(unsigned __int64));
		if (metricsArray == NULL) env->ThrowError("TDecimate:  malloc failure (metricsArray)!");
		for (int h=0; h<vi.num_frames*2; h+=2) 
		{
			metricsArray[h+0] = ULLONG_MAX-1;
			metricsArray[h+1] = 0;
		}
	}
	if (*ovr)
	{
		if ((f = fopen(ovr, "r")) != NULL)
		{		
			if (ovrArray == NULL)
			{
				ovrArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
				if (ovrArray == NULL)
				{
					fclose(f);
					f = NULL;
					env->ThrowError("TDecimate:  malloc failure (ovrArray, ovr)!");
				}
				if (!batch || (mode != 5 && mode != 6)) memset(ovrArray, 112, vi.num_frames);
				else memset(ovrArray, 0, vi.num_frames);
			}
			int q, w, z, count = 0;
			while(fgets(linein, 1024, f) != 0)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || linein[0] == '#') 
					continue;
				linep = linein;
				while (*linep != 0 && *linep != ' ' && *linep != ',') *linep++;
				if (*linep == ' ')
				{
					linet = linein;
					while(*linet != 0)
					{
						if (*linet != ' ' && *linet != 10) break;
						*linet++;
					}
					if (*linet == 0) continue;
					*linep++;
					if (*linep == '-' || *linep == '+')
					{
						sscanf(linein, "%d", &z);
						if (z<0 || z>nfrms) 
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  ovr file error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							q = *linep;
							if (q == 45) q = DROP_FRAME;
							else if (q == 43) q = KEEP_FRAME;
							else 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  ovr file error (invalid specifier)!");
							}
							ovrArray[z] &= 0xFC;
							ovrArray[z] |= q;
						}
					}
					else if (*linep == 'f' || *linep == 'v')
					{
						sscanf(linein, "%d", &z);
						if (z<0 || z>nfrms) 
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  ovr file error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							q = *linep;
							if (q == 102) q = FILM;
							else if (q == 118) q = VIDEO;
							else 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  ovr file error (invalid symbol)!");
							}
							ovrArray[z] &= 0xF3;
							ovrArray[z] |= q;
						}
					}
				}
				else if (*linep == ',')
				{
					while (*linep != ' ' && *linep != 0) *linep++;
					if (*linep == 0) continue;
					*linep++;
					if (*linep == 'f' || *linep == 'v')
					{
						sscanf(linein, "%d,%d", &z, &w);
						if (w == 0) w = nfrms;
						if (z<0 || z>nfrms || w<0 || w>nfrms || w<z) 
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  input file error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							q = *linep;
							if (q == 102) q = FILM;
							else if (q == 118) q = VIDEO;
							else 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  input file error (invalid specifier)!");
							}
							while (z<=w)
							{
								ovrArray[z] &= 0xF3;
								ovrArray[z] |= q;
								++z;
							}
						}
					}
					else if (*linep == '-' || *linep == '+')
					{
						sscanf(linein, "%d,%d", &z, &w);
						if (w == 0) w = nfrms;
						if (z<0 || z>nfrms || w<0 || w>nfrms || w<z) 
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  input file error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						*linep++;
						if (*(linep+1) == '-' || *(linep+1) == '+')
						{
							count = 0;
							while ((*linep == '-' || *linep == '+') && (z+count<=w))
							{
								q = *linep;
								if (q == 45) q = DROP_FRAME;
								else if (q == 43) q = KEEP_FRAME;
								else 
								{
									fclose(f);
									f = NULL;
									env->ThrowError("TDecimate:  input file error (invalid specifier)!");
								}
								ovrArray[z+count] &= 0xFC;
								ovrArray[z+count] |= q;
								++count;
								*linep++;
							}
							while (z+count<=w)
							{
								ovrArray[z+count] &= 0xFC;
								ovrArray[z+count] |= (ovrArray[z]&0x03);
								++z;
							}
						}
						else
						{
							q = *linep;
							if (q == 45) q = DROP_FRAME;
							else if (q == 43) q = KEEP_FRAME;
							else 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimatee:  input file error (invalid specifier)!");
							}
							while (z<=w)
							{
								ovrArray[z] &= 0xFC;
								ovrArray[z] |= q;
								++z;
							}
						}
					}
				}
			}
			fclose(f);
			f = NULL;
		}
		else env->ThrowError("TDecimate:  ovr error (could not open ovr file)!");
	}
	if (*tfmIn)
	{
		bool d2vmarked, micmarked;
		if ((f = fopen(tfmIn, "r")) != NULL)
		{
			int fieldt, firstLine, z, q, r;
			if (ovrArray == NULL)
			{
				ovrArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
				if (ovrArray == NULL)
				{
					fclose(f);
					f = NULL;
					env->ThrowError("TDecimate:  malloc failure (ovrArray, tfmIn)!");
				}
				if (!batch || mode != 5) memset(ovrArray,112,vi.num_frames);
				else memset(ovrArray,0,vi.num_frames);
			}
			fieldt = firstLine = 0;
			while (fgets(linein, 1024, f) != NULL)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' ||  linein[0] == ';' || linein[0] == '#') 
					continue;
				++firstLine;
				linep = linein;
				while (*linep != 'f' && *linep != 'F' && *linep != 0 && *linep != ' ' && *linep != 'c') *linep++;
				if (*linep == 'f' || *linep == 'F')
				{
					if (firstLine == 1)
					{
						if (strnicmp(linein, "field = top", 11) == 0) fieldt = 1;
						else if (strnicmp(linein, "field = bottom", 14) == 0) fieldt = 0;
					}
				}
				else if (*linep == ' ')
				{
					linet = linein;
					while(*linet != 0)
					{
						if (*linet != ' ' && *linet != 10) break;
						*linet++;
					}
					if (*linet == 0) { --firstLine; continue; }
					sscanf(linein, "%d", &z);
					linep = linein;
					while (*linep != 'p' && *linep != 'c' && *linep != 'n' && *linep != 'u' &&
							*linep != 'b' && *linep != 'l' && *linep != 'h' && *linep != 0) *linep++;
					if (*linep != 0)
					{
						if (z<0 || z>nfrms)
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TDecimate:  tfmIn file error (out of range frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							*linep++;
							q = *linep;
							if (q == 112) q = 0;
							else if (q == 99) q = 1;
							else if (q == 110) q = 2;
							else if (q == 98) q = 3;
							else if (q == 117) q = 4;
							else if (q == 108) q = 5;
							else if (q == 104) q = 6;
							else 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TDecimate:  tfmIn file error (invalid match specifier)!");
							}
							if (fieldt != 0)
							{
								if (q == 0) q = 3;
								else if (q == 2) q = 4;
								else if (q == 3) q = 0;
								else if (q == 4) q = 2;
							}
							d2vmarked = micmarked = false;
							*linep++;
							while (*linep == ' ' && *linep != 0 && *linep != 10) *linep++;
							if (*linep != 0 && *linep != 10)
							{
								r = *linep;
								if (r == 45 && useTFMPP) q = q;
								else if (r == 43 && q < 5 && useTFMPP)
								{
									if (fieldt == 0) q = 5;
									else q = 6;
								}
								else if (r == '1') d2vmarked = true;
								else if (r == '[') micmarked = true;
								else if (r != 43 && r != 45)
								{
									fclose(f);
									f = NULL;
									env->ThrowError("TDecimate:  tfmIn file error (invalid specifier)!");
								}
							}
							if (!d2vmarked && !micmarked && *linep != 0 && *linep != 10)
							{
								*linep++;
								while (*linep == ' ' && *linep != 0 && *linep != 10) *linep++;
								if (*linep != 0 && *linep != 10)
								{
									r = *linep;
									if (r == '1') d2vmarked = true;
								}
							}
							if (d2vmarked) ovrArray[z] |= ISD2VFILM;
							ovrArray[z] |= 0x70;
							ovrArray[z] &= ((q<<4)|0x8F);
						}
					}
				}
			}
			fclose(f);
			f = NULL;
			tfmFullInfo = true;
			for (int h=0; h<vi.num_frames; ++h)
			{
				if ((ovrArray[h]&ISMATCH) == 0x70)
				{
					tfmFullInfo = false;
					if (mode == 5 && !batch)
					{
						free(ovrArray);
						ovrArray = NULL;
						env->ThrowError("TDecimate:  tfmIn error (mode 5, all frames must have an entry)!");
					}
				}
			}
		}
		else env->ThrowError("TDecimate:  tfmIn file error (could not open file)!");
	}
	else if (mode == 5)
	{
		if (ovrArray == NULL)
		{
			ovrArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
			if (ovrArray == NULL) env->ThrowError("TDecimate:  malloc failure (ovrArray, tfmIn)!");
			memset(ovrArray,16,vi.num_frames);
		}
		else
		{
			for (int i=0; i<vi.num_frames; ++i)
			{
				ovrArray[i] |= 0x70;
				ovrArray[i] &= ((1<<4)|0x8F);
			}
		}
	}
	if (metricsFullInfo && (tfmFullInfo || !usehints)) fullInfo = true;
	else fullInfo = false;
	if (mode < 2)
	{
		if (hybrid != 3)
		{
			vi.num_frames = (vi.num_frames * (cycle-cycleR)) / cycle;
			nfrmsN = vi.num_frames - 1;
			vi.MulDivFPS(cycle-cycleR, cycle);
		}
		else nfrmsN = vi.num_frames - 1;
	}
	else if (mode == 2)
	{
		if (metricsOutArray == NULL)
		{
			metricsOutArray = (unsigned __int64 *)malloc(vi.num_frames*2*sizeof(unsigned __int64));
			if (metricsOutArray == NULL) env->ThrowError("TDecimate:  malloc failure (metricsOutArray)!");
			for (int h=0; h<vi.num_frames*2; ++h) metricsOutArray[h] = ULLONG_MAX;
		}
		mode2_decA = (int *)malloc(vi.num_frames*sizeof(int));
		if (mode2_decA == NULL) env->ThrowError("TDecimate:  malloc failure (mode2_decA)!");
		for (int j=0; j<vi.num_frames; ++j) mode2_decA[j] = -20;
		double arate = buildDecStrategy(env);
		if (mode2_numCycles > 0) 
		{
			if (curr.length < 0)
				env->ThrowError("TDecimate:  unknown error with mode 2!");
			if (curr.length <= 50) child->SetCacheHints(CACHE_RANGE, (curr.length*2)+1);
			else child->SetCacheHints(CACHE_ALL, 999);
			mode2_order = (int*)malloc(max(curr.length+10,100)*sizeof(int));
			mode2_metrics = (unsigned __int64*)malloc(max(curr.length+10,100)*sizeof(unsigned __int64));
		}
		else child->SetCacheHints(CACHE_RANGE, 3);  // fixed to diameter (07/30/2005)
		unsigned int num, den;
		FloatToFPS(arate, num, den, env);
		vi.SetFPS(num, den);
		vi.num_frames = (int)(vi.num_frames * (arate/fps));
		nfrmsN = vi.num_frames - 1;
	}
	else if (mode == 7)
	{
		if (metricsOutArray == NULL)
		{
			metricsOutArray = (unsigned __int64 *)malloc(vi.num_frames*2*sizeof(unsigned __int64));
			if (metricsOutArray == NULL) env->ThrowError("TDecimate:  malloc failure (metricsOutArray)!");
			for (int h=0; h<vi.num_frames*2; ++h) metricsOutArray[h] = ULLONG_MAX;
			metricsOutArray[0] = 0;
		}
		if (aLUT) free(aLUT);
		aLUT = (int *)malloc(vi.num_frames*sizeof(int));
		if (aLUT == NULL)
			env->ThrowError("TDecimate:  malloc failure (aLUT, mode 7)!");
		for (int h=0; h<vi.num_frames; ++h) aLUT[h] = -20;
		unsigned int num, den;
		FloatToFPS(rate,num,den,env);
		vi.SetFPS(num,den);
		vi.num_frames = (int)(vi.num_frames * (rate/fps));
		nfrmsN = vi.num_frames - 1;
		mode2_decA = (int *)malloc(vi.num_frames*sizeof(int));
		if (mode2_decA == NULL) env->ThrowError("TDecimate:  malloc failure (mode2_decA)!");
		for (int j=0; j<vi.num_frames; ++j) mode2_decA[j] = -20;
		child->SetCacheHints(CACHE_RANGE, int((fps/rate)+1.0)*2+3);  // fixed to diameter (07/30/2005)
		diff_thresh = unsigned __int64((vidThresh*MAX_DIFF)/100.0);
		same_thresh = unsigned __int64((dupThresh*MAX_DIFF)/100.0);
	}
	else if (mode == 3)
	{
		mkvfps = (fps*(cycle-cycleR))/cycle;
		mkvfps2 = (fps*(cycle-cycleR-1))/cycle;
		lastGroup = -1;
		lastCycle = -cycle;
		retFrames = -200;
		lastType = linearCount = 0;
		if ((mkvOutF = fopen(mkvOut, "w")) != NULL)
		{
			if (tcfv1)
			{
				fprintf(mkvOutF, "# timecode format v1\n");
				fprintf(mkvOutF, "Assume %4.6f\n", fps);
			}
			else fprintf(mkvOutF, "# timecode format v2\n");
			fprintf(mkvOutF, "# TDecimate %s by tritical\n", VERSION);
			fprintf(mkvOutF, "# Mode 3 - Auto-generated mkv timecodes file\n");
		}
		else env->ThrowError("TDecimate:  mode 3 error (cannot create mkvOut file)!");
	}
	else if (mode == 5)
	{
		mkvfps = (fps*(cycle-cycleR))/cycle;
		mkvfps2 = (fps*(cycle-cycleR-1))/cycle;
		int *input = NULL;
		input = (int*)malloc(vi.num_frames*sizeof(int));
		if (input == NULL) env->ThrowError("TDecimate:  malloc failure (mode 5, input)!");
		memset(input, 0, vi.num_frames*sizeof(int));
		Cycle prevM(5,sdlim), currM(5,sdlim), nextM(5,sdlim);
		if (cycle > 5)
		{
			prevM.setSize(cycle);
			currM.setSize(cycle);
			nextM.setSize(cycle);
		}
		prevM.length = currM.length = nextM.length = cycle;
		prevM.maxFrame = currM.maxFrame = nextM.maxFrame = nfrms;
		bool vid, prevVid;
		int i, h, w, firstkv, countprev, filmC, videoC, longestT, longestV, countVT;
		int count, b, np = vi.IsYV12() ? 3 : 1, passThrough = 0;
twopassrun:
		++passThrough;
		count = 0;
		for (b=0; b<=nfrms; b+=cycle)
		{
			if (b == 0)
			{
				currM.setFrame(0);
				getOvrCycle(currM, false);
				calcMetricCycle(currM, env, np, true, true);
				checkVideoMatches(currM, currM);
				checkVideoMetrics(currM, vidThresh);
			}
			else
			{
				prevM = currM;
				currM = nextM;
			}
			nextM.setFrame(b+cycle);
			getOvrCycle(nextM, false);
			calcMetricCycle(nextM, env, np, true, true);
			checkVideoMatches(currM, nextM);
			checkVideoMetrics(nextM, vidThresh);
			if (passThrough == 1)
			{
				if (currM.type == 5 || (!currM.isfilmd2v && ((currM.type == 2 && (vidDetect == 0 || vidDetect == 2)) ||
					(currM.type == 3 && (vidDetect == 1 || vidDetect == 2)) || (currM.type == 4 && vidDetect == 3))))
				{
					if (currM.type == 5) input[b] = 8;
					if (currM.sceneDetect(prevM, nextM, sceneThreshU) != -20) input[b] = 8;
				}
				else
				{
					if (vfrDec != 1) 
					{
						mostSimilarDecDecision(prevM, currM, nextM, env);
					}
					else
					{
                        prevM.setDups(dupThresh);
						currM.setDups(dupThresh);
						nextM.setDups(dupThresh);
						findDupStrings(prevM, currM, nextM, env);
					}
					for (w=0, i=b; i<b+cycle && i<=nfrms; ++i, ++w)
					{
						if (currM.decimate[w] == 1) input[i] = 2;
					}
				}
			}
			else
			{
				for (vid=true, i=b; i<=nfrms && i<b+cycle; ++i)
				{
					if (input[i] == 2) vid = false;
				}
				if (!vid)
				{
					if (vfrDec != 1) 
					{
						mostSimilarDecDecision(prevM, currM, nextM, env);
					}
					else
					{
                        prevM.setDups(dupThresh);
						currM.setDups(dupThresh);
						nextM.setDups(dupThresh);
						findDupStrings(prevM, currM, nextM, env);
					}
					for (w=0, i=b; i<b+cycle && i<=nfrms; ++i, ++w) 
					{   
						if (currM.decimate[w] == 1)
						{
							input[i] = 2;
							++count;
						}
						else input[i] = 0;
					}
				}
				else
				{
					for (i=b; i<b+cycle && i<=nfrms; ++i) input[i] = 0;
				}
			}
		}
		if (passThrough == 2) { goto finishTP; }
		for (w=0, h=0; h<=nfrms; h+=cycle)
		{
			for (vid=true, i=h; i<h+cycle && i<=nfrms; ++i)
			{
				if (input[i] == 2) vid = false;
			}
			if (vid) ++w;
			else
			{
				if (w > 0 && w < conCycleTP)
				{
					for (i=max(0,h-w*cycle); i<h && i <= nfrms; i+=cycle) 
					{
						if (input[i] != 8) input[i] = 2;
					}
				}
				w = 0;
			}
		}
		if (w > 0 && w < conCycleTP)
		{
			for (i=h-w*cycle; i<h && i <= nfrms; i+=cycle) 
			{
				if (input[i] != 8) input[i] = 2;
			}
		}
		goto twopassrun;
finishTP:
		if (metricsArray != NULL)
		{
			free(metricsArray);
			metricsArray = NULL;
		}
		if (ovrArray != NULL)
		{
			free(ovrArray);
			ovrArray = NULL;
		}
		vi.MulDivFPS(vi.num_frames-count, vi.num_frames);
		vi.num_frames = vi.num_frames - count;
		if ((f = fopen(mkvOut, "w")) != NULL)
		{
			double timestamp = 0.0;
			double sample1 = 1000.0/fps;
			double sample2 = 1000.0/mkvfps;
			double sample3 = 1000.0/mkvfps2;
			int ddup;
			if (tcfv1)
			{
				fprintf(f, "# timecode format v1\n");
				fprintf(f, "Assume %4.6f\n", fps);
			}
			else fprintf(f, "# timecode format v2\n");
			fprintf(f, "# TDecimate %s by tritical\n", VERSION);
			fprintf(f, "# Mode 5 - Auto-generated mkv timecodes file\n");
			firstkv = countprev = 0;
			vid = prevVid = true;
			filmC = videoC = longestT = longestV = countVT = 0;
			for (count=0, b=0; b<=nfrms; b+=cycle)
			{
				prevVid = vid;
				countprev = count;
				vid = true;
				for (i=b,ddup=0; i<b+cycle && i<=nfrms; ++i)
				{
					if (input[i] == 2) 
					{
						++ddup;
						if (ddup < 2) filmC += (b+cycle <= nfrms ? cycle : nfrms-b+1);
						vid = false;
					}
					else ++count;
				}
				if (vid)
				{
					if (!tcfv1)
					{
						int stop = (b+cycle <= nfrms ? cycle : nfrms-b+1);
						for (int x=0; x<stop; ++x)
						{
							fprintf(f, "%3.6f\n", timestamp);
							timestamp += sample1;
						}
					}
					videoC += (b+cycle <= nfrms ? cycle : nfrms-b+1);
					longestT += (b+cycle <= nfrms ? cycle : nfrms-b+1);
				}
				else if (!tcfv1)
				{
					if (ddup == 1)
					{
						int stop = (b+cycle <= nfrms ? cycle-cycleR : nfrms-b+1-cycleR);
						for (int x=0; x<stop; ++x)
						{
							fprintf(f, "%3.6f\n", timestamp);
							timestamp += sample2;
						}
					}
					else if (ddup == 2)
					{
						int stop = (b+cycle <= nfrms ? cycle-cycleR-1 : nfrms-b+1-cycleR-1);
						for (int x=0; x<stop; ++x)
						{
							fprintf(f, "%3.6f\n", timestamp);
							timestamp += sample3;
						}
					}
					else env->ThrowError("TDecimate:  unknown mode 5 error (tc file creation)!");
				}
				else if (ddup == 2)
				{
					if (!prevVid) fprintf(f, "%d,%d,%4.6f\n", firstkv, countprev-1, mkvfps);
					fprintf(f, "%d,%d,%4.6f\n", countprev, countprev+cycle-cycleR-2, mkvfps2);
					firstkv = countprev+cycle-cycleR-1;
				}
				if (prevVid != vid && countprev != 0 && ddup != 2 && countprev > firstkv)
				{
					if (!prevVid && tcfv1) fprintf(f, "%d,%d,%4.6f\n", firstkv, countprev-1, mkvfps);
					firstkv = countprev;
				}
				else if (prevVid != vid && ddup != 2) firstkv = countprev;
				if (prevVid != vid && prevVid && countprev != 0)
				{
					if (longestT > longestV) longestV = longestT;
					++countVT;
					longestT = 0;
				}
			}
			if (!vid && tcfv1) fprintf(f, "%d,%d,%4.6f\n", firstkv, count-1, mkvfps);
			double filmCf = ((double)(filmC)/(double)(nfrms+1))*100.0;
			double videoCf = ((double)(videoC)/(double)(nfrms+1))*100.0;
			fprintf(f, "# vfr stats:  %05.2f%c film  %05.2f%c video\n", filmCf, '%', videoCf, '%');
			fprintf(f, "# vfr stats:  %d - film  %d - video  %d - total\n", filmC, videoC, nfrms+1);
			fprintf(f, "# vfr stats:  longest vid section - %d frames\n", longestV);
			fprintf(f, "# vfr stats:  # of detected vid sections - %d", countVT);
			fclose(f);
			f = NULL;
		}
		else
		{
			free(input);
			input = NULL;
			env->ThrowError("TDecimate:  mkvOut file output error (cannot create file)!");
		}
		if (aLUT != NULL) 
		{
			free(aLUT);
			aLUT = NULL;
		}
		aLUT = (int *)malloc((vi.num_frames+1)*sizeof(int));
		if (aLUT == NULL) 
		{
			free(input);
			input = NULL;
			env->ThrowError("TDecimate:  malloc failure (aLUT, mode 5)!");
		}
		memset(aLUT,0,(vi.num_frames+1)*sizeof(int));
		i = w = 0;
		while(i <= nfrms && w <= vi.num_frames-1)
		{
			if (input[i] != 2)
			{
				aLUT[w] = i;
				++w;
			}
			++i;
		}
		free(input);
		input = NULL;
		nfrmsN = vi.num_frames - 1;
	}
	else if (mode == 6)
	{
		int *input = NULL;
		input = (int*)malloc(vi.num_frames*sizeof(int));
		if (input == NULL) env->ThrowError("TDecimate:  malloc failure (mode 6, input)!");
		memset(input, 0, vi.num_frames*sizeof(int));
		int j = 0, k = 0, dups;
		double timestamp = 0.0;
		int lastt = 0, lastf = 0;
		if ((f = fopen(mkvOut, "w")) == NULL)
		{
			free(input);
			env->ThrowError("TDecimate:  unable to create mkvOut file!");
		}
		if (tcfv1)
		{
			fprintf(f, "# timecode format v1\n");
			fprintf(f, "Assume 23.976024\n");
		}
		else fprintf(f, "# timecode format v2\n");
		fprintf(f, "# TDecimate %s by tritical\n", VERSION);
		fprintf(f, "# Mode 6 - Auto-generated mkv timecodes file\n");
		while (j < vi.num_frames)
		{
			dups = 1;
			++j;
			while(j < vi.num_frames && metricsArray[j*2] == 0)
			{
				++dups;
				++j;
			}
			while (dups > 0)
			{
				if (dups == 1) // 119.88012
				{
					if (!tcfv1)
					{
						fprintf(f, "%3.6f\n", timestamp*1000.0);
						timestamp += 0.00834166665833;
					}
					else if (lastt != 1 && lastt > 0)
					{
						if (lastt != 5) fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 1;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 1;
					input[j-1] = 2;
					dups = 0;
					++k;
				}
				else if (dups == 2) // 59.94006
				{
					if (!tcfv1)
					{
						fprintf(f, "%3.6f\n", timestamp*1000.0);
						timestamp += 0.01668333331665;
					}
					else if (lastt != 2 && lastt > 0)
					{
						if (lastt != 5) fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 2;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 2;
					input[j-2] = 2;
					dups = 0;
					++k;
				}
				else if (dups == 3) // 39.96004
				{
					if (!tcfv1)
					{
						fprintf(f, "%3.6f\n", timestamp*1000.0);
						timestamp += 0.02502499997498;
					}
					else if (lastt != 3 && lastt > 0)
					{
						if (lastt != 5) fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 3;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 3;
					input[j-3] = 2;
					dups = 0;
					++k;
				}
				else if ((dups%4) == 0) // 29.97003
				{
					if (!tcfv1)
					{
						int i, repeat = dups>>2;
						for (i=0; i<repeat; ++i)
						{
							fprintf(f, "%3.6f\n", timestamp*1000.0);
							timestamp += 0.03336666663330;
						}
					}
					else if (lastt != 4 && lastt > 0)
					{
						if (lastt != 5) fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 4;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 4;
					k += (dups>>2);
					for (int i=0; i<dups; i+=4) input[j-dups+i] = 2;
					dups = 0;
				}
				else if ((dups%5) == 0) // 23.97602
				{
					if (!tcfv1)
					{
						int i, repeat = dups/5;
						for (i=0; i<repeat; ++i)
						{
							fprintf(f, "%3.6f\n", timestamp*1000.0);
							timestamp += 0.04170834024997;
						}
					}
					else if (lastt != 5 && lastt > 0)
					{
						fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 5;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 5;
					k += (dups/5);
					for (int i=0; i<dups; i+=5) input[j-dups+i] = 2;
					dups = 0;
				}
				else if (dups > 5)
				{
					if (!tcfv1)
					{
						fprintf(f, "%3.6f\n", timestamp*1000.0);
						timestamp += 0.04170834024997;
					}
					else if (lastt != 5 && lastt > 0)
					{
						fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
						lastt = 5;
						lastf = k;
					}
					else if (lastt <= 0) lastt = 5;
					input[j-dups] = 2;
					dups -= 5;
					++k;
				}
			}
		}
		if (tcfv1 && lastt != 5) fprintf(f, "%d,%d,%s\n", lastf, k-1, cfps(lastt));
		vi.num_frames = k;
		if (aLUT != NULL) { free(aLUT); aLUT = NULL; }
		aLUT = (int *)malloc((vi.num_frames+1)*sizeof(int));
		if (aLUT == NULL) 
		{
			free(input);
			fclose(f);
			env->ThrowError("TDecimate:  malloc failure (aLUT, mode 6)!");
		}
		memset(aLUT,0,(vi.num_frames+1)*sizeof(int));
		k = j = 0;
		while(k <= nfrms && j <= vi.num_frames-1)
		{
			if (input[k] == 2)
			{
				aLUT[j] = k;
				++j;
			}
			++k;
		}
		free(input);
		input = NULL;
		fclose(f);
		f = NULL;
		nfrmsN = vi.num_frames - 1;
	}
	if (f != NULL) fclose(f);
	if (clip2) 
	{
		VideoInfo vi2 = clip2->GetVideoInfo();
		vi.width = vi2.width;
		vi.height = vi2.height;
		vi.pixel_type = vi2.pixel_type;
	}
}

TDecimate::~TDecimate()
{
	if (diff != NULL) _aligned_free(diff);
	if (aLUT != NULL) free(aLUT);
	if (ovrArray != NULL) free(ovrArray);
	if (metricsArray != NULL) free(metricsArray);
	if (metricsOutArray != NULL)
	{
		if (*output)
		{
			FILE *f = NULL;
			if ((f = fopen(outputFull, "w")) != NULL)
			{
				unsigned __int64 metricU, metricF;
				fprintf(f, "#TDecimate %s by tritical\n", VERSION);
				fprintf(f, "crc32 = %x, blockx = %d, blocky = %d, chroma = %c\n", outputCrc, blockx, blocky,
					chroma ? 'T' : 'F');
				for (int h=0; h<(nfrms+1)*2; h+=2)
				{
					metricU = metricF = ULLONG_MAX;
					if (metricsOutArray[h] != ULLONG_MAX) metricU = metricsOutArray[h];
					if (metricsOutArray[h+1] != ULLONG_MAX) metricF = metricsOutArray[h+1];
					if (metricU != ULLONG_MAX || metricF != ULLONG_MAX)
						fprintf(f, "%d %I64u %I64u\n", h>>1, metricU, metricF);
				}
				fclose(f);
				f = NULL;
			}
			if (f != NULL) fclose(f);
		}
		free(metricsOutArray);
	}
	if (mkvOutF != NULL) fclose(mkvOutF);
	if (mode2_decA != NULL) free(mode2_decA);
	if (mode2_metrics != NULL) free(mode2_metrics);
	if (mode2_order != NULL) free(mode2_order);
}