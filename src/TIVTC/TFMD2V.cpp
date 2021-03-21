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

#include <cstring>
#include <memory>
#include "TFM.h"

void TFM::parseD2V(IScriptEnvironment *env)
{
  std::vector<int> valIn;
  int error, D2Vformat, tff = -1, frames;
  bool found = false;
  char wfile[1024];
  error = D2V_initialize_array(valIn, D2Vformat, frames);
  if (error != 0)
  {
    if (error == 1) env->ThrowError("TFM:  could not open specified d2v file!");
    else if (error == 2) env->ThrowError("TFM:  d2v file is not a d2v file or is of unsupported format!");
    else if (error == 3) env->ThrowError("TFM:  malloc failure (d2v)!");
    return;
  }
  if (debug)
  {
    sprintf(buf, "TFM:  successfully opened specified d2v file.");
    OutputDebugString(buf);
    if (D2Vformat > 9) sprintf(buf, "TFM:  newest style (dgindex 1.2+) d2v detected.\n");
    else if (D2Vformat > 3) sprintf(buf, "TFM:  new style (dgindex 1.0+) d2v detected.\n");
    else if (D2Vformat > 0) sprintf(buf, "TFM:  new style (dvd2avidg 1.2+) d2v detected.\n");
    else sprintf(buf, "TFM:  old style (dvd2avi 1.76 or 1.77) d2v detected.\n");
    OutputDebugString(buf);
  }
  error = D2V_find_and_correct(valIn, found, tff);
  if (error != 0 || tff == -1)
  {
    if (tff == -1) env->ThrowError("TFM:  unknown error (no entries in d2v file?)!");
    else if (error == 1) env->ThrowError("TFM:  illegal transition exists after fixing d2v file!");
    else if (error == 2) env->ThrowError("TFM:  ignored rff exists after fixing d2v file!");
    return;
  }
  if (order == -1)
  {
    order = tff;
    if (field == -1) field = tff;
    if (debug)
    {
      sprintf(buf, "TFM:  auto detected field order from d2v is %s.\n", order == 1 ? "TFF" : "BFF");
      OutputDebugString(buf);
    }
  }
  else if (order != tff)
    env->ThrowError("TFM:  the field order of the d2v does not match the user specified field order!");
  if (!found)
  {
    if (debug)
    {
      sprintf(buf, "TFM:  no errors found in d2v.\n");
      OutputDebugString(buf);
    }
    if (flags != 3)
    {
      if (trimIn.size())
      {
        error = fillTrimArray(frames);
        if (error == 1) env->ThrowError("TFM:  malloc failure (trimArray)!");
        else if (error == 2) env->ThrowError("TFM:  couldn't open trimIn file!");
        else if (error == 3) env->ThrowError("TFM:  error parsing trimIn file. " \
          "Out of range frame numbers or invalid format!");
        else if (error == 4) env->ThrowError("TFM:  frame count using trimIn file " \
          "doesn't match filter frame count!");
      }
      else if (frames != vi.num_frames)
      {
         env->ThrowError("TFM:  d2v frame count does not match filter frame count (%d vs %d)!",
          frames, vi.num_frames);
      }
      error = D2V_fill_d2vfilmarray(valIn, frames);
       if (error == 2) env->ThrowError("TFM:  malloc failure (d2vt)!");
      else if (error == 3) env->ThrowError("TFM:  malloc failure (d2vfilmarray, 2)!");
      else if (error != 0) env->ThrowError("TFM:  malloc failure (d2vfilmarray)!");
    }
    return;
  }
  error = D2V_get_output_filename(wfile);
  if (error != 0)
  {
    env->ThrowError("TFM:  could not obtain output d2v filename!");
    return;
  }
  error = D2V_write_array(valIn, wfile);
  if (error != 0)
  {
    if (error == 1) env->ThrowError("\tERROR:  could not open specified d2v file! Qutting...\n");
    else if (error == 2) env->ThrowError("\tERROR:  could not create output d2v file! Qutting...\n");
    else if (error == 3) env->ThrowError("\tERROR:  specified file is not a d2v file or is of unsupported format! Qutting...\n");
    return;
  }
  env->ThrowError("TFM:  Illegal transitions found in dvd2avi project file!\n"
    "          Please use the fixed version that has been created\n"
    "          in the same directory as the original d2v file.");
}

int TFM::fillTrimArray(int frames)
{
  trimArray.resize(frames, 1); // bool array, default 1
  int x, y, v;
  char linein[81];
  if (sscanf(trimIn.c_str(), "%d,%d", &x, &y) == 2)
  {
    if (x < 0 && abs(x) <= frames)
      x = frames + x;
    if (y < 0 && abs(y) <= frames)
      y = frames + y;
    if (x < 0 || x >= frames || x > y || y < 0 || y >= frames)
      return 3;
    for (v = x; v <= y; ++v)
      trimArray[v] = 0;
  }
  else
  {
    FILE *f = tivtc_fopen(trimIn.c_str(), "r");
    if (f == nullptr) return 2;
    while (fgets(linein, 80, f) != nullptr)
    {
      sscanf(linein, "%d,%d", &x, &y);
      if (x < 0 && abs(x) <= frames)
        x = frames + x;
      if (y < 0 && abs(y) <= frames)
        y = frames + y;
      if (x < 0 || x >= frames || x > y || y < 0 || y >= frames)
        return 3;
      for (v = x; v <= y; ++v)
        trimArray[v] = 0;
    }
  }
  for (v = 0, x = 0; x < frames; ++x)
  {
    if (trimArray[x]) ++v;
  }
  if (v != vi.num_frames) return 4;
  return 0;
}

int TFM::D2V_find_and_correct(std::vector<int> &array, bool &found, int &tff) const
{
  found = false;
  tff = -1;
  int count = 1, sync = 0, f1, f2, fix, temp, change;
  while (array[count] != 9)
  {
    if (tff == -1)
    {
      if (array[count - 1] < 2) tff = 0;
      else tff = 1;
    }
    fix = D2V_check_illegal(array[count - 1], array[count]);
    if (!fix)
    {
      ++count;
      continue;
    }
    found = true;
    fix = false;
    if (array[count] == 0 && array[count + 1] == 3) fix = true;
    else if (array[count] == 2 && array[count + 1] == 1) fix = true;
    if (fix)
    {
      fix = D2V_check_illegal(array[count], array[count + 2]);
      if (!fix)
      {
        temp = array[count];
        array[count] = array[count + 1];
        array[count + 1] = temp;
        continue;
      }
    }
    fix = false;
    if (array[count - 1] == 1 && array[count] == 0) fix = true;
    else if (array[count - 1] == 3 && array[count] == 2) fix = true;
    if (fix)
    {
      fix = D2V_check_illegal(array[count], array[count + 1]);
      if (fix)
      {
        temp = array[count - 1];
        array[count - 1] = array[count];
        array[count] = temp;
        continue;
      }
    }
    D2V_find_fix(array[count - 1], array[count], sync, f1, f2, change);
    sync += change;
    if (f1 != -1) array[count - 1] = f1;
    else if (f2 != -1) array[count] = f2;
  }
  return D2V_check_final(array);
}

void TFM::D2V_find_fix(int a1, int a2, int sync, int &f1, int &f2, int &change) const
{
  f1 = f2 = -1;
  if (sync >= 0)
  {
  greater_than:
    if (a1 == 0 && a2 == 3) f2 = 0;
    else if (a1 == 1 && a2 == 0) f1 = 0;
    else if (a1 == 1 && a2 == 1) f1 = 0;
    else if (a1 == 2 && a2 == 1) f2 = 2;
    else if (a1 == 3 && a2 == 2) f1 = 2;
    else if (a1 == 3 && a2 == 3) f1 = 2;
    if (f1 != f2)
    {
      change = -1;
      return;
    }
    goto less_than;
  }
  else
  {
  less_than:
    if (a1 == 0 && a2 == 2) f1 = 1;
    else if (a1 == 0 && a2 == 3) f1 = 1;
    else if (a1 == 1 && a2 == 0) f2 = 3;
    else if (a1 == 2 && a2 == 0) f1 = 3;
    else if (a1 == 2 && a2 == 1) f1 = 3;
    else if (a1 == 3 && a2 == 2) f2 = 1;
    if (f1 != f2)
    {
      change = 1;
      return;
    }
    goto greater_than;
  }
}

bool TFM::D2V_check_illegal(int a1, int a2) const
{
  if (a1 == 0 && a2 == 2) return true;
  else if (a1 == 0 && a2 == 3) return true;
  else if (a1 == 1 && a2 == 0) return true;
  else if (a1 == 1 && a2 == 1) return true;
  else if (a1 == 2 && a2 == 0) return true;
  else if (a1 == 2 && a2 == 1) return true;
  else if (a1 == 3 && a2 == 2) return true;
  else if (a1 == 3 && a2 == 3) return true;
  return false;
}

int TFM::D2V_check_final(const std::vector<int> &array) const
{
  int i = 1, top = array[0] == 3 ? 1 : 0, bot = array[0] == 1 ? 1 : 0;
  while (array[i] != 9)
  {
    if (D2V_check_illegal(array[i - 1], array[i])) return 1;
    if (top)
    {
      if (array[i] == 1) top = bot = 0;
      else if (array[i] == 3) return 2;
    }
    else if (bot)
    {
      if (array[i] == 3) top = bot = 0;
      else if (array[i] == 1) return 2;
    }
    else
    {
      if (array[i] == 3) top = 1;
      else if (array[i] == 1) bot = 1;
    }
    ++i;
  }
  return 0;
}

int TFM::D2V_initialize_array(std::vector<int> &array, int &d2vtype, int &frames) const
{
  FILE *ind2v = nullptr;
  if (array.size() != 0) { array.resize(0); }
  int num = 0, num2 = 0, pass = 1, val, D2Vformat;
  char line[1025], *p;
pass2_start:
  ind2v = tivtc_fopen(d2v.c_str(), "r");
  if (ind2v == nullptr) return 1;
  if (pass == 2)
  {
    array.resize(num + 10, 9); // default 9
  }
  fgets(line, 1024, ind2v);
  D2Vformat = 0;
  if (strncmp(line, "DVD2AVIProjectFile", 18) != 0)
  {
    if (strncmp(line, "DGIndexProjectFile", 18) != 0)
    {
      fclose(ind2v);
      return 2;
    }
    sscanf(line, "DGIndexProjectFile%d", &D2Vformat);
    /* Disabled the check for newer formats
    if (D2Vformat > 14)
    {
      fclose(ind2v);
      ind2v = NULL;
      return 2;
    }
    */
    D2Vformat += 3;
  }
  if (D2Vformat == 0) sscanf(line, "DVD2AVIProjectFile%d", &D2Vformat);
  while (fgets(line, 1024, ind2v) != nullptr)
  {
    if (strncmp(line, "Location", 8) == 0) break;
  }
  fgets(line, 1024, ind2v);
  fgets(line, 1024, ind2v);
  do
  {
    p = line;
    while (*p++ != ' ');
    while (*p++ != ' ');
    if (D2Vformat > 9) while (*p++ != ' ');
    while (*p++ != ' ');
    if (D2Vformat > 0)
    {
      while (*p++ != ' ');
      while (*p++ != ' ');
      if (D2Vformat > 18)
        while (*p++ != ' ');
    }
    while (*p > 47 && *p < 123)
    {
      if (pass == 1) ++num;
      else
      {
        sscanf(p, "%x", &val);
        if (D2Vformat > 9)
        {
          if (D2Vformat > 10 && val == 0xFF) array[num2++] = 9;
          else if (D2Vformat == 10 && (val & 0x40)) array[num2++] = 9;
          else array[num2++] = (val & 0x03);
        }
        else array[num2++] = (val&~0x10);
      }
      while (*p != ' ' && *p != '\n') p++;
      p++;
    }
  } while ((fgets(line, 1024, ind2v) != nullptr) && line[0] > 47 && line[0] < 123);
  fclose(ind2v);
  if (pass == 1) { pass++; goto pass2_start; }
  d2vtype = D2Vformat;
  frames = 0;
  int i = 0;
  while (array[i] != 9)
  {
    if (array[i] & 1) frames += 3;
    else frames += 2;
    ++i;
  }
  frames >>= 1;
  return 0;
}

int TFM::D2V_write_array(const std::vector<int> &array, char wfile[]) const
{
  int num = 0, D2Vformat, val;
  char line[1025], *p, tbuf[16];
  FILE *ind2v = tivtc_fopen(d2v.c_str(), "r");
  if (ind2v == nullptr) return 1;
  FILE *outd2v = tivtc_fopen(wfile, "w");
  if (outd2v == nullptr) {
    fclose(ind2v);
    return 2;
  }
  fgets(line, 1024, ind2v);
  D2Vformat = 0;
  if (strncmp(line, "DVD2AVIProjectFile", 18) != 0)
  {
    if (strncmp(line, "DGIndexProjectFile", 18) != 0)
    {
      fclose(ind2v);
      fclose(outd2v); 
      return 3;
    }
    sscanf(line, "DGIndexProjectFile%d", &D2Vformat);
    /* Disabled the check for newer formats
    if (D2Vformat > 14)
    {
      fclose(ind2v);
      return 3;
    }
    */
    D2Vformat += 3;
  }
  if (D2Vformat == 0) sscanf(line, "DVD2AVIProjectFile%d", &D2Vformat);
  fputs(line, outd2v);
  while (fgets(line, 1024, ind2v) != nullptr)
  {
    fputs(line, outd2v);
    if (strncmp(line, "Location", 8) == 0) break;
  }
  fgets(line, 1024, ind2v);
  fputs(line, outd2v);
  fgets(line, 1024, ind2v);
  do
  {
    p = line;
    while (*p++ != ' ');
    while (*p++ != ' ');
    if (D2Vformat > 9) while (*p++ != ' ');
    while (*p++ != ' ');
    if (D2Vformat > 0)
    {
      while (*p++ != ' ');
      while (*p++ != ' ');
      if (D2Vformat > 18)
        while (*p++ != ' ');
    }
    while (*p > 47 && *p < 123)
    {
      if (D2Vformat < 10)
      {
        while (*(p + 1) >= '0' && *(p + 1) <= '9') p++;
        *p = array[num++] + '0';
      }
      else
      {
        sscanf(p, "%x", &val);
        if (array[num] != 9)
        {
          val &= ~0x03;
          val |= array[num++];
        }
        sprintf(tbuf, "%x", val);
        *p = tbuf[0]; ++p;
        *p = tbuf[1];
      }
      while (*p != ' ' && *p != '\n') p++;
      p++;
    }
    fputs(line, outd2v);
  } while ((fgets(line, 1024, ind2v) != nullptr) && line[0] > 47 && line[0] < 123);
  fputs(line, outd2v);
  while (fgets(line, 1024, ind2v) != nullptr) fputs(line, outd2v);
  fclose(ind2v);
  fclose(outd2v);
  return 0;
}

int TFM::D2V_get_output_filename(char wfile[]) const
{
  FILE *outd2v = nullptr;
  strcpy(wfile, d2v.c_str());
  char *p = wfile;
  while (*p != 0) p++;
  while (*p != 46) p--;
  *p++ = '-'; *p++ = 'F'; *p++ = 'I'; *p++ = 'X'; *p++ = 'E'; *p++ = 'D';
  *p++ = '.'; *p++ = 'd'; *p++ = '2'; *p++ = 'v'; *p = 0;
  bool checking = true;
  int inT = 1;
  while (checking && inT < 100)
  {
    outd2v = tivtc_fopen(wfile, "r");
    if (outd2v != nullptr)
    {
      fclose(outd2v);
      outd2v = nullptr;
      p = wfile;
      while (*p != 0) p++;
      while (*p != 46) p--;
      if (inT == 1)
      {
        *p++ = '_'; *p++ = inT + '0'; *p++ = '.'; *p++ = 'd';
        *p++ = '2'; *p++ = 'v'; *p = 0;
      }
      else if (inT < 10)
      {
        p--;
        *p++ = inT + '0'; *p++ = '.'; *p++ = 'd';
        *p++ = '2'; *p++ = 'v'; *p = 0;
      }
      else if (inT < 100)
      {
        p--;
        if (inT > 10) p--;
        *p++ = ((inT / 10) % 10) + '0';
        *p++ = (inT % 10) + '0';
        *p++ = '.'; *p++ = 'd'; *p++ = '2'; *p++ = 'v'; *p = 0;
      }
      else return 1;
      ++inT;
    }
    else checking = false;
  }
  outd2v = tivtc_fopen(wfile, "w");
  if (outd2v == NULL) return 2;
  fclose(outd2v);
  // delete file
#ifdef _WIN32
  _unlink(wfile);
#else
  unlink(wfile);
#endif
  return 0;
}

int TFM::D2V_fill_d2vfilmarray(const std::vector<int> &array, int frames)
{
  int i = 0, v, fields = 0, val, outpattern = 0;
  if (d2vfilmarray.size()) { d2vfilmarray.resize(0); }
  d2vfilmarray.resize(frames + 1, 0);
  while (array[i] != 9)
  {
    val = array[i];
    if (val & 1)
    {
      if (fields & 1)
      {
        d2vfilmarray[fields >> 1] |= 8;
        if (checkInPatternD2V(array, i))
        {
          d2vfilmarray[fields >> 1] |= 64;
          d2vfilmarray[(fields + 2) >> 1] |= 64;
        }
        else ++outpattern;
        d2vfilmarray[(fields + 2) >> 1] |= 4;
      }
      else
      {
        d2vfilmarray[fields >> 1] |= 4;
        if (checkInPatternD2V(array, i)) d2vfilmarray[fields >> 1] |= 64;
        else ++outpattern;
      }
      d2vfilmarray[(fields + 2) >> 1] |= val == 3 ? 0x3 : 0x1;
      fields += 3;
    }
    else
    {
      if (fields & 1) d2vfilmarray[fields >> 1] |= 8;
      else d2vfilmarray[fields >> 1] |= 4;
      if (checkInPatternD2V(array, i)) d2vfilmarray[fields >> 1] |= 64;
      else ++outpattern;
      fields += 2;
    }
    ++i;
  }
  if (i == 0) return 0;
  d2vpercent = double(i - outpattern)*100.0 / double(i);
  if (debug)
  {
    sprintf(buf, "TFM:  d2vflags = %d  out_of_pattern = %d  (%3.1f%s FILM)\n", i, outpattern,
      d2vpercent, "%");
    OutputDebugString(buf);
  }
  if (flags == 0) d2vpercent = -20.0;
  if (trimIn.size() && trimArray.size())
  {
    std::vector<uint8_t> d2vt(vi.num_frames, 0);
    for (v = 0, i = 0; i <= nfrms && v < frames; ++v)
    {
      if (trimArray[v])
      {
        if (v == 0 || trimArray[v - 1]) d2vt[i++] = d2vfilmarray[v];
        else ++i;
      }
    }
    d2vfilmarray.resize(0);
    d2vfilmarray.resize(vi.num_frames);
    memcpy(d2vfilmarray.data(), d2vt.data(), vi.num_frames * sizeof(unsigned char));
    trimArray.resize(0);   
  }
  return 0;
}

bool TFM::checkInPatternD2V(const std::vector<int> &array, int i) const
{
  if (array[i + 1] == 9
    && i > 3 && checkD2VCase((array[i - 4] << 12) + (array[i - 3] << 8) + (array[i - 2] << 4) + array[i - 1])
    && checkD2VCase((array[i - 3] << 12) + (array[i - 2] << 8) + (array[i - 1] << 4) + array[i + 0]))
    return true;
  if (array[i + 2] == 9 && array[i + 1] != 9
    && i > 2 && checkD2VCase((array[i - 3] << 12) + (array[i - 2] << 8) + (array[i - 1] << 4) + array[i + 0])
    && checkD2VCase((array[i - 2] << 12) + (array[i - 1] << 8) + (array[i - 0] << 4) + array[i + 1]))
    return true;
  if (i >= 2 && checkD2VCase((array[i - 2] << 12) + (array[i - 1] << 8) + (array[i - 0] << 4) + array[i + 1])
    && checkD2VCase((array[i - 1] << 12) + (array[i - 0] << 8) + (array[i + 1] << 4) + array[i + 2]))
    return true;
  if (i == 1 && checkD2VCase((array[i - 1] << 12) + (array[i - 0] << 8) + (array[i + 1] << 4) + array[i + 2])
    && checkD2VCase((array[i + 0] << 12) + (array[i + 1] << 8) + (array[i + 2] << 4) + array[i + 3]))
    return true;
  if (i == 0 && checkD2VCase((array[i + 0] << 12) + (array[i + 1] << 8) + (array[i + 2] << 4) + array[i + 3])
    && checkD2VCase((array[i + 1] << 12) + (array[i + 2] << 8) + (array[i + 3] << 4) + array[i + 4]))
    return true;
  return false;
}

bool TFM::checkD2VCase(int check) const
{
  switch (check)
  {
  case 0x123:
  case 0x1230:
  case 0x2301:
  case 0x3012:
    return true;
  default:
    return false;
  }
  return false;
}