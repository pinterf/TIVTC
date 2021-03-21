/*
**   Helper methods for TIVTC and TDeint
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
#include "internal.h"
#include <memory.h>

FILE *tivtc_fopen(const char *name, const char *mode) {
#ifdef _WIN32
    /*
    // VapourSynth:
    int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
    std::wstring wname(len, 0);

    int ret = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname.data(), len);
    if (ret == len) {
        std::wstring wmode(mode, mode + strlen(mode));
        return _wfopen(wname.c_str(), wmode.c_str());
    } else
        throw TIVTCError("Failed to convert file name to wide char.");
    */
    // Windows Avisynth source files are non-UTF8.
    // But in Win10 we can set native utf8 codepage which will work here flawlessly
    return fopen(name, mode);
#else
    return fopen(name, mode);
#endif
}

void BitBlt(uint8_t* dstp, int dst_pitch, const uint8_t* srcp,
  int src_pitch, int row_size, int height)
{
  if (!height || !row_size) return;
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size))
    memcpy(dstp, srcp, src_pitch * height);
  else
  {
    for (int y = height; y > 0; --y)
    {
      memcpy(dstp, srcp, row_size);
      dstp += dst_pitch;
      srcp += src_pitch;
    }
  }
}

#include "internal.h"
