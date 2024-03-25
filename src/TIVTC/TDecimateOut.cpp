/*
**                    TIVTC for AviSynth 2.6 interface
**
**   TIVTC includes a field matching filter (TFM) and a decimation
**   filter (TDecimate) which can be used together to achieve an
**   IVTC or for other uses. TIVTC currently supports 8 bit planar YUV and
**   YUY2 colorspaces.
**
**   Copyright (C) 2004-2008 Kevin Stone, additional work (C) 2020 pinterf
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
#include <algorithm>
#include "info.h"

void TDecimate::debugOutput1(int n, bool film, int blend)
{
  if (mode == 0 || (mode == 3 && vfrDec == 0))
  {
    sprintf(buf, "TDecimate:  %d: ", curr.frame);
    formatMetrics(curr);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", curr.frame);
    formatMatches(curr, prev);
    OutputDebugString(buf);
  }
  else
  {
    sprintf(buf, "TDecimate:  %d: ", prev.frame);
    formatMetrics(prev);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", prev.frame);
    formatDups(prev);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", prev.frame);
    formatMatches(prev, prev);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", curr.frame);
    formatMetrics(curr);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", curr.frame);
    formatDups(curr);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", curr.frame);
    formatMatches(curr, prev);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", next.frame);
    formatMetrics(next);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", next.frame);
    formatDups(next);
    OutputDebugString(buf);
    sprintf(buf, "TDecimate:  %d: ", next.frame);
    formatMatches(next, curr);
    OutputDebugString(buf);
  }
  if (film)
  {
    if (cycleR > 1 || blend == 3) sprintf(buf, "TDecimate:  %d:  Dropping Frames:", n);
    else sprintf(buf, "TDecimate:  %d:  Dropping Frame:", n);
    formatDecs(curr, false, -1, -1);
  }
  else sprintf(buf, "TDecimate:  %d:  VIDEO", n);
  OutputDebugString(buf);
}

void TDecimate::debugOutput2(int n, int ret, bool film, int f1, int f2, double amount1,
  double amount2)
{
  if (amount1 == 0.0 && amount2 == 0.0)
    sprintf(buf, "TDecimate:  inframe = %d  useframe = %d\n", n, ret);
  else sprintf(buf, "TDecimate:  inframe: %d  useframe = blend %d-%d (%3.2f,%3.2f)\n",
    n, f1, f2, amount1*100.0, amount2*100.0);
  OutputDebugString(buf);
}

void TDecimate::formatMetrics(Cycle &current)
{
  char tempBuf[40];
  for (int i = current.cycleS; i < current.cycleE; ++i)
  {
    sprintf(tempBuf, " %3.2f", current.diffMetricsN[i]);
    strcat(buf, tempBuf);
  }
  strcat(buf, "\n");
}

void TDecimate::formatDups(Cycle &current)
{
  char tempBuf[40];
  for (int i = current.cycleS; i < current.cycleE; ++i)
  {
    sprintf(tempBuf, " %d", current.dupArray[i]);
    strcat(buf, tempBuf);
  }
  strcat(buf, "\n");
}

void TDecimate::formatDecs(Cycle &current, bool displayDecimationDefined, int displayFrom, int displayTo)
{
  char tempBuf[40];
  int i = current.cycleS, b = current.frameSO;

  size_t buflen = strlen(buf);
  for (; i < current.cycleE; ++i, ++b)
  {
    if (current.decimate[i] == 1)
    {
      if (!displayDecimationDefined || (b >= displayFrom && b <= displayTo)) {
        sprintf(tempBuf, " %d", b);
        buflen += strlen(tempBuf);
        if(buflen + 1 < sizeof(buf))
          strcat(buf, tempBuf);
      }
    }
  }
}

void TDecimate::formatMatches(Cycle &current)
{
  char tempBuf[40];
  for (int i = current.cycleS; i < current.cycleE; ++i)
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
    mp = previous.cycleE > 0 ? previous.match[previous.cycleE - 1] : -20;
  else mp = -20;
  int mc = current.match[current.cycleS];
  for (int i = current.cycleS; i < current.cycleE; ++i)
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
    if (i < current.cycleE - 1) mc = current.match[i + 1];
  }
  strcat(buf, "\n");
}

void TDecimate::addMetricCycle(const Cycle &j)
{
  if (metricsOutArray.size() == 0) return;
  int i = j.cycleS, p = j.frameSO;
  for (; i < j.cycleE; ++i, ++p)
  {
    metricsOutArray[p << 1] = j.diffMetricsU[i];
    metricsOutArray[(p << 1) + 1] = j.diffMetricsUF[i];
  }
}

void TDecimate::displayOutput(IScriptEnvironment* env, PVideoFrame &dst, int n,
  int ret, bool film, double amount1, double amount2, int f1, int f2, const VideoInfo &vi_disp)
{
  int y = 0;
  char tempBuf[40];
  sprintf(buf, "TDecimate %s by tritical", VERSION);

  constexpr auto FONT_WIDTH = 10; // info_h
  constexpr auto FONT_HEIGHT = 20; // info_h
  const int MAX_X = vi_disp.width / FONT_WIDTH;
  const int MAX_Y = vi_disp.height / FONT_HEIGHT;

  Draw(dst, 0, y++, buf, vi_disp);

  const int info_disp_y = y;
  y++;
  // print it at the end with optional extended statistics
  /*
  sprintf(buf, "Mode: %d  Cycle: %d  CycleR: %d  Hybrid: %d", mode, cycle, cycleR, hybrid);
  Draw(dst, 0, y++, buf, vi_disp);
  */

  if (amount1 == 0.0 && amount2 == 0.0)
    sprintf(buf, "inframe: %d  useframe: %d", n, ret);
  else sprintf(buf, "inframe: %d  useframe: blend %d-%d (%3.2f,%3.2f)", n, f1, f2,
    amount1*100.0, amount2*100.0);

  const bool displayDecimationDefined = displayDecimation > 0;
  const bool displayOptDefined = displayOpt > 0;
  const int displayFrom = !displayDecimationDefined ? -1 : (ret / displayDecimation) * displayDecimation;
  const int displayTo = !displayDecimationDefined ? -1 : displayFrom + displayDecimation - 1;
  if (displayDecimationDefined) {
    sprintf(tempBuf, "  displayDecimation: %d (%d-%d)", displayDecimation, displayFrom, displayTo);
    strcat(buf, tempBuf);
  }
  Draw(dst, 0, y++, buf, vi_disp);

  int y_saved = y;
  int current_column_x = 0;
  int max_column_width = 0;

  int num_of_decimations = 0;
  int num_of_decimations_in_display = 0;
  int num_of_decimations_till_display_end = 0;
  int last_decimated = curr.cycleS;

  const bool mode0or3spec = mode == 0 || (mode == 3 && vfrDec == 0);

  int mp = prev.frame != -20 ? prev.match[prev.cycleE - 1] : -20;
  int mc = curr.match[curr.cycleS];
  for (int x = curr.cycleS; x < curr.cycleE; ++x)
  {
    bool decimated = (curr.decimate[x] == 1);
    if (decimated) {
      num_of_decimations++;
      if (curr.frame + x <= displayTo) num_of_decimations_till_display_end++;
    }

    if (!displayDecimationDefined ||
      (curr.frame + x >= displayFrom && curr.frame + x <= displayTo)
      )
    {
      int decimation_relation = 0;
      if (decimated) {
        num_of_decimations_in_display++;
        if (displayOptDefined && num_of_decimations > 1) { // the first decimation does not get comparison markers
          const int num_of_nondecimations_since_last = x - last_decimated;
          // num_of_nondecimations_since_last <=> displayOptDefined: draw << or ** or >>
          decimation_relation = (num_of_nondecimations_since_last < displayOpt) ? -1 : (num_of_nondecimations_since_last > displayOpt) ? 1 : 0;
        }
      }
      sprintf(buf, "%d%s%3.2f", curr.frame + x, decimated ? (decimation_relation < 0 ? ":<<" : decimation_relation > 0 ? ":>>" : ":**") : ":  ",
      curr.diffMetricsN[x]);
      if (mc >= 0)
      {
        sprintf(tempBuf, " %c", MTC(mc));
        strcat(buf, tempBuf);
      }
      if (!mode0or3spec) {
        sprintf(tempBuf, " %s", curr.dupArray[x] == 1 ? "(dup)" : "(new)");
        strcat(buf, tempBuf);
      }
      if (mc >= 0)
      {
        if (checkMatchDup(mp, mc))
        {
          sprintf(tempBuf, " (%s)", "mdup");
          strcat(buf, tempBuf);
        }
        if (curr.filmd2v[x] == 1)
        {
          sprintf(tempBuf, " (%s)", "d2vdup");
          strcat(buf, tempBuf);
        }
      }

      int len = (int)strlen(buf);

      int retd = Draw(dst, current_column_x, y++, buf, vi_disp);
      // retd is 
      // >=0: column width printed 
      // -1 if does not fit vertically 
      // (-2-length_written) if does not fit horizontally
      if (retd == -1)
      {
        // does not fit vertically
        current_column_x += max_column_width + 2; // make x to next column, leaving a gap
        max_column_width = 0; // reset width counter
        y = y_saved; // back to the top of the area
        Draw(dst, current_column_x, y++, buf, vi_disp);
      }
      else
        max_column_width = std::max(max_column_width, retd); // get max width so far in current column
    }
    mp = mc;
    if (x < curr.cycleE - 1) mc = curr.match[x + 1];
    if (decimated) last_decimated = x;
  }
  if (film)
  {
    sprintf(buf, "FILM, Drop:");
    formatDecs(curr, displayDecimationDefined, displayFrom, displayTo);
  }
  else sprintf(buf, "VIDEO");

  int len = (int)strlen(buf);

  int retd = Draw(dst, current_column_x, y++, buf, vi_disp);
  if (retd == -1)
  {
    y = y_saved;
    current_column_x += max_column_width + 2;
    retd = Draw(dst, current_column_x, y++, buf, vi_disp);
  }

  int length_available = (MAX_X - current_column_x);
  int buf_offset = length_available;
  len -= length_available;

  // print rest buffer in a line-wrapped style
  while (retd < -1)
  {
    retd = Draw(dst, current_column_x, y++, buf + buf_offset, vi_disp);
    if (retd < 0 && retd != -1) {
      int printed = (-retd - 2);
      buf_offset += printed;
      //len -= length_available;
    }
  }

  // display top with extra statistics for displayDecimationDefined case
  if (!displayDecimationDefined)
    sprintf(buf, "Mode: %d  Cycle: %d  CycleR: %d  Hybrid: %d", mode, cycle, cycleR, hybrid);
    // confirm sceneDec is set correctly for backwards compatibility
    // sprintf(buf, "Mode: %d  Cycle: %d  CycleR: %d  Hybrid: %d sceneDec: %d", mode, cycle, cycleR, hybrid, sceneDec);
  else
    sprintf(buf, "Mode: %d  Cycle: %d  CycleR: %d  Hybrid: %d  #ofDecimations: %d (%d:%d)", mode, cycle, cycleR, hybrid, 
      num_of_decimations_in_display, num_of_decimations_till_display_end, cycleR - num_of_decimations_till_display_end);
  // #ofDecimations: 49 (2347:5432)
  // #ofDecimations: num_of_decimations_in_display (yyyy:CycleR-yyyy)
  // where yyyy is num_of_decimations_till_display_end
  Draw(dst, 0, info_disp_y, buf, vi_disp);

}
