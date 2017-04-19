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

#include "TFM.h"

PVideoFrame __stdcall TFM::GetFrame(int n, IScriptEnvironment* env)
{
	if (n < 0) n = 0;
	else if (n > nfrms) n = nfrms;
	PVideoFrame prv = child->GetFrame(n > 0 ? n-1 : 0, env);
	PVideoFrame src = child->GetFrame(n, env);
	PVideoFrame nxt = child->GetFrame(n < nfrms ? n+1 : nfrms, env);
	PVideoFrame dst = env->NewVideoFrame(vi);
	PVideoFrame tmp = env->NewVideoFrame(vi);
	int dfrm = -20, tfrm = -20;
	int mmatch1, nmatch1, nmatch2, mmatch2, fmatch, tmatch;
	int combed = -1, tcombed = -1, xblocks = -20;
	bool d2vfilm = false, d2vmatch = false, isSC = true;
	int np = vi.IsYV12() ? 3 : 1;
	int mics[5] = { -20, -20, -20, -20, -20 };
	int blockN[5] = { -20, -20, -20, -20, -20 };
	order = orderS; 
	mode = modeS; 
	field = fieldS;
	PP = PPS;
	MI = MIS;
	getSettingOvr(n);
	if (order == -1) order = child->GetParity(n) ? 1 : 0;
	if (field == -1) field = order;
	int frstT = field^order ? 2 : 0;
	int scndT = (mode == 2 || mode == 6) ? (field^order ? 3 : 4) : (field^order ? 0 : 2);
	if (debug)
	{
		sprintf(buf,"TFM:  ----------------------------------------\n");
		OutputDebugString(buf);
	}
	if (getMatchOvr(n, fmatch, combed, d2vmatch, 
		flags == 5 ? checkSceneChange(prv, src, nxt, env, n) : false))
	{
		createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
		if (PP > 0 && combed == -1) 
		{
			if (checkCombed(dst, n, env, np, fmatch, blockN, xblocks, mics, false)) 
			{
				if (d2vmatch)
				{
					d2vmatch = false;
					for (int j=0; j<5; ++j) 
						mics[j] = -20;
					goto d2vCJump;
				}
				else combed = 2;
			}
			else combed = 0;
		}
		d2vfilm = d2vduplicate(fmatch, combed, n);
		if (micout > 0)
		{
			for (int i=0; i<5; ++i)
			{
				if (mics[i] == -20 && (i < 3 || micout > 1))
				{
					createWeaveFrame(tmp, prv, src, nxt, env, i, tfrm, np);
					checkCombed(tmp, n, env, np, i, blockN, xblocks, mics, true);
				}
			}
		}
		fileOut(fmatch, combed, d2vfilm, n, mics[fmatch], mics);
		if (display) writeDisplay(dst, np, n, fmatch, combed, true, blockN[fmatch], xblocks, 
			d2vmatch, mics, prv, src, nxt, env);
		if (debug)
		{
			char buft[20];
			if (mics[fmatch] < 0) sprintf(buft,"N/A");
			else sprintf(buft,"%d",mics[fmatch]);
			sprintf(buf,"TFM:  frame %d  - final match = %c %s  MIC = %s  (OVR)\n", n, MTC(fmatch), 
				d2vmatch ? "(D2V)" : "", buft);
			OutputDebugString(buf);
			if (micout > 0)
			{
				if (micout > 1)	
					sprintf(buf,"TFM:  frame %d  - mics: p = %d  c = %d  n = %d  b = %d  u = %d\n",
						n, mics[0], mics[1], mics[2], mics[3], mics[4]);
				else
					sprintf(buf,"TFM:  frame %d  - mics: p = %d  c = %d  n = %d\n",
						n, mics[0], mics[1], mics[2]);
				OutputDebugString(buf);
			}
			sprintf(buf,"TFM:  frame %d  - mode = %d  field = %d  order = %d  d2vfilm = %c\n", n, mode, field, order,
				d2vfilm ? 'T' : 'F');
			OutputDebugString(buf);
			if (combed != -1)
			{
				if (combed == 1) sprintf(buf,"TFM:  frame %d  - CLEAN FRAME  (forced!)\n", n);
				else if (combed == 5) sprintf(buf,"TFM:  frame %d  - COMBED FRAME  (forced!)\n", n);
				else if (combed == 0) sprintf(buf,"TFM:  frame %d  - CLEAN FRAME\n", n);
				else sprintf(buf,"TFM:  frame %d  - COMBED FRAME\n", n);
				OutputDebugString(buf);
			}
		}
		if (usehints || PP >= 2) putHint(dst, fmatch, combed, d2vfilm);
		lastMatch.frame = n;
		lastMatch.match = fmatch;
		lastMatch.field = field;
		lastMatch.combed = combed;
		return dst;
	}
d2vCJump:
	if (mode == 6)
	{
		int thrdT = field^order ? 0 : 2;
		int frthT = field^order ? 4 : 3;
		tcombed = 0;
		if (!slow) fmatch = compareFields(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		else fmatch = compareFieldsSlow(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		if (micmatching > 0)
			checkmm(fmatch,1,frstT,dst,dfrm,tmp,tfrm,prv,src,nxt,env,np,n,blockN,xblocks,mics);
		createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
		if (checkCombed(dst, n, env, np, fmatch, blockN, xblocks, mics, false))
		{
			tcombed = 2;
			if (ubsco) isSC = checkSceneChange(prv, src, nxt, env, n);
			if (isSC) createWeaveFrame(tmp, prv, src, nxt, env, scndT, tfrm, np);			
			if (isSC && !checkCombed(tmp, n, env, np, scndT, blockN, xblocks, mics, false))
			{
				fmatch = scndT;
				tcombed = 0;
				copyFrame(dst, tmp, env, np);
				dfrm = fmatch;
			}
			else
			{
				createWeaveFrame(tmp, prv, src, nxt, env, thrdT, tfrm, np);
				if (!checkCombed(tmp, n, env, np, thrdT, blockN, xblocks, mics, false))
				{
					fmatch = thrdT;
					tcombed = 0;
					copyFrame(dst, tmp, env, np);
					dfrm = fmatch;
				}
				else
				{
					if (isSC) createWeaveFrame(tmp, prv, src, nxt, env, frthT, tfrm, np);
					if (isSC && !checkCombed(tmp, n, env, np, frthT, blockN, xblocks, mics, false))
					{
						fmatch = frthT;
						tcombed = 0;
						copyFrame(dst, tmp, env, np);
						dfrm = fmatch;
					}
				}
			}
		}
		if (combed == -1 && PP > 0) combed = tcombed;
	}
	else if (mode == 7)
	{
		if (debug && lastMatch.frame != n && n != 0)
		{
			sprintf(buf,"TFM:  mode 7 - non-linear access detected!\n");
			OutputDebugString(buf);
		}
		combed = 0;
		bool combed1 = false, combed2 = false;
		if (!slow) fmatch = compareFields(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		else fmatch = compareFieldsSlow(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		createWeaveFrame(dst, prv, src, nxt, env, 1, dfrm, np);
		combed1 = checkCombed(dst, n, env, np, 1, blockN, xblocks, mics, false);
		createWeaveFrame(dst, prv, src, nxt, env, frstT, dfrm, np);
		combed2 = checkCombed(dst, n, env, np, frstT, blockN, xblocks, mics, false);
		if (!combed1 && !combed2)
		{
			createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
			if (field == 0) mode7_field = 1;
			else mode7_field = 0;
		}
		else if (!combed2 && combed1)
		{
			createWeaveFrame(dst, prv, src, nxt, env, frstT, dfrm, np);
			mode7_field = 1;
			fmatch = frstT;
		}
		else if (!combed1 && combed2)
		{
			createWeaveFrame(dst, prv, src, nxt, env, 1, dfrm, np);
			mode7_field = 0;
			fmatch = 1;
		}
		else
		{
			createWeaveFrame(dst, prv, src, nxt, env, 1, dfrm, np);
			combed = 2;
			field = mode7_field;
			fmatch = 1;
		}
	}
	else
	{
		if (!slow) fmatch = compareFields(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		else fmatch = compareFieldsSlow(prv, src, nxt, 1, frstT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
		if (micmatching > 0)
			checkmm(fmatch,1,frstT,dst,dfrm,tmp,tfrm,prv,src,nxt,env,np,n,blockN,xblocks,mics);
		createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
		if (mode > 3 || (mode > 0 && checkCombed(dst, n, env, np, fmatch, blockN, xblocks, mics, false)))
		{
			if (mode < 4) tcombed = 2;
			if (mode != 2)
			{
				if (!slow) tmatch = compareFields(prv, src, nxt, fmatch, scndT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
				else tmatch = compareFieldsSlow(prv, src, nxt, fmatch, scndT, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
				if (micmatching > 0)
					checkmm(tmatch,fmatch,scndT,dst,dfrm,tmp,tfrm,prv,src,nxt,env,np,n,blockN,xblocks,mics);
				createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
			}
			else tmatch = scndT;
			if (tmatch == scndT) 
			{
				if (mode > 3)
				{
					fmatch = tmatch;
					createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
				}
				else if (mode != 2 || !ubsco || checkSceneChange(prv, src, nxt, env, n))
				{
					createWeaveFrame(tmp, prv, src, nxt, env, tmatch, tfrm, np);			
					if (!checkCombed(tmp, n, env, np, tmatch, blockN, xblocks, mics, false))
					{
						fmatch = tmatch;
						tcombed = 0;
						copyFrame(dst, tmp, env, np);
						dfrm = fmatch;
					}
				}
			}
			if ((mode == 3 && tcombed == 2) || (mode == 5 && checkCombed(dst, n, env, np, fmatch, blockN, xblocks, mics, false)))
			{
				tcombed = 2;
				if (!ubsco || checkSceneChange(prv, src, nxt, env, n))
				{
					if (!slow) tmatch = compareFields(prv, src, nxt, 3, 4, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
					else tmatch = compareFieldsSlow(prv, src, nxt, 3, 4, nmatch1, nmatch2, mmatch1, mmatch2, np, n, env);
					if (micmatching > 0)
						checkmm(tmatch,3,4,dst,dfrm,tmp,tfrm,prv,src,nxt,env,np,n,blockN,xblocks,mics);
					createWeaveFrame(tmp, prv, src, nxt, env, tmatch, tfrm, np);
					if (!checkCombed(tmp, n, env, np, tmatch, blockN, xblocks, mics, false))
					{
						fmatch = tmatch;
						tcombed = 0;
						copyFrame(dst, tmp, env, np);
						dfrm = fmatch;
					}
					else
						createWeaveFrame(dst, prv, src, nxt, env, fmatch, dfrm, np);
				}
			}
			if (mode == 5 && tcombed == -1) tcombed = 0;
		}
		if ((mode == 1 || mode == 2 || mode == 3) && tcombed == -1) tcombed = 0;
		if (combed == -1 && PP > 0) combed = tcombed;
		if (PP > 0 && combed == -1)
		{
			if (checkCombed(dst, n, env, np, fmatch, blockN, xblocks, mics, false)) combed = 2;
			else combed = 0;
		}
		if (dfrm != fmatch)
			env->ThrowError("TFM:  internal error (dfrm!=fmatch).  Please report this.\n");
	}
	if (micout > 0 || (micmatching > 0 && mics[fmatch] > 15 && mode != 7 && !(micmatching == 2 && (mode == 0 || mode == 4)) 
		&& (!mmsco || checkSceneChange(prv, src, nxt, env, n))))
	{
		for (int i=0; i<5; ++i)
		{
			if (mics[i] == -20 && (i < 3 || micout > 1 || micmatching > 0))
			{
				createWeaveFrame(tmp, prv, src, nxt, env, i, tfrm, np);
				checkCombed(tmp, n, env, np, i, blockN, xblocks, mics, true);
			}
		}
		if (micmatching > 0 && mode != 7 && mics[fmatch] > 15 &&
			(!mmsco || checkSceneChange(prv, src, nxt, env, n)))
		{
			int i, j, temp1, temp2, order1[5], order2[5] = { 0, 1, 2, 3, 4 };
			for (i=0; i<5; ++i) order1[i] = mics[i];
			for (i=1; i<5; ++i) 
			{
				j = i;
				temp1 = order1[j];
				temp2 = order2[j];
				while (j>0 && order1[j-1]>temp1)
				{
					order1[j] = order1[j-1];
					order2[j] = order2[j-1];
					--j;
				}
				order1[j] = temp1;
				order2[j] = temp2;
			}
			if (micmatching == 1)
			{
othertest:
				if (order1[0]*3 < order1[1] && abs(order1[0]-order1[1]) > 15 &&
					order1[0] < MI && order2[0] != fmatch && 
					(((field^order) && (order2[0] == 1 || order2[0] == 2 || order2[0] == 3)) ||
					(!(field^order) && (order2[0] == 0 || order2[0] == 1 || order2[0] == 4))))
				{
					bool xfield = (field^order) == 0 ? false : true;
					int lmatch = lastMatch.frame == n-1 ? lastMatch.match : -20;
					if (!((order2[0] == 4 && lmatch == 0 && !xfield && (order2[1] == 0 || order2[2] == 0)) || 
						(order2[0] == 3 && lmatch == 2 && xfield && (order2[1] == 2 || order2[2] == 2))))
					{
						micChange(n, fmatch, order2[0], dst, prv, src, nxt, env, np, 
							fmatch, combed, dfrm);
					}
				}
				if (order1[0]*4 < order1[1] && abs(order1[0]-order1[1]) > 30 &&
					order1[0] < MI && order1[1] >= MI && order2[0] != fmatch)
				{
					micChange(n, fmatch, order2[0], dst, prv, src, nxt, env, np, 
						fmatch, combed, dfrm);
				}
			}
			else if (micmatching == 2 || micmatching == 3)
			{
				int try1 = field^order ? 2 : 0, try2, minm, mint, try3, try4;
				if (mode == 1) // p/c + n
				{
					try2 = try1 == 2 ? 0 : 2;
					minm = min(mics[1],mics[try1]);
					if (mics[try2]*3 < minm && mics[try2] < MI && abs(mics[try2]-minm) >= 30 && try2 != fmatch)
						micChange(n, fmatch, try2, dst, prv, src, nxt, env, np, 
								fmatch, combed, dfrm);
				}
				else if (mode == 2) // p/c + u
				{
					try2 = try1 == 2 ? 3 : 4;
					minm = min(mics[1],mics[try1]);
					if (mics[try2]*3 < minm && mics[try2] < MI && abs(mics[try2]-minm) >= 30 && try2 != fmatch)
						micChange(n, fmatch, try2, dst, prv, src, nxt, env, np, 
								fmatch, combed, dfrm);
				}
				else if (mode == 3) // p/c + n + u/b
				{
					try2 = try1 == 2 ? 0 : 2;
					minm = min(mics[1],mics[try1]);
					mint = min(mics[3],mics[4]);
					try3 = try1 == 2 ? (mint == mics[3] ? 3 : 4) : (mint == mics[4] ? 4 : 3); 
					if (mics[try2]*3 < minm && mics[try2] < MI && abs(mics[try2]-minm) >= 30 && try2 != fmatch && 
						fmatch != 3 && fmatch != 4)
					{
						micChange(n, fmatch, try2, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
						minm = mics[try2];
					}
					else if (fmatch == try2) minm = min(mics[try2],minm);
					if (mint*3 < minm && mint < MI && abs(mint-minm) >= 30 && fmatch != 3 && fmatch != 4)
						micChange(n, fmatch, try3, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
				}
				else if (mode == 5) // p/c/n + u/b
				{
					minm = min(mics[0],min(mics[1],mics[2]));
					mint = min(mics[3],mics[4]);
					try3 = try1 == 2 ? (mint == mics[3] ? 3 : 4) : (mint == mics[4] ? 4 : 3); 
					if (mint*3 < minm && mint < MI && abs(mint-minm) >= 30 && fmatch != 3 && fmatch != 4)
						micChange(n, fmatch, try3, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
				}
				else if (mode == 6) // p/c + u + n + b
				{
					try2 = try1 == 2 ? 3 : 4;
					try3 = try1 == 2 ? 0 : 2;
					try4 = try2 == 3 ? 4 : 3;
					minm = min(mics[1],mics[try1]);
					if (mics[try2]*3 < minm && mics[try2] < MI && abs(mics[try2]-minm) >= 30 && fmatch != try2 &&
						fmatch != try3 && fmatch != try4)
					{
						micChange(n, fmatch, try2, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
						minm = mics[try2];
					}
					else if (fmatch == try2) minm = min(mics[try2],minm);
					if (mics[try3]*3 < minm && mics[try3] < MI && abs(mics[try3]-minm) >= 30 && fmatch != try3 &&
						fmatch != try4)
					{
						micChange(n, fmatch, try3, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
						minm = mics[try3];
					}
					else if (fmatch == try3) minm = min(mics[try3],minm);
					if (mics[try4]*3 < minm && mics[try4] < MI && abs(mics[try4]-minm) >= 30 && fmatch != try4)
						micChange(n, fmatch, try4, dst, prv, src, nxt, env, np,
								fmatch, combed, dfrm);
				}
				if (micmatching == 3) { goto othertest; }
			}
		}
	}
	d2vfilm = d2vduplicate(fmatch, combed, n);
	fileOut(fmatch, combed, d2vfilm, n, mics[fmatch], mics);
	if (display) writeDisplay(dst, np, n, fmatch, combed, false, blockN[fmatch], xblocks, 
		d2vmatch, mics, prv, src, nxt, env);
	if (debug)
	{
		char buft[20];
		if (mics[fmatch] < 0) sprintf(buft,"N/A");
		else sprintf(buft,"%d",mics[fmatch]);
		sprintf(buf,"TFM:  frame %d  - final match = %c  MIC = %s\n", n, MTC(fmatch), buft);
		OutputDebugString(buf);
		if (micout > 0 || (micmatching > 0 && mics[0] != -20 && mics[1] != -20 && mics[2] != -20
			                           && mics[3] != -20 && mics[4] != -20))
		{
			if (micout > 1 || micmatching > 0)	
				sprintf(buf,"TFM:  frame %d  - mics: p = %d  c = %d  n = %d  b = %d  u = %d\n",
					n, mics[0], mics[1], mics[2], mics[3], mics[4]);
			else
				sprintf(buf,"TFM:  frame %d  - mics: p = %d  c = %d  n = %d\n",
					n, mics[0], mics[1], mics[2]);
			OutputDebugString(buf);
		}
		sprintf(buf,"TFM:  frame %d  - mode = %d  field = %d  order = %d  d2vfilm = %c\n", n, mode, field, order,
			d2vfilm ? 'T' : 'F');
		OutputDebugString(buf);
		if (combed != -1)
		{
			if (combed == 1) sprintf(buf,"TFM:  frame %d  - CLEAN FRAME  (forced!)\n", n);
			else if (combed == 5) sprintf(buf,"TFM:  frame %d  - COMBED FRAME  (forced!)\n", n);
			else if (combed == 0) sprintf(buf,"TFM:  frame %d  - CLEAN FRAME\n", n);
			else sprintf(buf,"TFM:  frame %d  - COMBED FRAME\n", n);
			OutputDebugString(buf);
		}
	}
	if (usehints || PP >= 2) putHint(dst, fmatch, combed, d2vfilm);
	lastMatch.frame = n;
	lastMatch.match = fmatch;
	lastMatch.field = field;
	lastMatch.combed = combed;
	return dst;
}

void TFM::checkmm(int &cmatch, int m1, int m2, PVideoFrame &dst, int &dfrm, PVideoFrame &tmp, int &tfrm, 
	PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int n,
	int *blockN, int &xblocks, int *mics)
{
	if (cmatch != m1)
	{
		int tx = m1;
		m1 = m2;
		m2 = tx;
	}
	if (dfrm == m1)
		checkCombed(dst,n,env,np,m1,blockN,xblocks,mics,false);
	else if (tfrm == m1)
		checkCombed(tmp,n,env,np,m1,blockN,xblocks,mics,false);
	else
	{
		if (tfrm != m2)
		{
			createWeaveFrame(tmp,prv,src,nxt,env,m1,tfrm,np);
			checkCombed(tmp,n,env,np,m1,blockN,xblocks,mics,false);
		}
		else
		{
			createWeaveFrame(dst,prv,src,nxt,env,m1,dfrm,np);
			checkCombed(dst,n,env,np,m1,blockN,xblocks,mics,false);
		}
	}
	if (mics[m1] < 30)
		return;
	if (dfrm == m2)
		checkCombed(dst,n,env,np,m2,blockN,xblocks,mics,false);
	else if (tfrm == m2)
		checkCombed(tmp,n,env,np,m2,blockN,xblocks,mics,false);
	else
	{
		if (tfrm != m1)
		{
			createWeaveFrame(tmp,prv,src,nxt,env,m2,tfrm,np);
			checkCombed(tmp,n,env,np,m2,blockN,xblocks,mics,false);
		}
		else
		{
			createWeaveFrame(dst,prv,src,nxt,env,m2,dfrm,np);
			checkCombed(dst,n,env,np,m2,blockN,xblocks,mics,false);
		}
	}
	if ((mics[m2]*3 < mics[m1] || (mics[m2]*2 < mics[m1] && mics[m1] > MI)) && 
		abs(mics[m2]-mics[m1]) >= 30 && mics[m2] < MI)
	{
		if (debug)
		{
			sprintf(buf,"TFM:  frame %d  - micmatching override:  %c (%d) to %c (%d)\n", n, 
				MTC(m1), mics[m1], MTC(m2), mics[m2]);
			OutputDebugString(buf);
		}
		cmatch = m2;
	}
}

void TFM::micChange(int n, int m1, int m2, PVideoFrame &dst, PVideoFrame &prv,
	PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env, int np, int &fmatch, 
	int &combed, int &cfrm)
{
	if (debug)
	{
		sprintf(buf,"TFM:  frame %d  - micmatching override:  %c to %c\n", n, 
			MTC(m1), MTC(m2));
		OutputDebugString(buf);
	}
	fmatch = m2;
	combed = 0;
	createWeaveFrame(dst, prv, src, nxt, env, m2, cfrm, np);
}

void TFM::writeDisplay(PVideoFrame &dst, int np, int n, int fmatch, int combed, bool over,
		int blockN, int xblocks, bool d2vmatch, int *mics, PVideoFrame &prv, 
		PVideoFrame &src, PVideoFrame &nxt, IScriptEnvironment *env)
{
	if (combed > 1 && PP > 1) return;
	if (combed > 1 && PP == 1 && blockN != -20)
	{
		if (np == 3) drawBoxYV12(dst, blockN, xblocks);
		else drawBoxYUY2(dst, blockN, xblocks);
	}
	sprintf(buf, "TFM %s by tritical ", VERSION);
	if (np == 3) DrawYV12(dst, 0, 0, buf);
	else DrawYUY2(dst, 0, 0, buf);
	if (PP > 0)
		sprintf(buf, "order = %d  field = %d  mode = %d  MI = %d ", order, field, mode, MI);
	else
		sprintf(buf, "order = %d  field = %d  mode = %d ", order, field, mode);
	if (np == 3) DrawYV12(dst, 0, 1, buf);
	else DrawYUY2(dst, 0, 1, buf);
	if (!over && !d2vmatch) sprintf(buf, "frame: %d  match = %c %s", n, MTC(fmatch),
		((ubsco || mmsco || flags == 5) && checkSceneChange(prv, src, nxt, env, n)) ? " (SC) " : "");
	else if (d2vmatch) sprintf(buf, "frame: %d  match = %c (D2V) %s", n, MTC(fmatch),
		((ubsco || mmsco || flags == 5) && checkSceneChange(prv, src, nxt, env, n)) ? " (SC) " : "");
	else sprintf(buf, "frame: %d  match = %c (OVR) %s", n, MTC(fmatch),
		((ubsco || mmsco || flags == 5) && checkSceneChange(prv, src, nxt, env, n)) ? " (SC) " : "");
	if (np == 3) DrawYV12(dst, 0, 2, buf);
	else DrawYUY2(dst, 0, 2, buf);
	int i = 3;
	if (micout > 0 || (micmatching > 0 && mics[0] != -20 && mics[1] != -20 && mics[2] != -20
			                       && mics[3] != -20 && mics[4] != -20))
	{
		if (micout == 1 && mics[0] != -20 && mics[1] != -20 && mics[2] != -20 && micmatching == 0)
		{
			sprintf(buf,"MICS:  p = %d  c = %d  n = %d ", mics[0], mics[1], mics[2]);
			if (np == 3) DrawYV12(dst, 0, i, buf);
			else DrawYUY2(dst, 0, i, buf);
			++i;
		}
		else if ((micout == 2 && mics[0] != -20 && mics[1] != -20 && mics[2] != -20 &&
			mics[3] != -20 && mics[4] != -20) || micmatching > 0)
		{
			sprintf(buf,"MICS:  p = %d  c = %d  n = %d ", mics[0], mics[1], mics[2]);
			if (np == 3) DrawYV12(dst, 0, i, buf);
			else DrawYUY2(dst, 0, i, buf);
			++i;
			sprintf(buf,"       b = %d  u = %d ", mics[3], mics[4]);
			if (np == 3) DrawYV12(dst, 0, i, buf);
			else DrawYUY2(dst, 0, i, buf);
			++i;
		}
	}
	if (combed != -1)
	{
		if (combed == 1) sprintf(buf, "PP = %d  CLEAN FRAME (forced!) ", PP);
		else if (combed == 5) sprintf(buf, "PP = %d  COMBED FRAME  (forced!) ", PP);
		else if (combed == 0) sprintf(buf, "PP = %d  CLEAN FRAME ", PP);
		else sprintf(buf, "PP = %d  COMBED FRAME ", PP);
		if (mics[fmatch] >= 0) 
		{
			char buft[20];
			sprintf(buft, " MIC = %d ", mics[fmatch]);
			strcat(buf,buft);
		}
		if (np == 3) DrawYV12(dst, 0, i, buf);
		else DrawYUY2(dst, 0, i, buf);
		++i;
	}
	if (d2vpercent >= 0.0)
	{
		sprintf(buf,"%3.1f%s FILM (D2V) ", d2vpercent, "%");
		if (np == 3) DrawYV12(dst, 0, i, buf);
		else DrawYUY2(dst, 0, i, buf);
	}
}

void TFM::getSettingOvr(int n)
{
	if (setArray == NULL || setArraySize <= 0) return;
	for (int x=0; x<setArraySize; x+=4)
	{
		if (n >= setArray[x+1] && n <= setArray[x+2])
		{
			if (setArray[x] == 111) order = setArray[x+3];
			else if (setArray[x] == 102) field = setArray[x+3];
			else if (setArray[x] == 109) mode = setArray[x+3];
			else if (setArray[x] == 80) PP = setArray[x+3];
			else if (setArray[x] == 105) MI = setArray[x+3];
		}
	}
}

bool TFM::getMatchOvr(int n, int &match, int &combed, bool &d2vmatch, bool isSC)
{
	bool combedset = false;
	d2vmatch = false;
	if (ovrArray != NULL && ovrArray[n] != 255)
	{
		int value = ovrArray[n], temp;
		temp = value&0x00000020;
		if (temp == 0 && PP > 0)
		{
			if (value&0x00000010) combed = 5;
			else combed = 1;
			combedset = true;
		}
		temp = value&0x00000007;
		if (temp >= 0 && temp <= 6)
		{
			match = temp;
			if (field != fieldO)
			{
				if (match == 0) match = 3;
				else if (match == 2) match = 4;
				else if (match == 3) match = 0;
				else if (match == 4) match = 2;
			}
			if (match == 5) { combed = 5; match = 1; field = 0; }
			else if (match == 6) { combed = 5; match = 1; field = 1; }
			return true;
		}
	}
	if (flags != 0 && flags != 3 && d2vfilmarray != NULL && (d2vfilmarray[n]&D2VARRAY_MATCH_MASK))
	{
		int ct = (flags == 4 || (flags == 5 && isSC)) ? -1 : 0;
		int temp = d2vfilmarray[n];
		if ((flags == 1 || flags == 4 || flags == 5) && !(temp&(0x1<<6))) return false;
		temp = (temp&D2VARRAY_MATCH_MASK)>>2;
		if (temp != 1 && temp != 2) return false;
		if (temp == 1) { match = 1; combed = combedset ? combed : ct; }
		else if (temp == 2) { match = field^order ? 2 : 0; combed = combedset ? combed : ct; }
		d2vmatch = true;
		return true;
	}
	return false;
}

bool TFM::d2vduplicate(int match, int combed, int n)
{
	if (d2vfilmarray == NULL || d2vfilmarray[n] == 0) return false;
	if (n-1 != lastMatch.frame)
		lastMatch.field = lastMatch.frame = lastMatch.combed = lastMatch.match = -20;
	if ((d2vfilmarray[n]&D2VARRAY_DUP_MASK) == 0x3) // indicates possible top field duplicate
	{
		if (lastMatch.field == 1)
		{
			if ((lastMatch.combed > 1 || lastMatch.match != 3) && field == 1 && 
				(match != 4 || combed > 1)) return true;
			else if ((lastMatch.combed > 1 || lastMatch.match != 3) && field == 0 && 
				combed < 2 && match != 2) return true;
		}
		else if (lastMatch.field == 0)
		{
			if (lastMatch.combed < 2 && lastMatch.match != 0 && field == 1 && 
				(match != 4 || combed > 1)) return true;
			else if (lastMatch.combed < 2 && lastMatch.match != 0 && field == 0 && 
				combed < 2 && match != 2) return true;
		}
	}
	else if ((d2vfilmarray[n]&D2VARRAY_DUP_MASK) == 0x1) // indicates possible bottom field duplicate
	{
		if (lastMatch.field == 1)
		{
			if (lastMatch.combed < 2 && lastMatch.match != 0 && field == 0 && 
				(match != 4 || combed > 1)) return true;
			else if (lastMatch.combed < 2 && lastMatch.match != 0 && field == 1 && 
				combed < 2 && match != 2) return true;
		}
		else if (lastMatch.field == 0)
		{
			if ((lastMatch.combed > 1 || lastMatch.match != 3) && field == 0 && 
				(match != 4 || combed > 1)) return true;
			else if ((lastMatch.combed > 1 || lastMatch.match != 3) && field == 1 && 
				combed < 2 && match != 2) return true;
		}
	}
	return false;
}

void TFM::fileOut(int match, int combed, bool d2vfilm, int n, int MICount, int mics[5])
{
	if (moutArray && MICount != -1) moutArray[n] = MICount;
	if (micout > 0 && moutArrayE)
	{
		int sn = micout == 1 ? 3 : 5;
		for (int i=0; i<sn; ++i)
			moutArrayE[n*sn+i] = mics[i];
	}
	if (outArray == NULL) return;
	if (*output || *outputC)
	{
		if (field != fieldO)
		{
			if (match == 0) match = 3;
			else if (match == 2) match = 4;
			else if (match == 3) match = 0;
			else if (match == 4) match = 2;
		}
		if (match == 1 && combed > 1 && field == 0) match = 5;
		else if (match == 1 && combed > 1  && field == 1) match = 6;
		unsigned char hint = 0;
		hint |= match;
		if (combed > 1) hint |= FILE_COMBED;
		else if (combed >= 0) hint |= FILE_NOTCOMBED;
		if (d2vfilm) hint |= FILE_D2V;
		hint |= FILE_ENTRY;
		outArray[n] = hint;
	}
}

bool TFM::checkCombed(PVideoFrame &src, int n, IScriptEnvironment *env, int np, int match,
					  int *blockN, int &xblocksi, int *mics, bool ddebug)
{
	if (np == 1) return checkCombedYUY2(src, n, env, match, blockN, xblocksi, mics, ddebug);
	else if (np == 3) return checkCombedYV12(src, n, env, match, blockN, xblocksi, mics, ddebug);
	else env->ThrowError("TFM:  an unknown error occured (unknown colorspace)!");
	return false;
}

void TFM::copyFrame(PVideoFrame &dst, PVideoFrame &src, IScriptEnvironment *env, int np)
{
	int plane[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
	for (int b=0; b<np; ++b)
	{
		env->BitBlt(dst->GetWritePtr(plane[b]), dst->GetPitch(plane[b]), src->GetReadPtr(plane[b]),
			src->GetPitch(plane[b]), src->GetRowSize(plane[b]+8), src->GetHeight(plane[b]));
	}
}

int TFM::compareFields(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n,
		IScriptEnvironment *env) 
{
	int b, plane, ret, y, startx, y0a, y1a;
	const unsigned char *prvp, *srcp, *nxtp;
	const unsigned char *curpf, *curf, *curnf;
	const unsigned char *prvpf, *prvnf, *nxtpf, *nxtnf;
	unsigned char *mapp, *mapn;
	int prv_pitch, src_pitch, Width, Widtha, Height, nxt_pitch;
	int prvf_pitch, nxtf_pitch, curf_pitch, stopx, map_pitch;
	int incl = np == 3 ? 1 : mChroma ? 1 : 2;
	int stop = np == 3 ? mChroma ? 3 : 1 : 1;
	unsigned long accumPc = 0, accumNc = 0, accumPm = 0, accumNm = 0;
	norm1 = norm2 = mtn1 = mtn2 = 0;
	for (b=0; b<stop; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		mapp = map->GetPtr(b);
		map_pitch = map->GetPitch(b);
		prvp = prv->GetReadPtr(plane);
		prv_pitch = prv->GetPitch(plane);
		srcp = src->GetReadPtr(plane);
		src_pitch = src->GetPitch(plane);
		Width = src->GetRowSize(plane);
		Widtha = src->GetRowSize(plane+8); // +8 = _ALIGNED
		Height = src->GetHeight(plane);
		nxtp = nxt->GetReadPtr(plane);
		nxt_pitch = nxt->GetPitch(plane);
		startx = np > 1 ? (b == 0 ? 8 : 4) : 16;
		stopx = Width - startx;
		curf_pitch = src_pitch<<1;
		if (b == 0) { y0a = y0; y1a = y1; }
		else { y0a = y0>>1; y1a = y1>>1; }
		if (match1 < 3) 
		{
			curf = srcp + ((3-field)*src_pitch);
			mapp = mapp + ((field == 1 ? 1 : 2)*map_pitch);
		}
		if (match1 == 0)
		{
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match1 == 1)
		{
			prvf_pitch = src_pitch<<1;
			prvpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match1 == 2)
		{
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match1 == 3)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		else if (match1 == 4)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		if (match2 == 0)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match2 == 1)
		{
			nxtf_pitch = src_pitch<<1;
			nxtpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match2 == 2)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match2 == 3)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
		}
		else if (match2 == 4)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
		}
		prvnf = prvpf + prvf_pitch;
		curpf = curf - curf_pitch;
		curnf = curf + curf_pitch;
		nxtnf = nxtpf + nxtf_pitch;
		map_pitch <<= 1;
		mapn = mapp + map_pitch;
		if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
			buildDiffMapPlane2(prvpf-prvf_pitch,nxtpf-nxtf_pitch,mapp-map_pitch,prvf_pitch,
				nxtf_pitch,map_pitch,Height>>1,Widtha,env);
		else
			buildDiffMapPlane2(prvnf-prvf_pitch,nxtnf-nxtf_pitch,mapn-map_pitch,prvf_pitch,
				nxtf_pitch,map_pitch,Height>>1,Widtha,env);
		__asm
		{
			mov y, 2
	yloop:
			mov ecx, y0a
			mov edx, y1a
			cmp ecx, edx
			je xloop_pre
			mov eax, y
			cmp eax, ecx
			jl xloop_pre
			cmp eax, edx
			jle end_yloop
	xloop_pre:
			mov esi, incl
			mov ebx, startx
			mov edi, mapp
			mov edx, mapn
			mov ecx, stopx
	xloop:
			movzx eax, BYTE PTR [edi+ebx]
			shl eax, 2
			add al, BYTE PTR [edx+ebx]
			jnz b1
			add ebx, esi
			cmp ebx, ecx
			jl xloop
			jmp end_yloop
	b1:
			mov edx, curf
			mov edi, curpf
			movzx ecx, BYTE PTR[edx+ebx]
			movzx esi, BYTE PTR[edi+ebx]
			shl ecx, 2
			mov edx, curnf
			add ecx, esi
			mov edi, prvpf
			movzx esi, BYTE PTR[edx+ebx]
			movzx edx, BYTE PTR[edi+ebx]
			add ecx, esi	
			mov edi, prvnf
			movzx esi, BYTE PTR[edi+ebx]
			add edx, esi
			mov edi, edx
			add edx, edx
			sub edi, ecx
			add edx, edi
			jge b2
			neg edx
	b2:
			cmp edx, 23
			jle p1
			add accumPc, edx
			cmp edx, 42
			jle p1
			test eax, 10
			jz p1
			add accumPm, edx
	p1:
			mov edi, nxtpf
			mov esi, nxtnf
			movzx edx, BYTE PTR[edi+ebx]
			movzx edi, BYTE PTR[esi+ebx]
			add edx, edi
			mov esi, edx
			add edx, edx
			sub esi, ecx
			add edx, esi
			jge b3
			neg edx
	b3:
			cmp edx, 23
			jle p2
			add accumNc, edx
			cmp edx, 42
			jle p2
			test eax, 10
			jz p2
			add accumNm, edx
	p2:
			mov esi, incl
			mov ecx, stopx
			mov edi, mapp
			add ebx, esi
			mov edx, mapn
			cmp ebx, ecx
			jl xloop
	end_yloop:
			mov esi, Height
			mov eax, prvf_pitch
			mov ebx, curf_pitch
			mov ecx, nxtf_pitch
			mov edi, map_pitch
			sub esi, 2
			add y, 2
			add mapp, edi
			add prvpf, eax
			add curpf, ebx
			add prvnf, eax
			add curf, ebx
			add nxtpf, ecx
			add curnf, ebx
			add nxtnf, ecx
			add mapn, edi
			cmp y, esi
			jl yloop
		}
	}
	norm1 = (int)((accumPc / 6.0) + 0.5);
	norm2 = (int)((accumNc / 6.0) + 0.5);
	mtn1 = (int)((accumPm / 6.0) + 0.5);
	mtn2 = (int)((accumNm / 6.0) + 0.5);
	// TODO:  improve this decision about whether to use the mtn metrics or
	//        the normal metrics.  mtn metrics give better recognition of
	//        small areas ("mouths")... the hard part is telling when they
	//        are reliable enough to use.
	float c1 = float(max(norm1,norm2))/float(max(min(norm1,norm2),1));
	float c2 = float(max(mtn1,mtn2))/float(max(min(mtn1,mtn2),1));
	float mr = float(max(mtn1,mtn2))/float(max(max(norm1,norm2),1));
	if (((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
		((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
		((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
		((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else if (mr > 0.005 && max(mtn1,mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else
	{
		if (norm1 > norm2) ret = match2;
		else ret = match1;
	}
	if (debug)
	{
		sprintf(buf,"TFM:  frame %d  - comparing %c to %c\n", n, MTC(match1), MTC(match2));
		OutputDebugString(buf);
		sprintf(buf,"TFM:  frame %d  - nmatches:  %d vs %d (%3.1f)  mmatches:  %d vs %d (%3.1f)\n", n, 
			norm1, norm2, c1, mtn1, mtn2, c2);
		OutputDebugString(buf);
	}
	return ret;
}

int TFM::compareFieldsSlow(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env)
{
	if (slow == 2)
		return compareFieldsSlow2(prv, src, nxt, match1, match2, norm1, norm2, mtn1, mtn2, np, n ,env);
	int b, plane, ret, y, startx, y0a, y1a, tp;
	const unsigned char *prvp, *srcp, *nxtp;
	const unsigned char *curpf, *curf, *curnf;
	const unsigned char *prvpf, *prvnf, *nxtpf, *nxtnf;
	unsigned char *mapp, *mapn;
	int prv_pitch, src_pitch, Width, Widtha, Height, nxt_pitch;
	int prvf_pitch, nxtf_pitch, curf_pitch, stopx, map_pitch;
	int incl = np == 3 ? 1 : mChroma ? 1 : 2;
	int stop = np == 3 ? mChroma ? 3 : 1 : 1;
	unsigned long accumPc = 0, accumNc = 0, accumPm = 0;
	unsigned long accumNm = 0, accumPml = 0, accumNml = 0;
	norm1 = norm2 = mtn1 = mtn2 = 0;
	for (b=0; b<stop; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		mapp = map->GetPtr(b);
		map_pitch = map->GetPitch(b);
		prvp = prv->GetReadPtr(plane);
		prv_pitch = prv->GetPitch(plane);
		srcp = src->GetReadPtr(plane);
		src_pitch = src->GetPitch(plane);
		Width = src->GetRowSize(plane);
		Widtha = src->GetRowSize(plane+8); // +8 = _ALIGNED
		Height = src->GetHeight(plane);
		nxtp = nxt->GetReadPtr(plane);
		nxt_pitch = nxt->GetPitch(plane);
		fmemset(env->GetCPUFlags(),mapp,Height*map_pitch,opt);
		startx = np > 1 ? (b == 0 ? 8 : 4) : 16;
		stopx = Width - startx;
		curf_pitch = src_pitch<<1;
		if (b == 0) { y0a = y0; y1a = y1; tp = tpitchy; }
		else { y0a = y0>>1; y1a = y1>>1; tp = tpitchuv; }
		if (match1 < 3) 
		{
			curf = srcp + ((3-field)*src_pitch);
			mapp = mapp + ((field == 1 ? 1 : 2)*map_pitch);
		}
		if (match1 == 0)
		{
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match1 == 1)
		{
			prvf_pitch = src_pitch<<1;
			prvpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match1 == 2)
		{
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match1 == 3)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		else if (match1 == 4)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		if (match2 == 0)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match2 == 1)
		{
			nxtf_pitch = src_pitch<<1;
			nxtpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match2 == 2)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match2 == 3)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
		}
		else if (match2 == 4)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
		}
		prvnf = prvpf + prvf_pitch;
		curpf = curf - curf_pitch;
		curnf = curf + curf_pitch;
		nxtnf = nxtpf + nxtf_pitch;
		map_pitch <<= 1;
		mapn = mapp + map_pitch;
		if (np == 3)
		{
			if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
				buildDiffMapPlaneYV12(prvpf,nxtpf,mapp,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
			else
				buildDiffMapPlaneYV12(prvnf,nxtnf,mapn,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
		}
		else
		{
			if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
				buildDiffMapPlaneYUY2(prvpf,nxtpf,mapp,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
			else
				buildDiffMapPlaneYUY2(prvnf,nxtnf,mapn,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
		}
		__asm
		{
			mov y, 2
	yloop:
			mov ecx, y0a
			mov edx, y1a
			cmp ecx, edx
			je xloop_pre
			mov eax, y
			cmp eax, ecx
			jl xloop_pre
			cmp eax, edx
			jle end_yloop
	xloop_pre:
			mov esi, incl
			mov ebx, startx
			mov edi, mapp
			mov edx, mapn
			mov ecx, stopx
	xloop:
			movzx eax, BYTE PTR [edi+ebx]
			shl eax, 3
			add al, BYTE PTR [edx+ebx]
			jnz b1
			add ebx, esi
			cmp ebx, ecx
			jl xloop
			jmp end_yloop
	b1:
			mov edx, curf
			mov edi, curpf
			movzx ecx, BYTE PTR[edx+ebx]
			movzx esi, BYTE PTR[edi+ebx]
			shl ecx, 2
			mov edx, curnf
			add ecx, esi
			mov edi, prvpf
			movzx esi, BYTE PTR[edx+ebx]
			movzx edx, BYTE PTR[edi+ebx]
			add ecx, esi	
			mov edi, prvnf
			movzx esi, BYTE PTR[edi+ebx]
			add edx, esi
			mov edi, edx
			add edx, edx
			sub edi, ecx
			add edx, edi
			jge b3
			neg edx
	b3:
			cmp edx, 23
			jle p3
			test eax, 9
			jz p1
			add accumPc, edx
	p1:
			cmp edx, 42
			jle p3
			test eax, 18
			jz p2
			add accumPm, edx
	p2:
			test eax, 36
			jz p3
			add accumPml, edx
	p3:
			mov edi, nxtpf
			mov esi, nxtnf
			movzx edx, BYTE PTR[edi+ebx]
			movzx edi, BYTE PTR[esi+ebx]
			add edx, edi
			mov esi, edx
			add edx, edx
			sub esi, ecx
			add edx, esi
			jge b2
			neg edx
	b2:
			cmp edx, 23
			jle p6
			test eax, 9
			jz p4
			add accumNc, edx
	p4:
			cmp edx, 42
			jle p6
			test eax, 18
			jz p5
			add accumNm, edx
	p5:
			test eax, 36
			jz p6
			add accumNml, edx
	p6:
			mov esi, incl
			mov ecx, stopx
			mov edi, mapp
			add ebx, esi
			mov edx, mapn
			cmp ebx, ecx
			jl xloop
	end_yloop:
			mov esi, Height
			mov eax, prvf_pitch
			mov ebx, curf_pitch
			mov ecx, nxtf_pitch
			mov edi, map_pitch
			sub esi, 2
			add y, 2
			add mapp, edi
			add prvpf, eax
			add curpf, ebx
			add prvnf, eax
			add curf, ebx
			add nxtpf, ecx
			add curnf, ebx
			add nxtnf, ecx
			add mapn, edi
			cmp y, esi
			jl yloop
		}
	}
	if (accumPm < 500 && accumNm < 500 && (accumPml >= 500 || accumNml >= 500) &&
		max(accumPml,accumNml) > 3*min(accumPml,accumNml)) 
	{
		accumPm = accumPml;
		accumNm = accumNml;
	}
	norm1 = (int)((accumPc / 6.0) + 0.5);
	norm2 = (int)((accumNc / 6.0) + 0.5);
	mtn1 = (int)((accumPm / 6.0) + 0.5);
	mtn2 = (int)((accumNm / 6.0) + 0.5);
	float c1 = float(max(norm1,norm2))/float(max(min(norm1,norm2),1));
	float c2 = float(max(mtn1,mtn2))/float(max(min(mtn1,mtn2),1));
	float mr = float(max(mtn1,mtn2))/float(max(max(norm1,norm2),1));
	if (((mtn1 >= 375  || mtn2 >= 375)  && (mtn1*3 < mtn2*1 || mtn2*3 < mtn1*1)) ||
		((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
		((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
		((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
		((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else if (mr > 0.005 && max(mtn1,mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else
	{
		if (norm1 > norm2) ret = match2;
		else ret = match1;
	}
	if (debug)
	{
		sprintf(buf,"TFM:  frame %d  - comparing %c to %c  (SLOW 1)\n", n, MTC(match1), MTC(match2));
		OutputDebugString(buf);
		sprintf(buf,"TFM:  frame %d  - nmatches:  %d vs %d (%3.1f)  mmatches:  %d vs %d (%3.1f)\n", n, 
			norm1, norm2, c1, mtn1, mtn2, c2);
		OutputDebugString(buf);
	}
	return ret;
}

int TFM::compareFieldsSlow2(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, int match1, 
		int match2, int &norm1, int &norm2, int &mtn1, int &mtn2, int np, int n, IScriptEnvironment *env)
{
	int b, plane, ret, y, startx, y0a, y1a, tp;
	const unsigned char *prvp, *srcp, *nxtp;
	const unsigned char *curpf, *curf, *curnf;
	const unsigned char *prvpf, *prvnf, *nxtpf, *nxtnf;
	const unsigned char *prvppf, *nxtppf, *prvnnf, *nxtnnf;
	unsigned char *mapp, *mapn;
	int prv_pitch, src_pitch, Width, Widtha, Height, nxt_pitch;
	int prvf_pitch, nxtf_pitch, curf_pitch, stopx, map_pitch;
	int incl = np == 3 ? 1 : mChroma ? 1 : 2;
	int stop = np == 3 ? mChroma ? 3 : 1 : 1;
	unsigned long accumPc = 0, accumNc = 0, accumPm = 0;
	unsigned long accumNm = 0, accumPml = 0, accumNml = 0;
	norm1 = norm2 = mtn1 = mtn2 = 0;
	for (b=0; b<stop; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		mapp = map->GetPtr(b);
		map_pitch = map->GetPitch(b);
		prvp = prv->GetReadPtr(plane);
		prv_pitch = prv->GetPitch(plane);
		srcp = src->GetReadPtr(plane);
		src_pitch = src->GetPitch(plane);
		Width = src->GetRowSize(plane);
		Widtha = src->GetRowSize(plane+8); // +8 = _ALIGNED
		Height = src->GetHeight(plane);
		nxtp = nxt->GetReadPtr(plane);
		nxt_pitch = nxt->GetPitch(plane);
		fmemset(env->GetCPUFlags(),mapp,Height*map_pitch,opt);
		startx = np > 1 ? (b == 0 ? 8 : 4) : 16;
		stopx = Width - startx;
		curf_pitch = src_pitch<<1;
		if (b == 0) { y0a = y0; y1a = y1; tp = tpitchy; }
		else { y0a = y0>>1; y1a = y1>>1; tp = tpitchuv; }
		if (match1 < 3) 
		{
			curf = srcp + ((3-field)*src_pitch);
			mapp = mapp + ((field == 1 ? 1 : 2)*map_pitch);
		}
		if (match1 == 0)
		{
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match1 == 1)
		{
			prvf_pitch = src_pitch<<1;
			prvpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match1 == 2)
		{
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match1 == 3)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = prv_pitch<<1;
			prvpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		else if (match1 == 4)
		{
			curf = srcp + ((2+field)*src_pitch);
			prvf_pitch = nxt_pitch<<1;
			prvpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
			mapp = mapp + ((field == 1 ? 2 : 1)*map_pitch);
		}
		if (match2 == 0)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 1 : 2)*prv_pitch);
		}
		else if (match2 == 1)
		{
			nxtf_pitch = src_pitch<<1;
			nxtpf = srcp + ((field == 1 ? 1 : 2)*src_pitch);
		}
		else if (match2 == 2)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 1 : 2)*nxt_pitch);
		}
		else if (match2 == 3)
		{
			nxtf_pitch = prv_pitch<<1;
			nxtpf = prvp + ((field == 1 ? 2 : 1)*prv_pitch);
		}
		else if (match2 == 4)
		{
			nxtf_pitch = nxt_pitch<<1;
			nxtpf = nxtp + ((field == 1 ? 2 : 1)*nxt_pitch);
		}
		prvppf = prvpf - prvf_pitch;
		prvnf = prvpf + prvf_pitch;
		prvnnf = prvnf + prvf_pitch;
		curpf = curf - curf_pitch;
		curnf = curf + curf_pitch;
		nxtppf = nxtpf - nxtf_pitch;
		nxtnf = nxtpf + nxtf_pitch;
		nxtnnf = nxtnf + nxtf_pitch;
		map_pitch <<= 1;
		mapn = mapp + map_pitch;
		if (np == 3)
		{
			if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
				buildDiffMapPlaneYV12(prvpf,nxtpf,mapp,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
			else
				buildDiffMapPlaneYV12(prvnf,nxtnf,mapn,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
		}
		else
		{
			if ((match1 >= 3 && field == 1) || (match1 < 3 && field != 1))
				buildDiffMapPlaneYUY2(prvpf,nxtpf,mapp,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
			else
				buildDiffMapPlaneYUY2(prvnf,nxtnf,mapn,prvf_pitch,nxtf_pitch,map_pitch,Height,Widtha,tp,env);
		}
		if (field == 0)
		{
			__asm
			{
				mov y, 2
		yloop0:
				mov ecx, y0a
				mov edx, y1a
				cmp ecx, edx
				je xloop_pre0
				mov eax, y
				cmp eax, ecx
				jl xloop_pre0
				cmp eax, edx
				jle end_yloop0
		xloop_pre0:
				mov esi, incl
				mov ebx, startx
				mov edi, mapp
				mov edx, mapn
				mov ecx, stopx
		xloop0:
				movzx eax, BYTE PTR [edi+ebx]
				shl eax, 3
				add al, BYTE PTR [edx+ebx]
				jnz b10
				add ebx, esi
				cmp ebx, ecx
				jl xloop0
				jmp end_yloop0
		b10:
				mov edx, curf
				mov edi, curpf
				movzx ecx, BYTE PTR[edx+ebx]
				movzx esi, BYTE PTR[edi+ebx]
				shl ecx, 2
				mov edx, curnf
				add ecx, esi
				mov edi, prvpf
				movzx esi, BYTE PTR[edx+ebx]
				movzx edx, BYTE PTR[edi+ebx]
				add ecx, esi	
				mov edi, prvnf
				movzx esi, BYTE PTR[edi+ebx]
				add edx, esi
				mov edi, edx
				add edx, edx
				sub edi, ecx
				add edx, edi
				jge b30
				neg edx
		b30:
				cmp edx, 23
				jle p30
				test eax, 9
				jz p10
				add accumPc, edx
		p10:
				cmp edx, 42
				jle p30
				test eax, 18
				jz p20
				add accumPm, edx
		p20:
				test eax, 36
				jz p30
				add accumPml, edx
		p30:
				mov edi, nxtpf
				mov esi, nxtnf
				movzx edx, BYTE PTR[edi+ebx]
				movzx edi, BYTE PTR[esi+ebx]
				add edx, edi
				mov esi, edx
				add edx, edx
				sub esi, ecx
				add edx, esi
				jge b20
				neg edx
		b20:
				cmp edx, 23
				jle p60
				test eax, 9
				jz p40
				add accumNc, edx
		p40:
				cmp edx, 42
				jle p60
				test eax, 18
				jz p50
				add accumNm, edx
		p50:
				test eax, 36
				jz p60
				add accumNml, edx
		p60:
				test eax, 56
				jz p120
				mov ecx, prvpf
				mov edi, prvppf
				movzx edx, BYTE PTR [ecx+ebx]
				movzx esi, BYTE PTR [edi+ebx]
				shl edx, 2
				mov ecx, prvnf
				add edx, esi
				mov edi, curpf
				movzx esi, BYTE PTR [ecx+ebx]
				movzx ecx, BYTE PTR [edi+ebx]
				add edx, esi
				mov edi, curf
				movzx esi, BYTE PTR [edi+ebx]
				add ecx, esi
				mov edi, ecx
				add ecx, ecx
				add ecx, edi
				sub edx, ecx
				jge b40
				neg edx
		b40:
				cmp edx, 23
				jle p90
				test eax, 8
				jz p70
				add accumPc, edx
		p70:
				cmp edx, 42
				jle p90
				test eax, 16
				jz p80
				add accumPm, edx
		p80:
				test eax, 32
				jz p90
				add accumPml, edx
		p90:
				mov edi, nxtpf
				mov esi, nxtppf
				movzx edx, BYTE PTR [edi+ebx]
				movzx edi, BYTE PTR [esi+ebx]
				shl edx, 2
				mov esi, nxtnf
				add edx, edi
				movzx edi, BYTE PTR [esi+ebx]
				add edx, edi
				sub edx, ecx
				jge b50
				neg edx
		b50:
				cmp edx, 23
				jle p120
				test eax, 8
				jz p100
				add accumNc, edx
		p100:
				cmp edx, 42
				jle p120
				test eax, 16
				jz p110
				add accumNm, edx
		p110:
				test eax, 32
				jz p120
				add accumNml, edx
		p120:
				mov esi, incl
				mov ecx, stopx
				mov edi, mapp
				add ebx, esi
				mov edx, mapn
				cmp ebx, ecx
				jl xloop0
		end_yloop0:
				mov esi, Height
				mov eax, prvf_pitch
				mov ebx, curf_pitch
				mov ecx, nxtf_pitch
				mov edi, map_pitch
				sub esi, 2
				add y, 2
				add mapp, edi
				add prvpf, eax
				add curpf, ebx
				add prvnf, eax
				add curf, ebx
				add nxtpf, ecx
				add prvppf, eax
				add curnf, ebx
				add nxtnf, ecx
				add mapn, edi
				add nxtppf, ecx
				cmp y, esi
				jl yloop0
			}
		}
		else
		{
			__asm
			{
				mov y, 2
		yloop1:
				mov ecx, y0a
				mov edx, y1a
				cmp ecx, edx
				je xloop_pre1
				mov eax, y
				cmp eax, ecx
				jl xloop_pre1
				cmp eax, edx
				jle end_yloop1
		xloop_pre1:
				mov esi, incl
				mov ebx, startx
				mov edi, mapp
				mov edx, mapn
				mov ecx, stopx
		xloop1:
				movzx eax, BYTE PTR [edi+ebx]
				shl eax, 3
				add al, BYTE PTR [edx+ebx]
				jnz b11
				add ebx, esi
				cmp ebx, ecx
				jl xloop1
				jmp end_yloop1
		b11:
				mov edx, curf
				mov edi, curpf
				movzx ecx, BYTE PTR[edx+ebx]
				movzx esi, BYTE PTR[edi+ebx]
				shl ecx, 2
				mov edx, curnf
				add ecx, esi
				mov edi, prvpf
				movzx esi, BYTE PTR[edx+ebx]
				movzx edx, BYTE PTR[edi+ebx]
				add ecx, esi	
				mov edi, prvnf
				movzx esi, BYTE PTR[edi+ebx]
				add edx, esi
				mov edi, edx
				add edx, edx
				sub edi, ecx
				add edx, edi
				jge b31
				neg edx
		b31:
				cmp edx, 23
				jle p31
				test eax, 9
				jz p11
				add accumPc, edx
		p11:
				cmp edx, 42
				jle p31
				test eax, 18
				jz p21
				add accumPm, edx
		p21:
				test eax, 36
				jz p31
				add accumPml, edx
		p31:
				mov edi, nxtpf
				mov esi, nxtnf
				movzx edx, BYTE PTR[edi+ebx]
				movzx edi, BYTE PTR[esi+ebx]
				add edx, edi
				mov esi, edx
				add edx, edx
				sub esi, ecx
				add edx, esi
				jge b21
				neg edx
		b21:
				cmp edx, 23
				jle p61
				test eax, 9
				jz p41
				add accumNc, edx
		p41:
				cmp edx, 42
				jle p61
				test eax, 18
				jz p51
				add accumNm, edx
		p51:
				test eax, 36
				jz p61
				add accumNml, edx
		p61:
				test eax, 7
				jz p121
				mov ecx, prvnf
				mov edi, prvpf
				movzx edx, BYTE PTR [ecx+ebx]
				movzx esi, BYTE PTR [edi+ebx]
				shl edx, 2
				mov ecx, prvnnf
				add edx, esi
				mov edi, curf
				movzx esi, BYTE PTR [ecx+ebx]
				movzx ecx, BYTE PTR [edi+ebx]
				add edx, esi
				mov edi, curnf
				movzx esi, BYTE PTR [edi+ebx]
				add ecx, esi
				mov edi, ecx
				add ecx, ecx
				add ecx, edi
				sub edx, ecx
				jge b41
				neg edx
		b41:
				cmp edx, 23
				jle p91
				test eax, 1
				jz p71
				add accumPc, edx
		p71:
				cmp edx, 42
				jle p91
				test eax, 2
				jz p81
				add accumPm, edx
		p81:
				test eax, 4
				jz p91
				add accumPml, edx
		p91:
				mov edi, nxtnf
				mov esi, nxtpf
				movzx edx, BYTE PTR [edi+ebx]
				movzx edi, BYTE PTR [esi+ebx]
				shl edx, 2
				mov esi, nxtnnf
				add edx, edi
				movzx edi, BYTE PTR [esi+ebx]
				add edx, edi
				sub edx, ecx
				jge b51
				neg edx
		b51:
				cmp edx, 23
				jle p121
				test eax, 1
				jz p101
				add accumNc, edx
		p101:
				cmp edx, 42
				jle p121
				test eax, 2
				jz p111
				add accumNm, edx
		p111:
				test eax, 4
				jz p121
				add accumNml, edx
		p121:
				mov esi, incl
				mov ecx, stopx
				mov edi, mapp
				add ebx, esi
				mov edx, mapn
				cmp ebx, ecx
				jl xloop1
		end_yloop1:
				mov esi, Height
				mov eax, prvf_pitch
				mov ebx, curf_pitch
				mov ecx, nxtf_pitch
				mov edi, map_pitch
				sub esi, 2
				add y, 2
				add mapp, edi
				add prvpf, eax
				add curpf, ebx
				add prvnf, eax
				add curf, ebx
				add prvnnf, eax
				add nxtpf, ecx
				add curnf, ebx
				add nxtnf, ecx
				add mapn, edi
				add nxtnnf, ecx
				cmp y, esi
				jl yloop1
			}
		}
	}
	if (accumPm < 500 && accumNm < 500 && (accumPml >= 500 || accumNml >= 500) &&
		max(accumPml,accumNml) > 3*min(accumPml,accumNml)) 
	{
		accumPm = accumPml;
		accumNm = accumNml;
	}
	norm1 = (int)((accumPc / 6.0) + 0.5);
	norm2 = (int)((accumNc / 6.0) + 0.5);
	mtn1 = (int)((accumPm / 6.0) + 0.5);
	mtn2 = (int)((accumNm / 6.0) + 0.5);
	float c1 = float(max(norm1,norm2))/float(max(min(norm1,norm2),1));
	float c2 = float(max(mtn1,mtn2))/float(max(min(mtn1,mtn2),1));
	float mr = float(max(mtn1,mtn2))/float(max(max(norm1,norm2),1));
	if (((mtn1 >= 250  || mtn2 >= 250)  && (mtn1*4 < mtn2*1 || mtn2*4 < mtn1*1)) ||
		((mtn1 >= 375  || mtn2 >= 375)  && (mtn1*3 < mtn2*1 || mtn2*3 < mtn1*1)) ||
		((mtn1 >= 500  || mtn2 >= 500)  && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1)) ||
		((mtn1 >= 1000 || mtn2 >= 1000) && (mtn1*3 < mtn2*2 || mtn2*3 < mtn1*2)) ||
		((mtn1 >= 2000 || mtn2 >= 2000) && (mtn1*5 < mtn2*4 || mtn2*5 < mtn1*4)) ||
		((mtn1 >= 4000 || mtn2 >= 4000) && c2 > c1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else if (mr > 0.005 && max(mtn1,mtn2) > 150 && (mtn1*2 < mtn2*1 || mtn2*2 < mtn1*1))
	{
		if (mtn1 > mtn2) ret = match2;
		else ret = match1;
	}
	else
	{
		if (norm1 > norm2) ret = match2;
		else ret = match1;
	}
	if (debug)
	{
		sprintf(buf,"TFM:  frame %d  - comparing %c to %c  (SLOW 2)\n", n, MTC(match1), MTC(match2));
		OutputDebugString(buf);
		sprintf(buf,"TFM:  frame %d  - nmatches:  %d vs %d (%3.1f)  mmatches:  %d vs %d (%3.1f)\n", n, 
			norm1, norm2, c1, mtn1, mtn2, c2);
		OutputDebugString(buf);
	}
	return ret;
}

bool TFM::checkSceneChange(PVideoFrame &prv, PVideoFrame &src, PVideoFrame &nxt, 
			IScriptEnvironment *env, int n)
{
	if (sclast.frame == n+1) return sclast.sc;
	unsigned long diffp = 0, diffn = 0;
	const unsigned char *prvp = prv->GetReadPtr(PLANAR_Y);
	const unsigned char *srcp = src->GetReadPtr(PLANAR_Y);
	const unsigned char *nxtp = nxt->GetReadPtr(PLANAR_Y);
	int height = src->GetHeight(PLANAR_Y)>>1;
	int width = src->GetRowSize(PLANAR_Y);
	if (vi.IsYV12()) width = ((width>>4)<<4);
	else width = ((width>>5)<<5);
	int prv_pitch = prv->GetPitch(PLANAR_Y)<<1;
	int src_pitch = src->GetPitch(PLANAR_Y)<<1;
	int nxt_pitch = nxt->GetPitch(PLANAR_Y)<<1;
	prvp += (1-field)*(prv_pitch>>1);
	srcp += (1-field)*(src_pitch>>1);
	nxtp += (1-field)*(nxt_pitch>>1);
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(prvp)|int(srcp)|int(nxtp)|prv_pitch|src_pitch|nxt_pitch)&15))
	{
		if (sclast.frame == n)
		{
			diffp = sclast.diff;
			if (vi.IsYV12()) checkSceneChangeYV12_1_SSE2(srcp, nxtp, height, width, src_pitch, 
				nxt_pitch, diffn);
			else checkSceneChangeYUY2_1_SSE2(srcp, nxtp, height, width, prv_pitch, src_pitch, diffn);
		}
		else
		{
			if (vi.IsYV12()) checkSceneChangeYV12_2_SSE2(prvp, srcp, nxtp, height, width,
				prv_pitch, src_pitch, nxt_pitch, diffp, diffn);
			else checkSceneChangeYUY2_2_SSE2(prvp, srcp, nxtp, height, width, prv_pitch,
				src_pitch, nxt_pitch, diffp, diffn);
		}
	}
	else if (cpu&CPUF_INTEGER_SSE)
	{
		if (sclast.frame == n)
		{
			diffp = sclast.diff;
			if (vi.IsYV12()) checkSceneChangeYV12_1_ISSE(srcp, nxtp, height, width, src_pitch, 
				nxt_pitch, diffn);
			else checkSceneChangeYUY2_1_ISSE(srcp, nxtp, height, width, prv_pitch, src_pitch, diffn);
		}
		else
		{
			if (vi.IsYV12()) checkSceneChangeYV12_2_ISSE(prvp, srcp, nxtp, height, width,
				prv_pitch, src_pitch, nxt_pitch, diffp, diffn);
			else checkSceneChangeYUY2_2_ISSE(prvp, srcp, nxtp, height, width, prv_pitch,
				src_pitch, nxt_pitch, diffp, diffn);
		}
	}
	else if (cpu&CPUF_MMX)
	{
		if (sclast.frame == n)
		{
			diffp = sclast.diff;
			if (vi.IsYV12()) checkSceneChangeYV12_1_MMX(srcp, nxtp, height, width, src_pitch, 
				nxt_pitch, diffn);
			else checkSceneChangeYUY2_1_MMX(srcp, nxtp, height, width, prv_pitch, src_pitch, diffn);
		}
		else
		{
			if (vi.IsYV12()) checkSceneChangeYV12_2_MMX(prvp, srcp, nxtp, height, width,
				prv_pitch, src_pitch, nxt_pitch, diffp, diffn);
			else checkSceneChangeYUY2_2_MMX(prvp, srcp, nxtp, height, width, prv_pitch,
				src_pitch, nxt_pitch, diffp, diffn);
		}
	}
	else
	{
		if (sclast.frame != n)
		{
			if (vi.IsYV12())
			{
				for (int y=0; y<height; ++y)
				{
					for (int x=0; x<width; x+=4)
					{
						diffp += abs(srcp[x+0]-prvp[x+0]);
						diffp += abs(srcp[x+1]-prvp[x+1]);
						diffp += abs(srcp[x+2]-prvp[x+2]);
						diffp += abs(srcp[x+3]-prvp[x+3]);
						diffn += abs(srcp[x+0]-nxtp[x+0]);
						diffn += abs(srcp[x+1]-nxtp[x+1]);
						diffn += abs(srcp[x+2]-nxtp[x+2]);
						diffn += abs(srcp[x+3]-nxtp[x+3]);
					}
					prvp += prv_pitch;
					srcp += src_pitch;
					nxtp += nxt_pitch;
				}
			}
			else
			{
				for (int y=0; y<height; ++y)
				{
					for (int x=0; x<width; x+=8)
					{
						diffp += abs(srcp[x+0]-prvp[x+0]);
						diffp += abs(srcp[x+2]-prvp[x+2]);
						diffp += abs(srcp[x+4]-prvp[x+4]);
						diffp += abs(srcp[x+6]-prvp[x+6]);
						diffn += abs(srcp[x+0]-nxtp[x+0]);
						diffn += abs(srcp[x+2]-nxtp[x+2]);
						diffn += abs(srcp[x+4]-nxtp[x+4]);
						diffn += abs(srcp[x+6]-nxtp[x+6]);
					}
					prvp += prv_pitch;
					srcp += src_pitch;
					nxtp += nxt_pitch;
				}
			}
		}
		else
		{
			diffp = sclast.diff;
			if (vi.IsYV12())
			{
				for (int y=0; y<height; ++y)
				{
					for (int x=0; x<width; x+=4)
					{
						diffn += abs(srcp[x+0]-nxtp[x+0]);
						diffn += abs(srcp[x+1]-nxtp[x+1]);
						diffn += abs(srcp[x+2]-nxtp[x+2]);
						diffn += abs(srcp[x+3]-nxtp[x+3]);
					}
					srcp += src_pitch;
					nxtp += nxt_pitch;
				}
			}
			else
			{
				for (int y=0; y<height; ++y)
				{
					for (int x=0; x<width; x+=8)
					{
						diffn += abs(srcp[x+0]-nxtp[x+0]);
						diffn += abs(srcp[x+2]-nxtp[x+2]);
						diffn += abs(srcp[x+4]-nxtp[x+4]);
						diffn += abs(srcp[x+6]-nxtp[x+6]);
					}
					srcp += src_pitch;
					nxtp += nxt_pitch;
				}
			}
		}
	}
	if (debug)
	{
		sprintf(buf,"TFM:  frame %d  - diffp = %u   diffn = %u  diffmaxsc = %u  %c\n", n, diffp, diffn, diffmaxsc,
			(diffp>diffmaxsc || diffn>diffmaxsc) ? 'T' : 'F');
		OutputDebugString(buf);
	}
	sclast.frame = n+1;
	sclast.diff = diffn;
	sclast.sc = true;
	if (diffp > diffmaxsc || diffn > diffmaxsc) return true;
	sclast.sc = false;
	return false;
}

void TFM::createWeaveFrame(PVideoFrame &dst, PVideoFrame &prv, PVideoFrame &src, 
		PVideoFrame &nxt, IScriptEnvironment *env, int match, int &cfrm, int np)
{
	if (cfrm == match)
		return;
	int b, plane;
	for (b=0; b<np; ++b)
	{
		if (b == 0) plane = PLANAR_Y;
		else if (b == 1) plane = PLANAR_V;
		else plane = PLANAR_U;
		if (match == 0)
		{
			env->BitBlt(dst->GetWritePtr(plane)+(1-field)*dst->GetPitch(plane),dst->GetPitch(plane)<<1,
				src->GetReadPtr(plane)+(1-field)*src->GetPitch(plane),src->GetPitch(plane)<<1, 
				src->GetRowSize(plane),src->GetHeight(plane)>>1);
			env->BitBlt(dst->GetWritePtr(plane)+field*dst->GetPitch(plane),dst->GetPitch(plane)<<1, 
				prv->GetReadPtr(plane)+field*prv->GetPitch(plane),prv->GetPitch(plane)<<1, 
				prv->GetRowSize(plane),prv->GetHeight(plane)>>1);
		}
		else if (match == 1)
		{
			env->BitBlt(dst->GetWritePtr(plane),dst->GetPitch(plane),src->GetReadPtr(plane), 
				src->GetPitch(plane),src->GetRowSize(plane),src->GetHeight(plane));
		}
		else if (match == 2)
		{
			env->BitBlt(dst->GetWritePtr(plane)+(1-field)*dst->GetPitch(plane),dst->GetPitch(plane)<<1,
				src->GetReadPtr(plane)+(1-field)*src->GetPitch(plane),src->GetPitch(plane)<<1, 
				src->GetRowSize(plane),src->GetHeight(plane)>>1);
			env->BitBlt(dst->GetWritePtr(plane)+field*dst->GetPitch(plane),dst->GetPitch(plane)<<1, 
				nxt->GetReadPtr(plane)+field*nxt->GetPitch(plane),nxt->GetPitch(plane)<<1,
				nxt->GetRowSize(plane),nxt->GetHeight(plane)>>1);
		}
		else if (match == 3)
		{
			env->BitBlt(dst->GetWritePtr(plane)+field*dst->GetPitch(plane),dst->GetPitch(plane)<<1,
				src->GetReadPtr(plane)+field*src->GetPitch(plane),src->GetPitch(plane)<<1, 
				src->GetRowSize(plane),src->GetHeight(plane)>>1);
			env->BitBlt(dst->GetWritePtr(plane)+(1-field)*dst->GetPitch(plane),dst->GetPitch(plane)<<1, 
				prv->GetReadPtr(plane)+(1-field)*prv->GetPitch(plane),prv->GetPitch(plane)<<1, 
				prv->GetRowSize(plane),prv->GetHeight(plane)>>1);
		}
		else if (match == 4)
		{
			env->BitBlt(dst->GetWritePtr(plane)+field*dst->GetPitch(plane),dst->GetPitch(plane)<<1,
				src->GetReadPtr(plane)+field*src->GetPitch(plane),src->GetPitch(plane)<<1, 
				src->GetRowSize(plane),src->GetHeight(plane)>>1);
			env->BitBlt(dst->GetWritePtr(plane)+(1-field)*dst->GetPitch(plane),dst->GetPitch(plane)<<1, 
				nxt->GetReadPtr(plane)+(1-field)*nxt->GetPitch(plane),nxt->GetPitch(plane)<<1, 
				nxt->GetRowSize(plane),nxt->GetHeight(plane)>>1);
		}
		else env->ThrowError("TFM:  an unknown error occurred (no such match!)");
	}
	cfrm = match;
}

void TFM::putHint(PVideoFrame &dst, int match, int combed, bool d2vfilm)
{
	unsigned char *p = dst->GetWritePtr(PLANAR_Y);
	unsigned char *srcp = p;
	unsigned int i, hint = 0;
	unsigned int hint2 = 0, magic_number = 0;
	if (match == 0) hint |= ISP;
	else if (match == 1 && combed < 2) hint |= ISC;
	else if (match == 2) hint |= ISN;
	else if (match == 3) hint |= ISB;
	else if (match == 4) hint |= ISU;
	else if (match == 1 && combed > 1 && field == 0) hint |= ISDB;
	else if (match == 1 && combed > 1 && field == 1) hint |= ISDT;
	if (field == 1) hint |= TOP_FIELD;
	if (combed > 1) hint |= COMBED;
	if (d2vfilm) hint |= D2VFILM;
	for (i=0; i<32; ++i)
	{
		magic_number |= ((*srcp++ & 1) << i);
	}
	if (magic_number == MAGIC_NUMBER_2)
	{
		for (i=0; i<32; ++i)
		{
			hint2 |= ((*srcp++ & 1) << i);
		}
		hint2 <<= 8;
		hint2 &= 0xFF00;
		hint |= hint2|0x80;
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((MAGIC_NUMBER & (1 << i)) >> i);
	}
	for (i=0; i<32; ++i)
	{
		*p &= ~1;
		*p++ |= ((hint & (1 << i)) >> i);
	}
}

void TFM::buildDiffMapPlane2(const unsigned char *prvp, const unsigned char *nxtp, 
		unsigned char *dstp, int prv_pitch, int nxt_pitch, int dst_pitch, int Height, 
		int Width, IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|int(dstp)|prv_pitch|nxt_pitch|dst_pitch)&15))
	{
		buildABSDiffMask2_SSE2(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch,
			Width, Height);
	}
	else if (cpu&CPUF_MMX)
	{
		buildABSDiffMask2_MMX(prvp, nxtp, dstp, prv_pitch, nxt_pitch, dst_pitch,
			Width, Height);
	}
	else
	{
		const int inc = (!(vi.IsYUY2() && !mChroma)) ? 1 : 2;
		for (int y=0; y<Height; ++y)
		{
			for (int x=0; x<Width; x+=inc)
			{
				const int diff = abs(prvp[x]-nxtp[x]);
				if (diff > 19) dstp[x] = 3;
				else if (diff > 3) dstp[x] = 1;
				else dstp[x] = 0;
			}
			prvp += prv_pitch;
			nxtp += nxt_pitch;
			dstp += dst_pitch;
		}
	}
}

void TFM::buildABSDiffMask(const unsigned char *prvp, const unsigned char *nxtp, 
			int prv_pitch, int nxt_pitch, int tpitch, int width, int height,
			IScriptEnvironment *env)
{
	long cpu = env->GetCPUFlags();
	if (!IsIntelP4()) cpu &= ~CPUF_SSE2;
	if (opt != 4)
	{
		if (opt == 0) cpu &= ~0x2C;
		else if (opt == 1) { cpu &= ~0x28; cpu |= 0x04; }
		else if (opt == 2) { cpu &= ~0x20; cpu |= 0x0C; }
		else if (opt == 3) cpu |= 0x2C;
	}
	if ((cpu&CPUF_SSE2) && !((int(prvp)|int(nxtp)|prv_pitch|nxt_pitch|tpitch)&15))
	{
		buildABSDiffMask_SSE2(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch,
			width, height);
	}
	else if (cpu&CPUF_MMX)
	{
		buildABSDiffMask_MMX(prvp, nxtp, tbuffer, prv_pitch, nxt_pitch, tpitch,
			width, height);
	}
	else
	{
		unsigned char *dstp = tbuffer;
		if (!(vi.IsYUY2() && !mChroma))
		{
			for (int y=0; y<height; ++y)
			{
				for (int x=0; x<width; x+=4)
				{
					dstp[x+0] = abs(prvp[x+0]-nxtp[x+0]);
					dstp[x+1] = abs(prvp[x+1]-nxtp[x+1]);
					dstp[x+2] = abs(prvp[x+2]-nxtp[x+2]);
					dstp[x+3] = abs(prvp[x+3]-nxtp[x+3]);
				}
				prvp += prv_pitch;
				nxtp += nxt_pitch;
				dstp += tpitch;
			}
		}
		else
		{
			for (int y=0; y<height; ++y)
			{
				for (int x=0; x<width; x+=4)
				{
					dstp[x+0] = abs(prvp[x+0]-nxtp[x+0]);
					dstp[x+2] = abs(prvp[x+2]-nxtp[x+2]);
				}
				prvp += prv_pitch;
				nxtp += nxt_pitch;
				dstp += tpitch;
			}
		}
	}
}

AVSValue __cdecl Create_TFM(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	AVSValue v = new TFM(args[0].AsClip(),args[1].AsInt(-1),args[2].AsInt(-1),args[3].AsInt(1),
		args[4].AsInt(6),args[5].AsString(""),args[6].AsString(""),args[7].AsString(""),args[8].AsString(""),
		args[9].AsBool(false),args[10].AsBool(false),args[11].AsInt(1),args[12].AsBool(true),
		args[13].AsInt(15),args[14].AsInt(9),args[15].AsInt(80),args[16].AsBool(false),args[17].AsInt(16),
		args[18].AsInt(16),args[19].AsInt(0),args[20].AsInt(0),args[23].AsString(""),args[24].AsInt(0),
		args[25].AsInt(4),args[26].AsFloat(12.0),args[27].AsInt(0),args[28].AsInt(1),args[29].AsString(""),
		args[30].AsBool(true),args[31].AsInt(0),args[32].AsBool(false),args[33].AsBool(true),
		args[34].AsBool(true),args[35].AsInt(4),env);
	if (!args[4].IsInt() || args[4].AsInt() >= 2)
	{
		if (!args[4].IsInt() || args[4].AsInt() > 4)
		{
			try { v = env->Invoke("InternalCache", v).AsClip(); } 
			catch (IScriptEnvironment::NotFound) {  }
		}
		v = new TFMPP(v.AsClip(),args[4].AsInt(6),args[21].AsInt(5),args[5].AsString(""),
				args[10].AsBool(false),(args[22].IsClip() ? args[22].AsClip() : NULL),
				args[30].AsBool(true),args[35].AsInt(4),env);
	}
	return v;
}

TFM::TFM(PClip _child, int _order, int _field, int _mode, int _PP, const char* _ovr, 
	const char* _input, const char* _output, const char * _outputC, bool _debug, bool _display, 
	int _slow, bool _mChroma, int _cNum, int _cthresh, int _MI, bool _chroma, int _blockx, 
	int _blocky, int _y0, int _y1, const char* _d2v, int _ovrDefault, int _flags, double _scthresh, 
	int _micout, int _micmatching, const char* _trimIn, bool _usehints, int _metric, bool _batch,
	bool _ubsco, bool _mmsco, int _opt, IScriptEnvironment* env) : GenericVideoFilter(_child), 
	order(_order), field(_field), mode(_mode), PP(_PP), ovr(_ovr), input(_input), output(_output), 
	outputC(_outputC), debug(_debug), display(_display), slow(_slow), mChroma(_mChroma), cNum(_cNum), 
	cthresh(_cthresh), MI(_MI), chroma(_chroma), blockx(_blockx), blocky(_blocky), y0(_y0), 
	y1(_y1), d2v(_d2v), ovrDefault(_ovrDefault), flags(_flags), scthresh(_scthresh), micout(_micout), 
	micmatching(_micmatching), trimIn(_trimIn), usehints(_usehints), metric(_metric), 
	batch(_batch), ubsco(_ubsco), mmsco(_mmsco), opt(_opt)
{
	cArray = setArray = moutArray = moutArrayE = NULL;
	ovrArray = outArray = NULL;
	d2vfilmarray = NULL;
	tbuffer = NULL;
	trimArray = NULL;
	map = cmask = NULL;
	int z, w, q, b, i, count, last, fieldt, firstLine, qt;
	int countOvrS, countOvrM;
	char linein[1024];
	char *linep, *linet;
	FILE *f = NULL;
	if (!vi.IsYV12() && !vi.IsYUY2())
		env->ThrowError("TFM:  YV12 and YUY2 data only!");
	if (vi.height&1 || vi.width&1)
		env->ThrowError("TFM:  height and width must be divisible by 2!");
	if (vi.height < 6 || vi.width < 64)
		env->ThrowError("TFM:  frame dimensions too small!");
	if (mode < 0 || mode > 7)
		env->ThrowError("TFM:  mode must be set to 0, 1, 2, 3, 4, 5, 6, or 7!");
	if (field < -1 || field > 1)
		env->ThrowError("TFM:  field must be set to -1, 0, or 1!");
	if (PP < 0 || PP > 7)
		env->ThrowError("TFM:  PP must be at least 0 and less than 8!");
	if (order < -1 || order > 1)
		env->ThrowError("TFM:  order must be set to -1, 0, or 1!");
	if (blockx != 4 && blockx != 8 && blockx != 16 && blockx != 32 && blockx != 64 && 
		blockx != 128 && blockx != 256 && blockx != 512 && blockx != 1024 && blockx != 2048)
		env->ThrowError("TFM:  illegal blockx size!");
	if (blocky != 4 && blocky != 8 && blocky != 16 && blocky != 32 && blocky != 64 && 
		blocky != 128 && blocky != 256 && blocky != 512 && blocky != 1024 && blocky != 2048)
		env->ThrowError("TFM:  illegal blocky size!");
	if (y0 != y1 && (y0 < 0 || y1 < 0 || y0 > y1 || y1 > vi.height || y0 > vi.height))
		env->ThrowError("TFM:  bad y0 and y1 exclusion band values!");
	if (ovrDefault < 0 || ovrDefault > 2)
		env->ThrowError("TFM:  ovrDefault must be set to 0, 1, or 2!");
	if (flags < 0 || flags > 5)
		env->ThrowError("TFM:  flags must be set to 0, 1, 2, 3, 4, or 5!");
	if (slow < 0 || slow > 2)
		env->ThrowError("TFM:  slow must be set to 0, 1, or 2!");
	if (micout < 0 || micout > 2)
		env->ThrowError("TFM:  micout must be set to 0, 1, or 2!");
	if (micmatching < 0 || micmatching > 4)
		env->ThrowError("TFM:  micmatching must be set to 0, 1, 2, 3, or 4!");
	if (opt < 0 || opt > 4)
		env->ThrowError("TFM:  opt must be set to 0, 1, 2, 3, or 4!");
	if (metric != 0 && metric != 1)
		env->ThrowError("TFM:  metric must be set to 0 or 1!");
	if (scthresh < 0.0 || scthresh > 100.0)
		env->ThrowError("TFM:  scthresh must be between 0.0 and 100.0 (inclusive)!");
	if (debug)
	{
		sprintf(buf, "TFM:  %s by tritical\n", VERSION);
		OutputDebugString(buf);
	}
	child->SetCacheHints(CACHE_RANGE, 3); // fixed to diameter (07/30/2005)
	lastMatch.frame = lastMatch.field = lastMatch.combed = lastMatch.match = -20;
	nfrms = vi.num_frames-1;
	f = NULL;
	modeS = mode;
	PPS = PP;
	MIS = MI;
	d2vpercent = -20.00f;
	vidCount = setArraySize = 0;
	xhalf = blockx >> 1;
	yhalf = blocky >> 1;
	xshift = blockx == 4 ? 2 : blockx == 8 ? 3 : blockx == 16 ? 4 : blockx == 32 ? 5 :
		blockx == 64 ? 6 : blockx == 128 ? 7 : blockx == 256 ? 8 : blockx == 512 ? 9 : 
		blockx == 1024 ? 10 : 11;
	yshift = blocky == 4 ? 2 : blocky == 8 ? 3 : blocky == 16 ? 4 : blocky == 32 ? 5 :
		blocky == 64 ? 6 : blocky == 128 ? 7 : blocky == 256 ? 8 : blocky == 512 ? 9 : 
		blocky == 1024 ? 10 : 11;
	diffmaxsc = int((double(((vi.width>>4)<<4)*vi.height*219)*scthresh*0.5)/100.0);
	sclast.frame = -20;
	sclast.sc = true;
	if (mode == 1 || mode == 2 || mode == 3 || mode == 5 || mode == 6 || mode == 7 || 
		PP > 0 || micout > 0 || micmatching > 0)
	{
		cArray = (int *)_aligned_malloc((((vi.width+xhalf)>>xshift)+1)*(((vi.height+yhalf)>>yshift)+1)*4*sizeof(int), 16);
		if (!cArray) env->ThrowError("TFM:  malloc failure (cArray)!");
		cmask = new PlanarFrame(vi, true);
	}
	map = new PlanarFrame(vi, true);
	if (vi.IsYUY2())
	{
		xhalf *= 2;
		++xshift;
	}
	if (*d2v) 
	{
		parseD2V(env);
		if (trimArray != NULL)
		{
			free(trimArray);
			trimArray = NULL;
		}
	}
	orderS = order;
	fieldS = fieldO = field;
	if (fieldO == -1) 
	{
		if (order == -1) fieldO = child->GetParity(0) ? 1 : 0;
		else fieldO = order;
	}
	tpitchy = tpitchuv = -20;
	if (vi.IsYV12())
	{
		tpitchy = (vi.width&15) ? vi.width+16-(vi.width&15) : vi.width;
		tpitchuv = ((vi.width>>1)&15) ? (vi.width>>1)+16-((vi.width>>1)&15) : (vi.width>>1);
	}
	else tpitchy = ((vi.width<<1)&15) ? (vi.width<<1)+16-((vi.width<<1)&15) : (vi.width<<1);
	tbuffer = (unsigned char*)_aligned_malloc((vi.height>>1)*tpitchy, 16);
	if (tbuffer == NULL)
		env->ThrowError("TFM:  malloc failure (tbuffer)!");
	mode7_field = field;
	if (*input)
	{
		bool d2vmarked, micmarked;
		if ((f = fopen(input, "r")) != NULL)
		{
			ovrArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
			if (ovrArray == NULL) 
			{
				fclose(f);
				f = NULL;
				env->ThrowError("TFM:  malloc failure (ovrArray)!");
			}
			memset(ovrArray,255,vi.num_frames);
			if (d2vfilmarray == NULL)
			{
				d2vfilmarray = (unsigned char *)malloc((vi.num_frames+1)*sizeof(unsigned char));
				if (d2vfilmarray == NULL) env->ThrowError("TFM:  malloc failure (d2vfilmarray)!");
				memset(d2vfilmarray,0,(vi.num_frames+1)*sizeof(unsigned char));
			}
			fieldt = fieldO;
			firstLine = 0;
			if (debug)
			{
				sprintf(buf,"TFM:  successfully opened input file.  Field defaulting to - %s.\n", 
					fieldt == 0 ? "bottom" : "top");
				OutputDebugString(buf);
			}
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
						bool changed = false;
						if (strnicmp(linein, "field = top", 11) == 0) { fieldt = 1; changed = true; }
						else if (strnicmp(linein, "field = bottom", 14) == 0) { fieldt = 0; changed = true; }
						if (debug && changed)
						{
							sprintf(buf,"TFM:  detected field for input file - %s.\n", 
								fieldt == 0 ? "bottom" : "top");
							OutputDebugString(buf);
						}
					}
				}
				else if (*linep == 'c')
				{
					if (strnicmp(linein, "crc32 = ", 8) == 0)
					{
						linet = linein;
						while(*linet != ' ') *linet++;
						*linet++;
						while(*linet != ' ') *linet++;
						*linet++;
						unsigned int m, tempCrc;
						sscanf(linet, "%x", &m);
						calcCRC(child, 15, tempCrc, env);
						if (tempCrc != m && !batch)
						{
							fclose(f);
							f = NULL;
							env->ThrowError("TFM:  crc32 in input file does not match that of the current clip (%#x vs %#x)!",
								m, tempCrc);
						}
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
							env->ThrowError("TFM:  input file error (out of range or non-ascending frame #)!");
						}
						linep = linein;
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep != 0)
						{
							qt = -1;
							d2vmarked = micmarked = false;
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
								env->ThrowError("TFM:  input file error (invalid match specifier)!");
							}
							*linep++;
							*linep++;
							if (*linep != 0)
							{
								qt = *linep;
								if (qt == 45) qt = 0;
								else if (qt == 43) qt = COMBED;
								else if (qt == '1') { d2vmarked = true; qt = -1; }
								else if (qt == '[') { micmarked = true; qt = -1; }
								else
								{
									fclose(f);
									f = NULL;
									env->ThrowError("TFM:  input file error (invalid specifier)!");
								}
							}
							if (fieldt != fieldO)
							{
								if (q == 0) q = 3;
								else if (q == 2) q = 4;
								else if (q == 3) q = 0;
								else if (q == 4) q = 2;
							}
							if (!d2vmarked && !micmarked && qt != -1)
							{
								*linep++;
								*linep++;
								if (*linep == '1') d2vmarked = true;
								else if (*linep == '[') micmarked = true;
							}
							if (d2vmarked) 
							{
								d2vfilmarray[z] &= ~0x03;
								d2vfilmarray[z] |= fieldt == 1 ? 0x3 : 0x1;
								if (!micmarked)
								{
									*linep++;
									*linep++;
									if (*linep == '[') micmarked = true;
								}
							}
							if (micmarked)
							{
								// add mic input handling in the future
							}
							ovrArray[z] |= 0x07;
							ovrArray[z] &= (q|0xF8);
							if (qt != -1)
							{
								ovrArray[z] &= 0xDF;
								ovrArray[z] |= 0x10;
								ovrArray[z] &= (qt|0xEF);
							}
						}
					}
				}
			}
			fclose(f);
			f = NULL;
		}
		else env->ThrowError("TFM:  input file error (could not open file)!");
	}
	if (*ovr)
	{
		if ((f = fopen(ovr, "r")) != NULL)
		{
			countOvrS = countOvrM = 0;
			while(fgets(linein, 1024, f) != 0)
			{
				if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' || linein[0] == ';' || linein[0] == '#') 
					continue;
				linep = linein;
				while (*linep != 'c' && *linep != 'p' && *linep != 'n' && *linep != 'b' && 
					*linep != 'u' && *linep != 'l' && *linep != 'h' && *linep != '+' && *linep != '-' && *linep != 0) *linep++;
				if (*linep == 0) ++countOvrS;
				else ++countOvrM;
			}
			fclose(f);
			f = NULL;
			if (ovrDefault != 0 && ovrArray != NULL)
			{
				if (ovrDefault == 1) q = 0;
				else if (ovrDefault == 2) q = COMBED;
				for (int h=0; h<vi.num_frames; ++h)
				{
					ovrArray[h] &= 0xDF;
					ovrArray[h] |= 0x10;
					ovrArray[h] &= (q|0xEF);
					if (q == 0 && ((ovrArray[h]&7) == 6 ||
						(ovrArray[h]&7) == 5))
					{
						ovrArray[h] |= 0x07;
						ovrArray[h] &= (1|0xF8);
					}
				}
			}
			if (countOvrS == 0 && countOvrM == 0) { goto emptyovr; }
			if (countOvrS > 0)
			{
				++countOvrS;
				countOvrS *= 4;
				setArray = (int *)malloc(countOvrS*sizeof(int));
				if (setArray == NULL) env->ThrowError("TFM:  malloc failure (setArray)!");
				memset(setArray,255,countOvrS*sizeof(int));
				setArraySize = countOvrS;
			}
			if (countOvrM > 0 && ovrArray == NULL)
			{
				ovrArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
				if (ovrArray == NULL) env->ThrowError("TFM:  malloc failure (ovrArray)!");
				memset(ovrArray,255,vi.num_frames);
				if (ovrDefault != 0)
				{
					if (ovrDefault == 1) q = 0;
					else if (ovrDefault == 2) q = COMBED;
					for (int h=0; h<vi.num_frames; ++h)
					{
						ovrArray[h] &= 0xDF;
						ovrArray[h] |= 0x10;
						ovrArray[h] &= (q|0xEF);
					}
				}
			}
			last = -1;
			fieldt = fieldO;
			firstLine = 0;
			i = 0;
			if ((f = fopen(ovr, "r")) != NULL)
			{
				if (debug)
				{
					sprintf(buf,"TFM:  successfully opened ovr file.  Field defaulting to - %s.\n", 
						fieldt == 0 ? "bottom" : "top");
					OutputDebugString(buf);
				}
				while (fgets(linein, 1024, f) != NULL)
				{
					if (linein[0] == 0 || linein[0] == '\n' || linein[0] == '\r' ||  linein[0] == ';' || linein[0] == '#') 
						continue;
					++firstLine;
					linep = linein;
					while (*linep != 'f' && *linep != 'F' && *linep != 0 && *linep != ' ' && *linep != ',') *linep++;
					if (*linep == 'f' || *linep == 'F')
					{
						if (firstLine == 1)
						{
							bool changed = false;
							if (strnicmp(linein, "field = top", 11) == 0) { fieldt = 1; changed = true; }
							else if (strnicmp(linein, "field = bottom", 14) == 0) { fieldt = 0; changed = true; }
							if (debug && changed)
							{
								sprintf(buf,"TFM:  detected field for ovr file - %s.\n", 
									fieldt == 0 ? "bottom" : "top");
								OutputDebugString(buf);
							}
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
						*linep++;
						if (*linep == 'p' || *linep == 'c' || *linep == 'n' || *linep == 'b' || *linep == 'u' || *linep == 'l' || *linep == 'h')
						{
							sscanf(linein, "%d", &z);
							if (z<0 || z>nfrms || z <= last) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFM:  ovr file error (out of range or non-ascending frame #)!");
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
									env->ThrowError("TFM:  ovr file error (invalid match specifier)!");
								}
								if (fieldt != fieldO)
								{
									if (q == 0) q = 3;
									else if (q == 2) q = 4;
									else if (q == 3) q = 0;
									else if (q == 4) q = 2;
								}
								ovrArray[z] |= 0x07;
								ovrArray[z] &= (q|0xF8);
								last = z;
							}
						}
						else if (*linep == '-' || *linep == '+')
						{
							sscanf(linein, "%d", &z);
							if (z<0 || z>nfrms) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFM:  ovr file error (out of range or non-ascending frame #)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								q = *linep;
								if (q == 45) q = 0;
								else if (q == 43) q = COMBED;
								else 
								{
									fclose(f);
									f = NULL;
									env->ThrowError("TFM:  ovr file error (invalid symbol)!");
								}
								ovrArray[z] &= 0xDF;
								ovrArray[z] |= 0x10;
								ovrArray[z] &= (q|0xEF);
								if (q == 0 && ((ovrArray[z]&7) == 6 ||
									(ovrArray[z]&7) == 5))
								{
									ovrArray[z] |= 0x07;
									ovrArray[z] &= (1|0xF8);
								}
							}
						}
						else
						{
							sscanf(linein, "%d", &z);
							if (z<0 || z>nfrms) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFM:  ovr input error (out of range frame #)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*linep == 'f' || *linep == 'm' || *linep == 'o' || *linep == 'P' || *linep == 'i')
								{
									q = *linep;
									*linep++; 
									*linep++;
									if (*linep == 0) continue;
									sscanf(linep, "%d", &b);
									if (q == 102 && b != 0 && b != 1 && b != -1) 
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad field value)!");
									}
									else if (q == 111 && b != 0 && b != 1 && b != -1) 
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad order value)!");
									}
									else if (q == 109 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad mode value)!");
									}
									else if (q == 80 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad PP value)!");
									}
									setArray[i] = q; ++i;
									setArray[i] = z; ++i;
									setArray[i] = z; ++i;
									setArray[i] = b; ++i;
								}
							}
						}
					}
					else if (*linep == ',')
					{
						while (*linep != ' ' && *linep != 0) *linep++;
						if (*linep == 0) continue;
						*linep++;
						if (*linep == 'p' || *linep == 'c' || *linep == 'n' || *linep == 'u' || *linep == 'b' || *linep == 'l' || *linep == 'h')
						{
							sscanf(linein, "%d,%d", &z, &w);
							if (w == 0) w = nfrms;
							if (z<0 || z>nfrms || w<0 || w>nfrms || w<z || z <= last) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFM:  input file error (out of range or non-ascending frame #)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*(linep+1) == 'p' || *(linep+1) == 'c' || *(linep+1) == 'n' || *(linep+1) == 'b' || *(linep+1) == 'u' || *(linep+1) == 'l' || *(linep+1) == 'h')
								{
									count = 0;
									while ((*linep == 'p' || *linep == 'c' || *linep == 'n' || *linep == 'b' || *linep == 'u' || *linep == 'l' || *linep == 'h') && (z+count<=w))
									{
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
											env->ThrowError("TFM:  input file error (invalid match specifier)!");
										}
										if (fieldt != fieldO)
										{
											if (q == 0) q = 3;
											else if (q == 2) q = 4;
											else if (q == 3) q = 0;
											else if (q == 4) q = 2;
										}
										ovrArray[z+count] |= 0x07;
										ovrArray[z+count] &= (q|0xF8);
										++count;
										*linep++;
									}
									while (z+count<=w)
									{
										ovrArray[z+count] |= 0x07;
										ovrArray[z+count] &= (ovrArray[z]|0xF8);
										++z;
									}
									last = w;
								}
								else
								{
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
										env->ThrowError("TFM:  input file error (invalid match specifier)!");
									}
									if (fieldt != fieldO)
									{
										if (q == 0) q = 3;
										else if (q == 2) q = 4;
										else if (q == 3) q = 0;
										else if (q == 4) q = 2;
									}
									while (z<=w)
									{
										ovrArray[z] |= 0x07;
										ovrArray[z] &= (q|0xF8);
										++z;
									}
									last = w;
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
								env->ThrowError("TFM:  input file error (out of range or non-ascending frame #)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*(linep+1) == '-' || *(linep+1) == '+')
								{
									count = 0;
									while ((*linep == '-' || *linep == '+') && (z+count<=w))
									{
										q = *linep;
										if (q == 45) q = 0;
										else if (q == 43) q = COMBED;
										else 
										{
											fclose(f);
											f = NULL;
											env->ThrowError("TFM:  input file error (invalid symbol)!");
										}
										ovrArray[z+count] &= 0xDF;
										ovrArray[z+count] |= 0x10;
										ovrArray[z+count] &= (q|0xEF);
										if (q == 0 && ((ovrArray[z+count]&7) == 6 ||
											(ovrArray[z+count]&7) == 5))
										{
											ovrArray[z+count] |= 0x07;
											ovrArray[z+count] &= (1|0xF8);
										}
										++count;
										*linep++;
									}
									while (z+count<=w)
									{
										ovrArray[z+count] &= 0xDF;
										ovrArray[z+count] |= 0x10;
										ovrArray[z+count] &= (ovrArray[z]|0xEF);
										if ((ovrArray[z]&0x10) == 0 && ((ovrArray[z+count]&7) == 6 ||
											(ovrArray[z+count]&7) == 5))
										{
											ovrArray[z+count] |= 0x07;
											ovrArray[z+count] &= (1|0xF8);
										}
										++z;
									}
								}
								else
								{
									q = *linep;
									if (q == 45) q = 0;
									else if (q == 43) q = COMBED;
									else 
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  input file error (invalid symbol)!");
									}
									while (z<=w)
									{
										ovrArray[z] &= 0xDF;
										ovrArray[z] |= 0x10;
										ovrArray[z] &= (q|0xEF);
										if (q == 0 && ((ovrArray[z]&7) == 6 ||
											(ovrArray[z]&7) == 5))
										{
											ovrArray[z] |= 0x07;
											ovrArray[z] &= (1|0xF8);
										}
										++z;
									}
								}
							}
						}
						else
						{
							sscanf(linein, "%d,%d", &z, &w);
							if (w == 0) w = nfrms;
							if (z<0 || z>nfrms || w<0 || w>nfrms || w < z) 
							{
								fclose(f);
								f = NULL;
								env->ThrowError("TFM: ovr input error (invalid frame range)!");
							}
							linep = linein;
							while (*linep != ' ' && *linep != 0) *linep++;
							if (*linep != 0)
							{
								*linep++;
								if (*linep == 'f' || *linep == 'm' || *linep == 'o' || *linep == 'P' || *linep == 'i')
								{
									q = *linep;
									*linep++; 
									*linep++;
									if (*linep == 0) continue;
									sscanf(linep, "%d", &b);
									if (q == 102 && b != 0 && b != 1 && b != -1) 
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad field value)!");
									}
									else if (q == 111 && b != 0 && b != 1 && b != -1) 
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad order value)!");
									}
									else if (q == 109 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad mode value)!");
									}
									else if (q == 80 && (b < 0 || b > 7))
									{
										fclose(f);
										f = NULL;
										env->ThrowError("TFM:  ovr input error (bad PP value)!");
									}
									setArray[i] = q; ++i;
									setArray[i] = z; ++i;
									setArray[i] = w; ++i;
									setArray[i] = b; ++i;
								}
							}
						}
					}
				}
				fclose(f);
				f = NULL;
			}
			else env->ThrowError("TFM:  ovr file error (could not open file)!");
		}
		else env->ThrowError("TFM:  ovr input error (could not open ovr file)!");
	}
emptyovr:
	if (*output)
	{
		if ((f = fopen(output, "w")) != NULL)
		{
			_fullpath(outputFull, output, MAX_PATH);
			calcCRC(child, 15, outputCrc, env);
			fclose(f);
			f = NULL;
			outArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
			if (outArray == NULL)
				env->ThrowError("TFM:  malloc failure (outArray, output)!");
			memset(outArray, 0, vi.num_frames);
			moutArray = (int *)malloc(vi.num_frames*sizeof(int));
			if (moutArray == NULL)
				env->ThrowError("TFM:  malloc failure (moutArray, output)!");
			for (int i=0; i<vi.num_frames; ++i) moutArray[i] = -1;
			if (micout > 0)
			{
				int sn = micout == 1 ? 3 : 5;
				moutArrayE = (int*)malloc(vi.num_frames*sn*sizeof(int));
				if (moutArrayE == NULL)
					env->ThrowError("TFM:  malloc failure (moutArrayE)!");
				for (int i=0; i<sn*vi.num_frames; ++i) moutArrayE[i] = -20;
			}
		}
		else env->ThrowError("TFM:  output file error (cannot create file)!");
	}
	if (*outputC)
	{
		if ((f = fopen(outputC, "w")) != NULL) 
		{
			_fullpath(outputCFull, outputC, MAX_PATH);
			fclose(f);
			f = NULL;
			if (outArray == NULL)
			{
				outArray = (unsigned char *)malloc(vi.num_frames*sizeof(unsigned char));
				if (outArray == NULL)
					env->ThrowError("TFM:  malloc failure (outArray, outputC)!");
				memset(outArray, 0, vi.num_frames);
			}
		}
		else env->ThrowError("TFM:  outputC file error (cannot create file)!");
	}
	AVSValue tfmPassValue(PP);
	const char *varname = "TFMPPValue";
	env->SetVar(varname,tfmPassValue);
	if (f != NULL) fclose(f);
}

TFM::~TFM()
{
	if (map) delete map;
	if (cmask) delete cmask;
	if (cArray != NULL) _aligned_free(cArray);
	if (tbuffer != NULL) _aligned_free(tbuffer);
	if (setArray != NULL) free(setArray);
	if (ovrArray != NULL) free(ovrArray);
	if (d2vfilmarray != NULL) free(d2vfilmarray);
	if (trimArray != NULL) free(trimArray);
	if (outArray != NULL)
	{
		FILE *f = NULL;
		if (*output)
		{
			if ((f = fopen(outputFull, "w")) != NULL)
			{
				char tempBuf[40], tb2[40];
				int match, sn = micout == 1 ? 3 : 5;
				if (moutArrayE)
				{
					for (int i=0; i<sn*vi.num_frames; ++i)
					{
						if (moutArrayE[i] == -20) moutArrayE[i] = -1;
					}
				}
				fprintf(f, "#TFM %s by tritical\n", VERSION);
				fprintf(f, "field = %s\n", fieldO == 1 ? "top" : "bottom");
				fprintf(f, "crc32 = %x\n", outputCrc);
				for (int h=0; h<=nfrms; ++h)
				{
					if (outArray[h]&FILE_ENTRY)
					{
						match = (outArray[h]&0x07);
						sprintf(tempBuf, "%d %c", h, MTC(match));
						if (outArray[h]&0x20)
						{
							if (outArray[h]&0x10) strcat(tempBuf, " +");
							else strcat(tempBuf, " -");
						}
						if (outArray[h]&FILE_D2V) strcat(tempBuf, " 1");
						if (moutArray && moutArray[h] != -1) 
						{
							sprintf(tb2," [%d]", moutArray[h]);
							strcat(tempBuf, tb2);
						}
						if (moutArrayE)
						{
							int th = h*sn;
							if (sn == 3) sprintf(tb2, " (%d %d %d)", moutArrayE[th+0], 
								moutArrayE[th+1], moutArrayE[th+2]);
							else sprintf(tb2, " (%d %d %d %d %d)", moutArrayE[th+0],
								moutArrayE[th+1], moutArrayE[th+2], moutArrayE[th+3], 
								moutArrayE[th+4]);
							strcat(tempBuf, tb2);
						}
						strcat(tempBuf, "\n");
						fprintf(f, tempBuf);
					}
				}
				generateOvrHelpOutput(f);
				fclose(f);
				f = NULL;
			}
		}
		if (*outputC)
		{
			if ((f = fopen(outputCFull, "w")) != NULL)
			{
				int count = 0, match;
				fprintf(f, "#TFM %s by tritical\n", VERSION);
				for (int h=0; h<=nfrms; ++h)
				{
					if (outArray[h]&FILE_ENTRY) match = (outArray[h]&0x07);
					else match = 0;
					if (match == 1 || match == 5 || match == 6) ++count;
					else
					{
						if (count > cNum) fprintf(f, "%d,%d\n", h-count, h-1);
						count = 0;
					}
				}
				if (count > cNum) fprintf(f, "%d,%d\n", nfrms-count+1, nfrms);
				fclose(f);
				f = NULL;
			}
		}
		if (f != NULL) fclose(f);
		free(outArray);
	}
	if (moutArray != NULL) free(moutArray);
	if (moutArrayE != NULL) free(moutArrayE);
}

void TFM::generateOvrHelpOutput(FILE *f)
{
	int ccount = 0, mcount = 0, acount = 0;
	int ordert = order == -1 ? child->GetParity(0) : order;
	int ao = fieldO^ordert ? 0 : 2;
	for (int i=0; i<vi.num_frames; ++i)
	{
		if (!(outArray[i]&FILE_ENTRY)) return;
		const int temp = outArray[i]&0x07;
		if (temp == 3 || temp == 4 || temp == ao) ++acount;
		if (moutArray[i] != -1) ++mcount;
		if ((outArray[i]&0x30) == 0x30) ++ccount;
	}
	fprintf(f,"#\n#\n# OVR HELP INFORMATION:\n#\n");
	fprintf(f,"# [COMBED FRAMES]\n#\n");
	fprintf(f,"#   [Individual Frames]\n");
	fprintf(f,"#   FORMAT:  frame_number (mic_value)\n#\n");
	if (PP == 0) fprintf(f, "#   none detected (PP=0)\n");
	else if (ccount)
	{
		for (int i=0; i<vi.num_frames; ++i)
		{
			if ((outArray[i]&0x30) == 0x30)
			{
				if (moutArray[i] < 0) fprintf(f, "#   %d\n", i);
				else fprintf(f, "#   %d (%d)\n", i, moutArray[i]);
			}
		}
	}
	else fprintf(f, "#   none detected\n");
	fprintf(f,"#\n#   [Grouped Ranges Allowing Small Breaks]\n");
	fprintf(f,"#   FORMAT:  frame_start, frame_end (percentage combed)\n#\n");
	if (PP == 0) fprintf(f, "#   none detected (PP=0)\n");
	else if (ccount)
	{
		int icount = 0, pcount = 0, rcount = 0, i = 0;
		for (; i<vi.num_frames; ++i)
		{
			if ((outArray[i]&0x30) == 0x30) 
			{
				++icount;
				++rcount;
				pcount = 0;
			}
			else 
			{	++pcount;
				if (rcount > 0) ++rcount;
				if (pcount > 12)
				{
					if (icount > 1)
						fprintf(f,"#   %d,%d (%3.1f%c)\n", i-rcount+1, i-pcount, 
							icount*100.0/double(rcount-pcount), '%');
					rcount = icount = 0;
				}
			}
		}
		if (icount > 1)
			fprintf(f,"#   %d,%d (%3.1f%c)\n", i-rcount, i-pcount, 
				icount*100.0/double(rcount-pcount), '%');
	}
	else fprintf(f, "#   none detected\n");
	fprintf(f,"#\n#\n# [POSSIBLE MISSED COMBED FRAMES]\n#\n");
	fprintf(f,"#   FORMAT:  frame_number (mic_value)\n#\n");
	if (PP == 0) fprintf(f,"#   none detected (PP=0)\n");
	else if (mcount)
	{
		int maxcp = int(MI*0.85), count = 0;
		int mt = max(int(MI*0.1875),5);
		for (int i=0; i<vi.num_frames; ++i)
		{
			if ((outArray[i]&0x30) == 0x30)
				continue;
			const int prev = i > 0 ? moutArray[i-1] : 0;
			const int curr = moutArray[i];
			const int next = i < vi.num_frames-1 ? moutArray[i+1] : 0;
			if (curr <= MI && ((curr >= mt && curr > next*2 && curr > prev*2 && 
				curr-next > mt && curr-prev > mt) || (curr > maxcp) || 
				(prev > MI && next > MI && curr > MI*0.5) || 
				((prev > MI || next > MI) && curr > MI*0.75)))
			{
				fprintf(f,"#   %d (%d)\n", i, moutArray[i]);
				++count;
			}
		}
		if (!count) fprintf(f,"#   none detected\n");
	}
	else fprintf(f,"#   none detected\n");
	fprintf(f,"#\n#\n# [u, b, AND AGAINST ORDER (%c) MATCHES]\n#\n", MTC(ao));
	fprintf(f,"#   FORMAT:  frame_number match  or  range_start,range_end match\n#\n");
	if (acount)
	{
		int lastf = -1, count = 0, i = 0;
		for (; i<vi.num_frames; ++i)
		{
			const int temp = outArray[i]&0x07;
			if (temp == 3 || temp == 4 || temp == ao) 
			{
				if (lastf == -1) lastf = temp;
				else if (temp != lastf)
				{
					if (count == 1) fprintf(f,"#   %d %c\n",i-1,MTC(lastf));
					else fprintf(f,"#   %d,%d %c\n",i-count,i-1,MTC(lastf));
					count = 0;
					lastf = temp;
				}
				++count;
			}
			else if (count)
			{
				if (count == 1) fprintf(f,"#   %d %c\n",i-1,MTC(lastf));
				else fprintf(f,"#   %d,%d %c\n",i-count,i-1,MTC(lastf));
				count = 0;
				lastf = -1;
			}
		}
		if (count == 1) fprintf(f,"#   %d %c\n",i-1,MTC(lastf));
		else if (count > 1) fprintf(f,"#   %d,%d %c\n",i-count,i-1,MTC(lastf));
	}
	else fprintf(f,"#   none detected\n");
}