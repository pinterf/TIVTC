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

void TDecimate::debugOutput1(int n, bool film, int blend)
{
	if (mode == 0 || (mode == 3 && vfrDec == 0))
	{
		sprintf(buf,"TDecimate:  %d: ", curr.frame);
		formatMetrics(curr);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", curr.frame);
		formatMatches(curr,prev);
		OutputDebugString(buf);
	}
	else
	{
		sprintf(buf,"TDecimate:  %d: ", prev.frame);
		formatMetrics(prev);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", prev.frame);
		formatDups(prev);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", prev.frame);
		formatMatches(prev,prev);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", curr.frame);
		formatMetrics(curr);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", curr.frame);
		formatDups(curr);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", curr.frame);
		formatMatches(curr,prev);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", next.frame);
		formatMetrics(next);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", next.frame);
		formatDups(next);
		OutputDebugString(buf);
		sprintf(buf,"TDecimate:  %d: ", next.frame);
		formatMatches(next,curr);
		OutputDebugString(buf);
	}
	if (film)
	{
		if (cycleR > 1 || blend == 3) sprintf(buf,"TDecimate:  %d:  Dropping Frames:", n);
		else sprintf(buf,"TDecimate:  %d:  Dropping Frame:", n);
		formatDecs(curr);
	}
	else sprintf(buf,"TDecimate:  %d:  VIDEO", n);
	OutputDebugString(buf);
}

void TDecimate::debugOutput2(int n, int ret, bool film, int f1, int f2, double amount1,
							 double amount2)
{
	if (amount1 == 0.0 && amount2 == 0.0) 
		sprintf(buf,"TDecimate:  inframe = %d  useframe = %d\n", n, ret);
	else sprintf(buf, "TDecimate:  inframe: %d  useframe = blend %d-%d (%3.2f,%3.2f)\n", 
		n, f1, f2, amount1*100.0, amount2*100.0);
	OutputDebugString(buf);
}

void TDecimate::formatMetrics(Cycle &current)
{
	char tempBuf[40];
	for (int i=current.cycleS; i<current.cycleE; ++i)
	{
		sprintf(tempBuf, " %3.2f", current.diffMetricsN[i]);
		strcat(buf, tempBuf);
	}
	strcat(buf, "\n");
}

void TDecimate::formatDups(Cycle &current)
{
	char tempBuf[40];
	for (int i=current.cycleS; i<current.cycleE; ++i)
	{
		sprintf(tempBuf, " %d", current.dupArray[i]);
		strcat(buf, tempBuf);
	}
	strcat(buf, "\n");
}

void TDecimate::formatDecs(Cycle &current)
{
	char tempBuf[40];
	int i = current.cycleS, b = current.frameSO;
	for (; i<current.cycleE; ++i, ++b)
	{
		if (current.decimate[i] == 1)
		{
			sprintf(tempBuf, " %d", b);
			strcat(buf, tempBuf);
		}
	}
}

void TDecimate::formatMatches(Cycle &current)
{
	char tempBuf[40];
	for (int i=current.cycleS; i<current.cycleE; ++i)
	{
		if (current.match[i] >= 0)
			sprintf(tempBuf, " %c  %d", MTC(current.match[i]), current.filmd2v[i]);
		else
			sprintf(tempBuf, " %c", MTC(current.match[i]));
		strcat(buf, tempBuf);
	}
	strcat(buf, "\n");
}

void TDecimate::formatMatches(Cycle &current, Cycle &previous)
{
	char tempBuf[40];
	int mp;
	if (previous.frame != current.frame) 
		mp = previous.cycleE > 0 ? previous.match[previous.cycleE-1] : -20;
	else mp = -20;
	int mc = current.match[current.cycleS];
	for (int i=current.cycleS; i<current.cycleE; ++i)
	{
		sprintf(tempBuf, " %c", MTC(mc));
		strcat(buf, tempBuf);
		if (mc >= 0)
		{
			if (checkMatchDup(mp, mc))
			{
				sprintf(tempBuf, " (%s)", "mdup");
				strcat(buf, tempBuf);
			}
			if (current.filmd2v[i] == 1)
			{
				sprintf(tempBuf, " (%s)", "d2vdup");
				strcat(buf, tempBuf);
			}
		}
		mp = mc;
		if (i < current.cycleE-1) mc = current.match[i+1];
	}
	strcat(buf, "\n");
}

void TDecimate::addMetricCycle(Cycle &j)
{
	if (metricsOutArray == NULL) return;
	int i = j.cycleS, p = j.frameSO;
	for (; i<j.cycleE; ++i, ++p)
	{
		metricsOutArray[p<<1] = j.diffMetricsU[i];
		metricsOutArray[(p<<1)+1] = j.diffMetricsUF[i];
	}
}

void TDecimate::displayOutput(IScriptEnvironment* env, PVideoFrame &dst, int n, 
		int ret, bool film, double amount1, double amount2, int f1, int f2, int np)
{
	int i=0;
	char tempBuf[40];
	sprintf(buf, "TDecimate %s by tritical", VERSION);
	Draw(dst, 0, i++, buf, np);
	sprintf(buf, "Mode: %d  Cycle: %d  CycleR: %d  Hybrid: %d", mode, cycle, cycleR, hybrid);
	Draw(dst, 0, i++, buf, np);
	if (amount1 == 0.0 && amount2 == 0.0) 
		sprintf(buf, "inframe: %d  useframe: %d", n, ret);
	else sprintf(buf, "inframe: %d  useframe: blend %d-%d (%3.2f,%3.2f)", n, f1, f2,
			amount1*100.0, amount2*100.0);
	Draw(dst, 0, i++, buf, np);
	int i_saved = i, retd, side = 0, strack = 0;
	if (mode == 0 || (mode == 3 && vfrDec == 0))
	{
		int mp = prev.frame != -20 ? prev.match[prev.cycleE-1] : -20;
		int mc = curr.match[curr.cycleS];
		for (int x=curr.cycleS; x<curr.cycleE; ++x)
		{
			sprintf(buf, "%d%s%3.2f", curr.frame+x, curr.decimate[x] == 1 ? ":**" : ":  ", 
				curr.diffMetricsN[x]);
			if (mc >= 0)
			{
				sprintf(tempBuf," %c", MTC(mc));
				strcat(buf, tempBuf);
				if (checkMatchDup(mp, mc))
				{
					sprintf(tempBuf," (%s)", "mdup");
					strcat(buf, tempBuf);
				}
				if (curr.filmd2v[x] == 1)
				{
					sprintf(tempBuf," (%s)", "d2vdup");
					strcat(buf, tempBuf);
				}
			}
			retd = Draw(dst, side, i++, buf, np);
			if (retd == -1) 
			{
				side += strack+2;
				strack = 0;
				i = i_saved;
				Draw(dst, side, i++, buf, np);
			}
			else strack = max(strack,retd);
			mp = mc;
			if (x < curr.cycleE-1) mc = curr.match[x+1];
		}
	}
	else
	{
		int mp = prev.frame != -20 ? prev.match[prev.cycleE-1] : -20;
		int mc = curr.match[curr.cycleS];
		for (int x=curr.cycleS; x<curr.cycleE; ++x)
		{
			sprintf(buf, "%d%s%3.2f", curr.frame+x, curr.decimate[x] == 1 ? ":**" : ":  ", 
				curr.diffMetricsN[x]);
			if (mc >= 0)
			{
				sprintf(tempBuf," %c", MTC(mc));
				strcat(buf, tempBuf);
			}
			sprintf(tempBuf," %s", curr.dupArray[x] == 1 ? "(dup)" : "(new)");
			strcat(buf, tempBuf);
			if (mc >= 0)
			{
				if (checkMatchDup(mp, mc))
				{
					sprintf(tempBuf," (%s)", "mdup");
					strcat(buf, tempBuf);
				}
				if (curr.filmd2v[x] == 1)
				{
					sprintf(tempBuf," (%s)", "d2vdup");
					strcat(buf, tempBuf);
				}
			}
			retd = Draw(dst, side, i++, buf, np);
			if (retd == -1) 
			{
				side += strack+2;
				strack = 0;
				i = i_saved;
				Draw(dst, side, i++, buf, np);
			}
			else strack = max(strack,retd);
			mp = mc;
			if (x < curr.cycleE-1) mc = curr.match[x+1];
		}
	}
	if (film) 
	{
		sprintf(buf, "FILM, Drop:");
		formatDecs(curr);
	}
	else sprintf(buf, "VIDEO");
	retd = Draw(dst, side, i++, buf, np);
	if (retd == -1) 
	{
		i = i_saved;
		side += strack+2;
		retd = Draw(dst, side, i++, buf, np);
	}
	while (retd < -1) 
	{
		retd = Draw(dst,side,i++,buf,np,-retd-2);
	}
}