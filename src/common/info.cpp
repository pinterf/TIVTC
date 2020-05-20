// original IT0051 by thejam79
// add YV12 mode by minamina 2003/05/01
//
// Borrowed from the author of IT.dll, whose name I
// could not determine. Modified for YV12 by Donald Graft.
// RGB32 Added by Klaus Post
// Converted to generic planar, and now using exact coordinates - NOT character coordinates by Klaus Post
// Refactor, DrawString...() is the primative, Oct 2010 Ian Brabham
// TO DO: Clean up - and move functions to a .c file.

// pinterf:
// high bit depth, planar RGB
// utf8 option, internally unicode
// info_h font definition reorganized, Latin-1 Supplement 00A0-00FF
// Configurable color
// Configurable halocolor (text outline)
// Configurable background fading
// Alignment
// multiline
// multiple size, multiple fonts, "Terminus", "info_h"

#include "info.h"

#ifdef AVS_WINDOWS
#include <io.h>
#include <avs/win.h>
#else
#include <avs/posix.h>
#include "parser/os/win32_string_compat.h"
#endif

#include <cassert>
#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cstring>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <array>
#include <iomanip>
#include "fixedfonts.h"

#ifdef AVS_WINDOWS
static std::unique_ptr<wchar_t[]> AnsiToWideCharACP(const char* s_ansi)
{
  const size_t bufsize = strlen(s_ansi) + 1;
  auto w_string = std::make_unique<wchar_t[]>(bufsize);
  MultiByteToWideChar(CP_ACP, 0, s_ansi, -1, w_string.get(), (int)bufsize);
  //mbstowcs(script_name_w, script_name, len); // ansi to wchar_t, does not convert properly out-of-the box
  return w_string;
}

static std::unique_ptr<wchar_t[]> Utf8ToWideChar(const char* s_ansi)
{
  const size_t wchars_count = MultiByteToWideChar(CP_UTF8, 0, s_ansi, -1, NULL, 0);
  const size_t bufsize = wchars_count + 1;
  auto w_string = std::make_unique<wchar_t[]>(bufsize);
  MultiByteToWideChar(CP_UTF8, 0, s_ansi, -1, w_string.get(), (int)bufsize);
  return w_string;
}
#endif

std::wstring charToWstring(const char* text, bool utf8)
{
  std::wstring ws;
  // AVS_POSIX: utf8 is always true, no ANSI here
#ifdef AVS_POSIX
  utf8 = true;
#endif
  if (utf8) {
    // warning C4996 :
    // ...and the <codecvt> header(containing std::codecvt_mode, std::codecvt_utf8, std::codecvt_utf16, and std::codecvt_utf8_utf16)
    // are deprecated in C++17.
    // (The std::codecvt class template is NOT deprecated.)
    // The C++ Standard doesn't provide equivalent non-deprecated functionality;
    // consider using MultiByteToWideChar() and WideCharToMultiByte() from <Windows.h> instead.
    // You can define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING or _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to acknowledge that you have received this warning.

    // utf8 to wchar_t
#if defined(MSVC_PURE)
    // workround for v141_xp toolset suxxx: unresolved externals
    auto wsource = Utf8ToWideChar(text);
    ws = wsource.get();
#else
    // MSVC++ 14.2  _MSC_VER == 1920 (Visual Studio 2019 version 16.0) v142 toolset
    // MSVC++ 14.16 _MSC_VER == 1916 (Visual Studio 2017 version 15.9) v141_xp toolset
    // this one suxx with MSVC v141_xp toolset: unresolved external error

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;

    // crash warning, strings here are considered as being a valid utf8 sequence
    // which is not the case when our own original VersionString() is passed here with an embedded ansi encoded (C) symbol
    // Crash occurs: "\n\xA9 2000-2015 Ben Rudiak-Gould, et al.\nhttp://avisynth.nl\n\xA9 2013-2020 AviSynth+ Project"
    // O.K.: u8"\n\u00A9 2000-2015 Ben Rudiak-Gould, et al.\nhttp://avisynth.nl\n\u00A9 2013-2020 AviSynth+ Project"
    // FIXME: make it crash-tolerant?
    try {
      ws = convert.from_bytes(text);
    }
    catch (const std::exception&) {
      ws = L"Error converting utf8 string to wide string\r\n";
      // FIXME: Throw a proper avs exception
    }
#endif
  }
#ifdef AVS_WINDOWS
  else {
    // ANSI, Windows
    auto wsource = AnsiToWideCharACP(text);
    ws = wsource.get();
  }
#endif
  return ws;
}


// generate outline on-the-fly
// consider: thicker outline for font sizes over 24?
void BitmapFont::generateOutline(uint16_t* outlined, int fontindex) const
{
  const uint16_t* currentfont = &font_bitmaps[height * fontindex];
  for (int i = 0; i < height; i++)
    outlined[i] = 0;

  auto make_dizzyLR = [](uint16_t fontline) {
    return (uint16_t)((fontline << 1) | (fontline >> 1));
  };
  auto make_dizzyLCR = [](uint16_t fontline) {
    return (uint16_t)(fontline | (fontline << 1) | (fontline >> 1));
  };

  // font bitmap is left (msb) aligned, if width is less than 16
  // fixme: why? because having 0x8000 as a fixed position rendering mask?
  const uint16_t mask = ((1 << width) - 1) << (16 - width);
  // 5432109876543210
  // 0000001111111111

  uint16_t prev_line = 0;
  uint16_t current_line = 0;
  uint16_t next_line;
  for (int i = 0; i < height - 1; i++)
  {
    current_line = currentfont[i];
    next_line = currentfont[i + 1];
    uint16_t line = make_dizzyLCR(prev_line) | make_dizzyLR(current_line) | make_dizzyLCR(next_line);

    uint16_t value = (line & ~current_line) & mask;
    outlined[i] = value;

    prev_line = current_line;
    current_line = next_line;
  }
  // last one, no next line
  uint16_t line = make_dizzyLCR(prev_line) | make_dizzyLR(current_line) | make_dizzyLCR(0);
  uint16_t value = (line & ~current_line) & mask;
  outlined[height - 1] = value;
}

// helper function for remapping a wchar_t string to font index entry list
std::vector<int> BitmapFont::remap(const std::wstring& ws)
{
  // new vector with characters remapped to table indexes
  std::vector<int> s_remapped;
  s_remapped.resize(ws.size());
  for (size_t i = 0; i < ws.size(); i++) {
    auto it = charReMap.find(ws[i]);
    if (it != charReMap.end())
      s_remapped[i] = it->second;
    else
      s_remapped[i] = 0; // empty neutral character (space)
  }
  return s_remapped;
}

typedef struct CharInfo { // STARTCHAR charname
  std::string friendly_name;
  uint16_t encoding;
} CharDef;

typedef struct FontProperties {
  std::string Copyright;
  std::string Notice;
  std::string Family_name;
  std::string Weight_name;
  int pixel_size;
  int font_ascent; // 12
  int font_descent; // 4
  uint16_t default_char; // 65533
} FontProperties;

typedef struct FontInfo {
  std::string font; // n/a
  int sizeA, sizeB, sizeC; // n/a
  int font_bounding_box_x;
  int font_bounding_box_y;
  int font_bounding_box_what;
  int font_bounding_box_what2; // -4 e.g. baseline?
  int chars; // number of characters
} FontInfo;

class BdfFont {
public:
  std::string font_filename;
  FontInfo font_info;
  FontProperties font_properties;
  std::vector<uint16_t> codepoints_array;
  std::vector<std::string> charnames_array;
  std::vector<uint16_t> font_bitmaps; // uint16_t: max. width is 16
};

#include <locale>
#include <fstream>

std::string UnQuote(std::string s) {
  if (s.size() >= 2 && s.substr(0, 1) == "\"" && (s.substr(s.size() - 1, 1) == "\""))
    return s.substr(1,s.size()-2); // zero based
  return s;
}


static constexpr int ATA_LEFT = 1;
static constexpr int ATA_RIGHT = 2;
static constexpr int ATA_CENTER = 4;

static constexpr int ATA_TOP = 8;
static constexpr int ATA_BOTTOM = 16;
static constexpr int ATA_BASELINE = 32;

static int alignToBitmask(int align_1_to_9)
{
  // alignment 1-9: digit positions on numeric keypad
  int al = 0;
  switch (align_1_to_9) // This spec where [X, Y] is relative to the text (inverted logic)
  {
  case 1: al = ATA_BOTTOM | ATA_LEFT; break;     // .----
  case 2: al = ATA_BOTTOM | ATA_CENTER; break;   // --.--
  case 3: al = ATA_BOTTOM | ATA_RIGHT; break;    // ----.
  case 4: al = ATA_BASELINE | ATA_LEFT; break;   // .____
  case 5: al = ATA_BASELINE | ATA_CENTER; break; // __.__
  case 6: al = ATA_BASELINE | ATA_RIGHT; break;  // ____.
  case 7: al = ATA_TOP | ATA_LEFT; break;        // `----
  case 8: al = ATA_TOP | ATA_CENTER; break;      // --`--
  case 9: al = ATA_TOP | ATA_RIGHT; break;       // ----`
  default: al = ATA_BASELINE | ATA_LEFT; break;  // .____
  }
  return al;
}

static int getColorForPlane(int plane, int color)
{
  switch (plane) {
  case PLANAR_A:
    return (color >> 24) & 0xff; break;
  case PLANAR_R:
  case PLANAR_Y:
    return (color >> 16) & 0xff; break;
  case PLANAR_G:
  case PLANAR_U:
    return (color >> 8) & 0xff; break;
  case PLANAR_B:
  case PLANAR_V:
    return color & 0xff; break;
  }
  return color & 0xFF;
}

template<bool fadeBackground>
void AVS_FORCEINLINE LightOnePixelYUY2(const bool lightIt, BYTE* dp, int val_color, int val_color_U, int val_color_V)
{
  if (lightIt) { // character definition bits aligned to msb
    if (size_t(dp) & 2) { // Assume dstp is dword aligned
      dp[0] = val_color;
      dp[-1] = val_color_U;
      dp[1] = val_color_V;
    }
    else {
      dp[0] = val_color;
      dp[1] = val_color_U;
      dp[3] = val_color_V;
    }
  }
  else {
    if constexpr (fadeBackground) {
      if (size_t(dp) & 2) {
        dp[0] = (unsigned char)((dp[0] * 7) >> 3) + 2;
        dp[-1] = (unsigned char)((dp[-1] * 7) >> 3) + 16;
        dp[1] = (unsigned char)((dp[1] * 7) >> 3) + 16;
      }
      else {
        dp[0] = (unsigned char)((dp[0] * 7) >> 3) + 2;
        dp[1] = (unsigned char)((dp[1] * 7) >> 3) + 16;
        dp[3] = (unsigned char)((dp[3] * 7) >> 3) + 16;
      }
    }
  }
}

template<typename pixel_t, bool fadeBackground>
void AVS_FORCEINLINE LightOnePixelRGB(const bool lightIt, BYTE* _dp, int val_color_R, int val_color_G, int val_color_B)
{
  pixel_t* dp = reinterpret_cast<pixel_t*>(_dp);
  if (lightIt) { // character definition bits aligned to msb
    dp[0] = val_color_B;
    dp[1] = val_color_G;
    dp[2] = val_color_R;
  }
  else {
    if constexpr (fadeBackground) {
      dp[0] = (pixel_t)((dp[0] * 7) >> 3);
      dp[1] = (pixel_t)((dp[1] * 7) >> 3);
      dp[2] = (pixel_t)((dp[2] * 7) >> 3);
    }
  }
}

template<typename pixel_t, int bits_per_pixel, bool fadeBackground, bool isRGB>
void AVS_FORCEINLINE LightOnePixel(const bool lightIt, pixel_t* dstp, int j, pixel_t& val_color)
{
  if (lightIt) { // character definition bits aligned to msb
    dstp[j] = val_color;
  }
  else {
    // 16 = y_min
    // speed optimization: one subtraction less, 5-8% faster
    // (((Y - 16) * 7) >> 3) + 16 = ((Y * 7) >> 3) + 2
    // in general: ((Y * 7) >> 3) + n, where n = range_min - ((range_min * 7) >> 3)
    if constexpr (fadeBackground) {
      // background darkening
      if constexpr (isRGB) {
        if constexpr (sizeof(pixel_t) != 4)
          dstp[j] = (pixel_t)((dstp[j] * 7) >> 3);
        else {
          constexpr float factor = 7.0f / 8;
          dstp[j] = (pixel_t)(dstp[j] * factor);
        }
      }
      else {
        if constexpr (sizeof(pixel_t) != 4) {
          constexpr int range_min = 16 << (bits_per_pixel - 8);
          constexpr int n = range_min - ((range_min * 7) >> 3);
          dstp[j] = (pixel_t)(((dstp[j] * 7) >> 3) + n); // (_dstp[j] - range_min) * 7) >> 3) + range_min);
        }
        else {
          constexpr float range_min_f = 16.0f / 255.0f;
          dstp[j] = (pixel_t)(((dstp[j] - range_min_f) * 7 / 8) + range_min_f);
        }
      }
    }
  }
}

template<typename pixel_t, int bits_per_pixel, bool fadeBackground, bool isRGB>
void LightOneUVPixel(const bool lightIt, pixel_t* dstpU, int j, pixel_t& color_u, pixel_t* dstpV, pixel_t& color_v)
{
  if (lightIt) { // lit if any font pixels are on under the 1-2-4 pixel-wide mask
      dstpU[j] = color_u; // originally: neutral U and V (128)
      dstpV[j] = color_v;
  }
  else {
    // speed optimization: one subtraction less
    // (((U - 128) * 7) >> 3) + 128 = ((U * 7) >> 3) + 16
    // in general: ((U * 7) >> 3) + n where n = range_half - ((range_half * 7) >> 3)
    if constexpr (fadeBackground) {
      if constexpr (sizeof(pixel_t) != 4) {
        constexpr int range_half = 1 << (bits_per_pixel - 1);
        constexpr int n = range_half - ((range_half * 7) >> 3);
        dstpU[j] = (pixel_t)(((dstpU[j] * 7) >> 3) + n); // ((((U - range_half) * 7) >> 3) + range_half);
        dstpV[j] = (pixel_t)(((dstpV[j] * 7) >> 3) + n);
      }
      else {
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
        constexpr float shift = 0.5f;
#else
        constexpr float shift = 0.0f;
#endif
        constexpr float factor = 7.0f / 8.0f;
        dstpU[j] = (pixel_t)(((dstpU[j] - shift) * factor) + shift);
        dstpV[j] = (pixel_t)(((dstpV[j] - shift) * factor) + shift);
      }
    }
  }
}

static void adjustWriteLimits(std::vector<int>& s, const int width, const int height, const int FONT_WIDTH, const int FONT_HEIGHT, int align, int& x, int& y, int& len, int& startindex, int& xstart, int& ystart, int& yend)
{
  const int al = alignToBitmask(align);

  // alignment X
  if (al & ATA_RIGHT)
    x -= (FONT_WIDTH * len + 1);
  else if (al & ATA_CENTER)
    x -= (FONT_WIDTH * len / 2);

  // alignment Y
  if (al & ATA_BASELINE)
    y -= FONT_HEIGHT / 2;
  else if (al & ATA_BOTTOM)
    y -= (FONT_HEIGHT + 1);

  // Chop text if exceed right margin
  if (len * FONT_WIDTH > width - x)
    len = (width - x) / FONT_WIDTH;

  startindex = 0;
  xstart = 0;
  // Chop 1st char if exceed left margin
  if (x < 0) {
    startindex = (-x) / FONT_WIDTH;
    xstart = (-x) % FONT_WIDTH;
    x = 0;
  }

  ystart = 0;
  yend = FONT_HEIGHT;
  // Chop font if exceed bottom margin
  if (y > height - FONT_HEIGHT)
    yend = height - y;

  // Chop font if exceed top margin
  if (y < 0) {
    ystart = -y;
    y = 0;
  }

  // Roll in start index
  if (startindex > 0) {
    s.erase(s.begin(), s.begin() + startindex - 1);
    len -= startindex;
  }
}

template<int bits_per_pixel, bool fadeBackground, bool isRGB>
void do_DrawStringPlanar(const int width, const int height, BYTE **dstps, int *pitches, const int logXRatioUV, const int logYRatioUV, const int planeCount,
  const BitmapFont *bmfont, int x, int y, std::vector<int>& s, int color, int halocolor, int align, bool useHalocolor)
{

  // define pixel_t as uint8_t, uint16_t or float, based on bits_per_pixel
  typedef typename std::conditional<bits_per_pixel == 8, uint8_t, typename std::conditional < bits_per_pixel <= 16, uint16_t, float > ::type >::type pixel_t;

  std::vector<uint16_t> current_outlined_char(bmfont->height);
  const uint16_t* fonts = bmfont->font_bitmaps.data();
  const int FONT_WIDTH = bmfont->width;
  const int FONT_HEIGHT = bmfont->height;

  // x, y: pixels
  int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  int* planes = isRGB ? planes_r : planes_y;

  const int pixelsize = sizeof(pixel_t);

  // Default string length
  int len = (int)s.size();
  int startindex;
  int xstart;
  int ystart;
  int yend;

  adjustWriteLimits(s, width, height, FONT_WIDTH, FONT_HEIGHT, align,
    // adjusted parameters
    x, y, len, startindex, xstart, ystart, yend);

  if (len <= 0)
    return;

  pixel_t val_color;
  pixel_t val_color_outline;

  // some helper lambdas
  auto getHBDColor_UV = [](int color) {
    if constexpr (bits_per_pixel < 32)
      return (pixel_t)(color << (bits_per_pixel - 8));
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
    constexpr float shift = 0.5f;
#else
    constexpr float shift = 0.0f;
#endif
    return (pixel_t)((color - 128) / 255.0f + shift); // 32 bit float chroma 128=0.5
    // FIXME: consistently using limited->fullscale conversion for float
  };

  auto getHBDColor_Y = [](int color) {
    if constexpr (bits_per_pixel < 32)
      return (pixel_t)(color << (bits_per_pixel - 8));
    return (pixel_t)(color / 255.0f); // 0..255 -> 0..1.0
    // FIXME: consistently using limited->fullscale conversion for float
  };

  auto getHBDColor_RGB = [](int color) {
    if constexpr (bits_per_pixel <= 16) {
      constexpr int max_pixel_value = (1 << (bits_per_pixel & 31)) - 1;
      return (pixel_t)((float)color * max_pixel_value / 255); // 0..255 --> 0..1023,4095,16383,65535
    }
    return (pixel_t)(color / 255.0f); // 0..255 -> 0..1.0
  };

  for (int p = 0; p < planeCount; p++)
  {
    int plane = planes[p];

    if (!isRGB && plane != PLANAR_Y)
      continue; // handle U and V specially. Y, R, G, B is O.K.

    int planecolor = getColorForPlane(plane, color);
    int planecolor_outline = getColorForPlane(plane, halocolor);
    if (isRGB) {
      val_color = getHBDColor_RGB(planecolor);
      val_color_outline = getHBDColor_RGB(planecolor_outline);
    }
    else if (plane == PLANAR_U || plane == PLANAR_V) {
      val_color = getHBDColor_UV(planecolor);
      val_color_outline = getHBDColor_UV(planecolor_outline);
    }
    else {// Y
      val_color = getHBDColor_Y(planecolor);
      val_color_outline = getHBDColor_Y(planecolor_outline);
    }

    const int pitch = pitches[p];
    BYTE* dstp = dstps[p] + x * pixelsize + y * pitch;

    // Start rendering
    for (int ty = ystart; ty < yend; ty++) {
      int num = s[0];

      unsigned int fontline;
      unsigned int fontoutline;

      fontline = fonts[num * FONT_HEIGHT + ty] << xstart; // shift some pixels if leftmost is chopped

      if (useHalocolor) {
        bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
        fontoutline = current_outlined_char[ty] << xstart; // shift some pixels if leftmost is chopped
      }

      int current_xstart = xstart; // leftmost can be chopped
      int j = 0;
      pixel_t* _dstp = reinterpret_cast<pixel_t*>(dstp);

      for (int i = 0; i < len; i++) {
        for (int tx = current_xstart; tx < FONT_WIDTH; tx++) {
          const bool lightIt = fontline & 0x8000;
          LightOnePixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(lightIt, _dstp, j, val_color);
          if (useHalocolor) {
            if (!lightIt) // it can be outline
              LightOnePixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(fontoutline & 0x8000, _dstp, j, val_color_outline);
          }
          j += 1;
          fontline <<= 1; // next pixel to the left
          if (useHalocolor)
            fontoutline <<= 1;
        }
        current_xstart = 0; // further characters are not chopped

        if (i + 1 < len)
        {
          num = s[i + 1];
          if (useHalocolor) {
            bmfont->generateOutline(current_outlined_char.data(), num);
            fontoutline = current_outlined_char[ty]; // shift some pixels if leftmost is chopped
          }
          fontline = fonts[num * FONT_HEIGHT + ty];
        }
      }
      dstp += pitch;
    }
  }

  if constexpr (isRGB)
    return;

  if (planeCount < 3)
    return; // Y

  // draw U and V in one step
  pixel_t color_u = getHBDColor_UV((color >> 8) & 0xff);
  pixel_t color_v = getHBDColor_UV(color & 0xff);
  pixel_t color_outline_u = getHBDColor_UV((halocolor >> 8) & 0xff);
  pixel_t color_outline_v = getHBDColor_UV(halocolor & 0xff);

  const int pitchUV = pitches[1];

  // .SubS = 1, 2 or 4
  const int xSubS = 1 << logXRatioUV;
  const int ySubS = 1 << logYRatioUV;
  const int offset = (x >> logXRatioUV) * pixelsize + (y >> logYRatioUV) * pitchUV;

  BYTE* dstpU = dstps[1] + offset;
  BYTE* dstpV = dstps[2] + offset;

  // fontmask = 0x2000000, 0x3000000 or 0x3C00000; 1, 2 or 4 bits
  unsigned int fontmask = 0;
  for (int i = 0; i < xSubS; i++) {
    fontmask >>= 1;
    fontmask |= 0x8000 << FONT_WIDTH;
  }

  /*
        U and V handling, multiple possible source for a given
        01 23 45 67 89 01
        .. .. OO O. .. ..
        .. .O .. .O .. ..
        .. .O .. .O .. ..
  */

  for (int ty = ystart; ty < yend; ty += ySubS) {
    int i, j, num;
    unsigned int fontline = 0;
    unsigned int fontoutline = 0;

    // two characters at a time

    num = s[0];
    // Or in vertical subsampling of glyph
    for (int m = 0; m < ySubS; m++) fontline |= fonts[num * FONT_HEIGHT + ty + m];
    if (useHalocolor) {
      bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
      for (int m = 0; m < ySubS; m++) fontoutline |= current_outlined_char[ty + m];
    }
    //             AAAAAAAAAA000000
    //             BBBBBBBBBB000000
    //             aaaaaaaaaa000000 // or'd

    fontline <<= FONT_WIDTH; // Move 1st glyph up
    if (useHalocolor) fontoutline <<= FONT_WIDTH;
    //   aaaaaaaaaa0000000000000000 // shift up by 10

    if (1 < len) {
      num = s[1];
      // Or in vertical subsampling of 2nd glyph
      for (int m = 0; m < ySubS; m++) fontline |= fonts[num * FONT_HEIGHT + ty + m];
      if (useHalocolor) {
        bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
        for (int m = 0; m < ySubS; m++) fontoutline |= current_outlined_char[ty + m];
      }
    }

    //   aaaaaaaaaa0000000000000000
    //             CCCCCCCCCC000000
    //             DDDDDDDDDD000000
    //   aaaaaaaaaabbbbbbbbbb000000 // two fonts together
    //   fontmasks  (depends on horizontal subsampling)
    //   1000......................
    //   1100......................
    //   1111......................

    // Cope with left crop of glyph
    fontline <<= xstart;
    if (useHalocolor) fontoutline <<= xstart;
    int _xs = xstart;

    pixel_t* _dstpU = reinterpret_cast<pixel_t*>(dstpU);
    pixel_t* _dstpV = reinterpret_cast<pixel_t*>(dstpV);

    // handle two characters at a time because one pixel may consist of two fonts
    // when we have horizontal subsampling.
    // Note: extremely ugly for 411!
    for (i = 1, j = 0; i < len; i += 2) {
      for (int tx = _xs; tx < 2 * FONT_WIDTH; tx += xSubS) {
        const bool lightIt = (fontline & fontmask);
        LightOneUVPixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(lightIt, _dstpU, j, color_u, _dstpV, color_v);
        if (useHalocolor) {
          if (!lightIt) // it can be outline
            LightOneUVPixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(fontoutline & fontmask, _dstpU, j, color_outline_u, _dstpV, color_outline_v);
        }
        j += 1;
        fontline <<= xSubS;
        if (useHalocolor)
          fontoutline <<= xSubS;
      }
      _xs = 0;
      fontline = 0;
      if (useHalocolor)
        fontoutline = 0;

      if (i + 1 < len) {
        num = s[i + 1];
        for (int m = 0; m < ySubS; m++) fontline |= fonts[num * FONT_HEIGHT + ty + m];;
        fontline <<= FONT_WIDTH;

        if (useHalocolor) {
          bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
          for (int m = 0; m < ySubS; m++) fontoutline |= current_outlined_char[ty + m];
          fontoutline <<= FONT_WIDTH;
        }

        if (i + 2 < len) {
          num = s[i + 2];
          for (int m = 0; m < ySubS; m++) fontline |= fonts[num * FONT_HEIGHT + ty + m];

          if (useHalocolor) {
            bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
            for (int m = 0; m < ySubS; m++) fontoutline |= current_outlined_char[ty + m];
          }
        }
      }
    }

    // Do odd length last glyph
    if (i == len) {
      for (int tx = _xs; tx < FONT_WIDTH; tx += xSubS) {
        const bool lightIt = (fontline & fontmask);
        LightOneUVPixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(lightIt, _dstpU, j, color_u, _dstpV, color_v);
        if (useHalocolor) {
          if (!lightIt) // it can be outline
            LightOneUVPixel<pixel_t, bits_per_pixel, fadeBackground, isRGB>(fontoutline & fontmask, _dstpU, j, color_outline_u, _dstpV, color_outline_v);
        }
        j += 1;
        fontline <<= xSubS;
        if (useHalocolor)
          fontoutline <<= xSubS;
      }
    }
    dstpU += pitchUV;
    dstpV += pitchUV;
  }
}

template<bool fadeBackground>
static void do_DrawStringYUY2(const int width, const int height, BYTE* _dstp, int pitch, const BitmapFont *bmfont, int x, int y, std::vector<int>& s, int color, int halocolor, int align, bool useHalocolor)
{
  // fixedFontRec_t current_outlined_char;

  std::vector<uint16_t> current_outlined_char(bmfont->height);
  const uint16_t *fonts = bmfont->font_bitmaps.data();
  const int FONT_WIDTH = bmfont->width;
  const int FONT_HEIGHT = bmfont->height;

  // Default string length
  int len = (int)s.size();
  int startindex;
  int xstart;
  int ystart;
  int yend;

  adjustWriteLimits(s, width, height, FONT_WIDTH, FONT_HEIGHT, align,
    // adjusted parameters
    x, y, len, startindex, xstart, ystart, yend);

  if (len <= 0)
    return;

  int val_color = getColorForPlane(PLANAR_Y, color);
  int val_color_outline = getColorForPlane(PLANAR_Y, halocolor);
  int val_color_U = getColorForPlane(PLANAR_U, color);
  int val_color_U_outline = getColorForPlane(PLANAR_U, halocolor);
  int val_color_V = getColorForPlane(PLANAR_V, color);
  int val_color_V_outline = getColorForPlane(PLANAR_V, halocolor);

  BYTE* dstp = _dstp + x * 2 + y * pitch;

  // Start rendering
  for (int ty = ystart; ty < yend; ty++, dstp += pitch) {
    BYTE* dp = dstp;

    int num = s[0];

    unsigned int fontline;
    unsigned int fontoutline;

    fontline = fonts[num * FONT_HEIGHT + ty] << xstart; // shift some pixels if leftmost is chopped

    if (useHalocolor) {
      bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
      fontoutline = current_outlined_char[ty] << xstart; // shift some pixels if leftmost is chopped
    }

    int current_xstart = xstart; // leftmost can be chopped

    for (int i = 0; i < len; i++) {
      for (int tx = current_xstart; tx < FONT_WIDTH; tx++) {
        const bool lightIt = fontline & 0x8000;
        LightOnePixelYUY2<fadeBackground>(lightIt, dp, val_color, val_color_U, val_color_V);
        if (useHalocolor) {
          if (!lightIt) // it can be outline
            LightOnePixelYUY2<fadeBackground>(fontoutline & 0x8000, dp, val_color_outline, val_color_U_outline, val_color_V_outline);
        }
        dp += 2;
        fontline <<= 1; // next pixel to the left
        if (useHalocolor)
          fontoutline <<= 1;
      }

      current_xstart = 0;

      if (i + 1 < len)
      {
        num = s[i + 1];
        if (useHalocolor) {
          bmfont->generateOutline(current_outlined_char.data(), num);
          fontoutline = current_outlined_char[ty]; // shift some pixels if leftmost is chopped
        }
        fontline = fonts[num * FONT_HEIGHT + ty];
      }
    }
  }
}

template<int bits_per_pixel, int rgbstep, bool fadeBackground>
static void do_DrawStringPackedRGB(const int width, const int height, BYTE* _dstp, int pitch, const BitmapFont *bmfont, int x, int y, std::vector<int>& s, int color, int halocolor, int align, bool useHalocolor)
{
  // define pixel_t as uint8_t, uint16_t or float, based on bits_per_pixel
  typedef typename std::conditional<bits_per_pixel == 8, uint8_t, typename std::conditional < bits_per_pixel <= 16, uint16_t, float > ::type >::type pixel_t;

  auto getHBDColor_RGB = [](int color) {
    if constexpr (bits_per_pixel <= 16) {
      constexpr int max_pixel_value = (1 << bits_per_pixel) - 1;
      return (pixel_t)(color * max_pixel_value / 255); // 0..255 --> 0..1023,4095,16383,65535
    }
    return (pixel_t)(color / 255.0f); // 0..255 -> 0..1.0
  };


  std::vector<uint16_t> current_outlined_char(bmfont->height);
  const uint16_t* fonts = bmfont->font_bitmaps.data();
  const int FONT_WIDTH = bmfont->width;
  const int FONT_HEIGHT = bmfont->height;


  // Default string length
  int len = (int)s.size();
  int startindex;
  int xstart;
  int ystart;
  int yend;

  adjustWriteLimits(s, width, height, FONT_WIDTH, FONT_HEIGHT, align,
    // adjusted parameters
    x, y, len, startindex, xstart, ystart, yend);

  if (len <= 0)
    return;

  int val_color_R = getHBDColor_RGB(getColorForPlane(PLANAR_R, color));
  int val_color_R_outline = getHBDColor_RGB(getColorForPlane(PLANAR_R, halocolor));
  int val_color_G = getHBDColor_RGB(getColorForPlane(PLANAR_G, color));
  int val_color_G_outline = getHBDColor_RGB(getColorForPlane(PLANAR_G, halocolor));
  int val_color_B = getHBDColor_RGB(getColorForPlane(PLANAR_B, color));
  int val_color_B_outline = getHBDColor_RGB(getColorForPlane(PLANAR_B, halocolor));

  // upside down
  BYTE* dstp = _dstp + x * rgbstep + (height - 1 - y) * pitch;;

  // Start rendering
  for (int ty = ystart; ty < yend; ty++, dstp -= pitch) {
    BYTE* dp = dstp;

    int num = s[0];

    unsigned int fontline;
    unsigned int fontoutline;

    fontline = fonts[num * FONT_HEIGHT + ty] << xstart; // shift some pixels if leftmost is chopped

    if (useHalocolor) {
      bmfont->generateOutline(current_outlined_char.data(), num); // on the fly, can be
      fontoutline = current_outlined_char[ty] << xstart; // shift some pixels if leftmost is chopped
    }

    int current_xstart = xstart; // leftmost can be chopped

    for (int i = 0; i < len; i++) {
      for (int tx = current_xstart; tx < FONT_WIDTH; tx++) {
        const bool lightIt = fontline & 0x8000;
        LightOnePixelRGB<pixel_t, fadeBackground>(lightIt, dp, val_color_R, val_color_G, val_color_B);
        if (useHalocolor) {
          if (!lightIt) // it can be outline
            LightOnePixelRGB<pixel_t, fadeBackground>(fontoutline & 0x8000, dp, val_color_R_outline, val_color_G_outline, val_color_B_outline);
        }
        dp += rgbstep;
        fontline <<= 1; // next pixel to the left
        if (useHalocolor)
          fontoutline <<= 1;
      }

      current_xstart = 0;

      if (i + 1 < len)
      {
        num = s[i + 1];
        if (useHalocolor) {
          bmfont->generateOutline(current_outlined_char.data(), num);
          fontoutline = current_outlined_char[ty]; // shift some pixels if leftmost is chopped
        }
        fontline = fonts[num * FONT_HEIGHT + ty];
      }
    }
  }
}

static bool strequals_i(const std::string& a, const std::string& b)
{
  return std::equal(a.begin(), a.end(),
    b.begin(), b.end(),
    [](char a, char b) {
      return tolower(a) == tolower(b);
    });
}

// in fixedfonts.cpp
extern const uint16_t *font_bitmaps[];
extern const uint16_t *font_codepoints[];
extern const FixedFont_info_t *font_infos[];

std::unique_ptr<BitmapFont> GetBitmapFont(int size, const char *name, bool bold, bool debugSave) {

  BitmapFont* current_font = nullptr;

  // check internal embedded fonts
  bool found = false;

  // find font in internal list
  for (int i = 0; i < PREDEFINED_FONT_COUNT; i++)
  {
    const FixedFont_info_t* fi = font_infos[i];
    if (fi->height == size && fi->bold == bold && strequals_i(fi->fontname, name)) {
      current_font = new BitmapFont(
        fi->charcount,
        font_bitmaps[i],
        font_codepoints[i],
        fi->width,
        fi->height,
        fi->fontname,
        "",
        fi->bold,
        false);
      found = true;
      break;
    }
  }
  // pass #2 when size does not match exactly, find nearest, but still smaller font.
  if (!found) {
    // find font i internal list
    int last_good_size = 0;
    int found_index = -1;
    for (int i = 0; i < PREDEFINED_FONT_COUNT; i++)
    {
      const FixedFont_info_t* fi = font_infos[i];
      if (fi->bold == bold && strequals_i(fi->fontname, name)) {
        if (last_good_size == 0) {
          found_index = i;
          last_good_size = fi->height;
        }
        else if (std::abs(fi->height - size) < std::abs(last_good_size - size) && fi->height <= size) {
          // has better size match and is not larger
          found_index = i;
          last_good_size = fi->height;
        }
      }
    }
    if (found_index >= 0) {
      const FixedFont_info_t* fi = font_infos[found_index];
      current_font = new BitmapFont(
        fi->charcount,
        font_bitmaps[found_index],
        font_codepoints[found_index],
        fi->width,
        fi->height,
        fi->fontname,
        "",
        fi->bold,
        false);
      found = true;
    }
  }

  if (!found) {
    return nullptr;
  }
  return std::unique_ptr<BitmapFont>(current_font);
}

static void DrawString_internal(BitmapFont *current_font, const VideoInfo& vi, PVideoFrame& dst, int x, int y, std::wstring &s16, int color, int halocolor, bool useHalocolor, int align, bool fadeBackground)
{
  // map unicode to character map index
  auto s_remapped = current_font->remap(s16); // array of font table indexes

  const bool isRGB = vi.IsRGB();
  const int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  const int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  const int* planes = isRGB ? planes_r : planes_y;

  int logXRatioUV = 0;
  int logYRatioUV = 0;
  if (!vi.IsY() && !vi.IsRGB()) {
    logXRatioUV = vi.IsYUY2() ? 1 : vi.GetPlaneWidthSubsampling(PLANAR_U);
    logYRatioUV = vi.IsYUY2() ? 0 : vi.GetPlaneHeightSubsampling(PLANAR_U);
  }
  int planecount = vi.IsYUY2() ? 1 : std::min(vi.NumComponents(), 3);
  BYTE* dstps[3];
  int pitches[3];

  for (int i = 0; i < planecount; i++)
  {
    int plane = planes[i];
    dstps[i] = dst->GetWritePtr(plane);
    pitches[i] = dst->GetPitch(plane);
  }

  const int width = vi.width;
  const int height = vi.height;

  // fixme: put parameter to a single struct

  const int bits_per_pixel = vi.BitsPerComponent();

  if (vi.IsYUY2()) {
    if (fadeBackground)
      do_DrawStringYUY2<true>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
    else
      do_DrawStringYUY2<false>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
    return;
  }

  // Packed RGB24/32/48/64
  if (isRGB && !vi.IsPlanar()) {
    if (fadeBackground) {
      if (vi.IsRGB24())
        do_DrawStringPackedRGB<8, 3, true>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB32())
        do_DrawStringPackedRGB<8, 4, true>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB48())
        do_DrawStringPackedRGB<16, 6, true>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB64())
        do_DrawStringPackedRGB<16, 8, true>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
    }
    else {
      if (vi.IsRGB24())
        do_DrawStringPackedRGB<8, 3, false>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB32())
        do_DrawStringPackedRGB<8, 4, false>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB48())
        do_DrawStringPackedRGB<16, 6, false>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
      else if (vi.IsRGB64())
        do_DrawStringPackedRGB<16, 8, false>(width, height, dstps[0], pitches[0], current_font, x, y, s_remapped, color, halocolor, align, useHalocolor);
    }
    return;
  }

  // planar and Y
  if (fadeBackground) {
    if (isRGB) {
      switch (bits_per_pixel)
      {
        // FIXME: we have outline inside there, pass halocolor and an option: bool outline
      case 8: do_DrawStringPlanar<8, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 10: do_DrawStringPlanar<10, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 12: do_DrawStringPlanar<12, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 14: do_DrawStringPlanar<14, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 16: do_DrawStringPlanar<16, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 32: do_DrawStringPlanar<32, true, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      }
    }
    else {
      switch (bits_per_pixel)
      {
      case 8: do_DrawStringPlanar<8, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 10: do_DrawStringPlanar<10, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 12: do_DrawStringPlanar<12, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 14: do_DrawStringPlanar<14, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 16: do_DrawStringPlanar<16, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 32: do_DrawStringPlanar<32, true, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      }
    }
  }
  else {
    if (isRGB) {
      switch (bits_per_pixel)
      {
      case 8: do_DrawStringPlanar<8, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 10: do_DrawStringPlanar<10, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 12: do_DrawStringPlanar<12, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 14: do_DrawStringPlanar<14, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 16: do_DrawStringPlanar<16, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 32: do_DrawStringPlanar<32, false, true>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      }
    }
    else {
      switch (bits_per_pixel)
      {
      case 8: do_DrawStringPlanar<8, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 10: do_DrawStringPlanar<10, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 12: do_DrawStringPlanar<12, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 14: do_DrawStringPlanar<14, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 16: do_DrawStringPlanar<16, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      case 32: do_DrawStringPlanar<32, false, false>(width, height, dstps, pitches, logXRatioUV, logYRatioUV, planecount, current_font, x, y, s_remapped, color, halocolor, align, useHalocolor); break;
      }
    }
  }
}

void SimpleTextOutW(BitmapFont *current_font, const VideoInfo& vi, PVideoFrame& frame, int real_x, int real_y, std::wstring& text, bool fadeBackground, int textcolor, int halocolor, bool useHaloColor, int align)
{
  DrawString_internal(current_font, vi, frame, real_x, real_y, text, textcolor, halocolor, useHaloColor, align, fadeBackground); // fully transparent background
}

// additional parameter: lsp line spacing
void SimpleTextOutW_multi(BitmapFont *current_font, const VideoInfo& vi, PVideoFrame& frame, int real_x, int real_y, std::wstring& text, bool fadeBackground, int textcolor, int halocolor, bool useHaloColor, int align, int lsp)
{

  // make list governed by LF separator
  using wstringstream = std::basic_stringstream<wchar_t>;
  std::wstring temp;
  std::vector<std::wstring> parts;
  wstringstream wss(text);
  while (std::getline(wss, temp, L'\n'))
    parts.push_back(temp);

  const int fontSize = current_font->height;

  // when multiline, bottom and vertically centered cases affect starting y
  int al = alignToBitmask(align);
  if (al & ATA_BOTTOM)
    real_y -= fontSize * ((int)parts.size() - 1);
  else if (al & ATA_BASELINE)
    real_y -= fontSize * ((int)parts.size() / 2);

  for (auto ws : parts) {
    SimpleTextOutW(current_font, vi, frame, real_x, real_y, ws, fadeBackground, textcolor, halocolor, useHaloColor, align);
    real_y += fontSize + lsp;
  }
}

// Old legacy info.h functions, but with utf8 mode
// w/o outline, originally with ASCII input, background fading
// unline name Planar, it works for all format
void DrawStringPlanar(VideoInfo& vi, PVideoFrame& dst, int x, int y, const char* s)
{
  int color;
  if (vi.IsRGB())
    color = (250 << 16) + (250 << 8) + (250);
  else
    color = (230 << 16) + (128 << 8) + (128);

  // fadeBackground = true: background letter area is faded instead not being untouched.

  std::wstring ws = charToWstring(s, false);

  int halocolor = 0;

  std::unique_ptr<BitmapFont> current_font = GetBitmapFont(20, "info_h", false, false); // 10x20

  if (current_font == nullptr)
    return;

  DrawString_internal(current_font.get(), vi, dst, x, y, ws,
    color,
    halocolor,
    false, // don't use halocolor
    7 /* upper left */,
    true // fadeBackGround
  );

}

void DrawStringYUY2(VideoInfo& vi, PVideoFrame& dst, int x, int y, const char* s)
{
  DrawStringPlanar(vi, dst, x, y, s); // same
}

// legacy function w/o outline, originally with ASCII input, background fading
void DrawStringRGB24(VideoInfo &vi, PVideoFrame& dst, int x, int y, const char* s)
{
  DrawStringPlanar(vi, dst, x, y, s); // same
}

// legacy function w/o outline, originally with ASCII input, background fading
void DrawStringRGB32(VideoInfo& vi, PVideoFrame& dst, int x, int y, const char* s)
{
  DrawStringPlanar(vi, dst, x, y, s); // same
}

/*
Draw and display helpers for TIVTC/TDeint
*/
static void fillBoxYUY2(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks, bool dot)
{
  uint8_t* dstp = dst->GetWritePtr();
  ptrdiff_t pitch = dst->GetPitch();
  int width = dst->GetRowSize();
  int height = dst->GetHeight();
  int cordy, cordx, x, y, temp, xlim, ylim;
  cordy = blockN / xblocks;
  cordx = blockN - (cordy * xblocks);
  temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= (blockx << 1);
  if (temp == 1) cordx -= blockx;
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= blockx; cordy -= (blocky >> 1); }
  xlim = cordx + 2 * blockx;
  ylim = cordy + blocky;
  int ymid = std::max(std::min(cordy + ((ylim - cordy) >> 1), height - 1), 0);
  int xmid = std::max(std::min(cordx + ((xlim - cordx) >> 1), width - 2), 0);
  if (xlim > width) xlim = width;
  if (ylim > height) ylim = height;
  cordy = std::max(cordy, 0);
  cordx = std::max(cordx, 0);
  dstp = dstp + cordy * pitch;
  for (y = cordy; y < ylim; ++y)
  {
    for (x = cordx; x < xlim; x += 4)
    {
      if (y == ymid && (x == xmid || x + 2 == xmid) && dot)
      {
        dstp[x] = 0;
        dstp[x + 2] = 0;
      }
      else if (!(dstp[x] == 0 && dstp[x + 2] == 0 &&
        (x <= 1 || dstp[x - 2] == 255) &&
        (x >= width - 4 || dstp[x + 4] == 255) &&
        (y == 0 || dstp[x - pitch] == 255) &&
        (y == height - 1 || dstp[x + pitch] == 255)) || !dot)
      {
        dstp[x] = 255;
        dstp[x + 2] = 255;
      }
    }
    dstp += pitch;
  }
}

static void fillBoxPlanar(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks, bool dot, const VideoInfo& vi)
{
  const int pixelsize = vi.ComponentSize();

  uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
  const int width = dst->GetRowSize(PLANAR_Y) / pixelsize;
  const int height = dst->GetHeight(PLANAR_Y);
  ptrdiff_t pitch = dst->GetPitch(PLANAR_Y);

  int cordy, cordx, xlim, ylim;

  cordy = blockN / xblocks;
  cordx = blockN - (cordy * xblocks);

  int temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= blockx;

  if (temp == 1) cordx -= (blockx >> 1);
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= (blockx >> 1); cordy -= (blocky >> 1); }

  xlim = cordx + blockx;
  ylim = cordy + blocky;

  int ymid = std::max(std::min(cordy + ((ylim - cordy) >> 1), height - 1), 0);
  int xmid = std::max(std::min(cordx + ((xlim - cordx) >> 1), width - 1), 0);
  if (xlim > width) xlim = width;
  if (ylim > height) ylim = height;
  cordy = std::max(cordy, 0);
  cordx = std::max(cordx, 0);
  dstp = dstp + cordy * pitch;

  const int bits_per_pixel = vi.BitsPerComponent();

  if (bits_per_pixel == 8) {
    for (int y = cordy; y < ylim; ++y)
    {
      for (int x = cordx; x < xlim; ++x)
      {
        if (y == ymid && x == xmid && dot) dstp[x] = 0;
        else if (!(dstp[x] == 0 &&
          (x == width - 1 || dstp[x + 1] == 255) &&
          (x == 0 || dstp[x - 1] == 255) &&
          (y == height - 1 || dstp[x + pitch] == 255) &&
          (y == 0 || dstp[x - pitch] == 255)) || !dot)
        {
          dstp[x] = 255;
        }
      }
      dstp += pitch;
    }
  }
  else if (bits_per_pixel <= 16) {
    const int max_pixel_value = (1 << bits_per_pixel) - 1;

    uint16_t* dstp16 = reinterpret_cast<uint16_t*>(dstp);
    ptrdiff_t pitch16 = pitch / sizeof(uint16_t);

    for (int y = cordy; y < ylim; ++y)
    {
      for (int x = cordx; x < xlim; ++x)
      {
        if (y == ymid && x == xmid && dot)
          dstp16[x] = 0;
        else if (!(dstp16[x] == 0 &&
          (x == width - 1 || dstp16[x + 1] == max_pixel_value) &&
          (x == 0 || dstp16[x - 1] == max_pixel_value) &&
          (y == height - 1 || dstp16[x + pitch16] == max_pixel_value) &&
          (y == 0 || dstp16[x - pitch16] == max_pixel_value)) || !dot)
        {
          dstp16[x] = max_pixel_value;
        }
      }
      dstp16 += pitch16;
    }
  }
}

static void drawBoxYUY2(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks)
{
  uint8_t* dstp = dst->GetWritePtr();
  ptrdiff_t pitch = dst->GetPitch();
  int width = dst->GetRowSize();
  int height = dst->GetHeight();
  int cordy, cordx, x, y, temp, xlim, ylim;
  cordy = blockN / xblocks;
  cordx = blockN - (cordy * xblocks);
  temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= (blockx << 1);
  if (temp == 1) cordx -= blockx;
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= blockx; cordy -= (blocky >> 1); }
  xlim = cordx + 2 * blockx;
  if (xlim > width) xlim = width;
  ylim = cordy + blocky;
  if (ylim > height) ylim = height;
  for (y = std::max(cordy, 0), temp = cordx + 2 * (blockx - 1); y < ylim; ++y)
  {
    if (cordx >= 0) (dstp + y * pitch)[cordx] = (dstp + y * pitch)[cordx] <= 128 ? 255 : 0;
    if (temp < width) (dstp + y * pitch)[temp] = (dstp + y * pitch)[temp] <= 128 ? 255 : 0;
  }
  for (x = std::max(cordx, 0), temp = cordy + blocky - 1; x < xlim; x += 4)
  {
    if (cordy >= 0)
    {
      (dstp + cordy * pitch)[x] = (dstp + cordy * pitch)[x] <= 128 ? 255 : 0;
      (dstp + cordy * pitch)[x + 1] = 128;
      (dstp + cordy * pitch)[x + 2] = (dstp + cordy * pitch)[x + 2] <= 128 ? 255 : 0;
      (dstp + cordy * pitch)[x + 3] = 128;
    }
    if (temp < height)
    {
      (dstp + temp * pitch)[x] = (dstp + temp * pitch)[x] <= 128 ? 255 : 0;
      (dstp + temp * pitch)[x + 1] = 128;
      (dstp + temp * pitch)[x + 2] = (dstp + temp * pitch)[x + 2] <= 128 ? 255 : 0;
      (dstp + temp * pitch)[x + 3] = 128;
    }
  }
}

static void drawBoxPlanar(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks, const VideoInfo& vi)
{
  const int pixelsize = vi.ComponentSize();

  uint8_t* dstp = dst->GetWritePtr(PLANAR_Y);
  const int width = dst->GetRowSize(PLANAR_Y) / pixelsize;
  const int height = dst->GetHeight(PLANAR_Y);
  ptrdiff_t pitch = dst->GetPitch(PLANAR_Y);
  int cordy, cordx, xlim, ylim;

  cordy = blockN / xblocks;
  cordx = blockN - (cordy * xblocks);

  int temp = cordx % 4;
  cordx = (cordx >> 2);
  cordy *= blocky;
  cordx *= blockx;

  if (temp == 1) cordx -= (blockx >> 1);
  else if (temp == 2) cordy -= (blocky >> 1);
  else if (temp == 3) { cordx -= (blockx >> 1); cordy -= (blocky >> 1); }

  xlim = cordx + blockx;
  if (xlim > width) xlim = width;
  ylim = cordy + blocky;
  if (ylim > height) ylim = height;

  const int bits_per_pixel = vi.BitsPerComponent();

  if (bits_per_pixel == 8) {
    for (int y = std::max(cordy, 0), temp = cordx + blockx - 1; y < ylim; ++y)
    {
      if (cordx >= 0)
        (dstp + y * pitch)[cordx] = (dstp + y * pitch)[cordx] <= 128 ? 255 : 0;
      if (temp < width)
        (dstp + y * pitch)[temp] = (dstp + y * pitch)[temp] <= 128 ? 255 : 0;
    }
    for (int x = std::max(cordx, 0), temp = cordy + blocky - 1; x < xlim; ++x)
    {
      if (cordy >= 0)
        (dstp + cordy * pitch)[x] = (dstp + cordy * pitch)[x] <= 128 ? 255 : 0;
      if (temp < height)
        (dstp + temp * pitch)[x] = (dstp + temp * pitch)[x] <= 128 ? 255 : 0;
    }
  }
  else if (bits_per_pixel <= 16) {
    const int max_pixel_value = (1 << bits_per_pixel) - 1;
    const int half = 1 << (bits_per_pixel - 1);
    for (int y = std::max(cordy, 0), temp = cordx + blockx - 1; y < ylim; ++y)
    {
      uint16_t* ptr1 = reinterpret_cast<uint16_t*>(dstp + y * pitch);
      uint16_t* ptr2 = reinterpret_cast<uint16_t*>(dstp + y * pitch);
      if (cordx >= 0)
        ptr1[cordx] = ptr1[cordx] <= half ? max_pixel_value : 0;
      if (temp < width)
        ptr2[temp] = ptr2[temp] <= half ? max_pixel_value : 0;
    }
    for (int x = std::max(cordx, 0), temp = cordy + blocky - 1; x < xlim; ++x)
    {
      uint16_t* ptr1 = reinterpret_cast<uint16_t*>(dstp + cordy * pitch);
      uint16_t* ptr2 = reinterpret_cast<uint16_t*>(dstp + temp * pitch);
      if (cordy >= 0)
        ptr1[x] = ptr1[x] <= half ? max_pixel_value : 0;
      if (temp < height)
        ptr2[x] = ptr2[x] <= half ? max_pixel_value : 0;
    }
  }
}

void fillBox(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks, bool dot, const VideoInfo& vi)
{
  if (vi.IsPlanar()) return fillBoxPlanar(dst, blockx, blocky, blockN, xblocks, dot, vi);
  else return fillBoxYUY2(dst, blockx, blocky, blockN, xblocks, dot);
}

void drawBox(PVideoFrame& dst, int blockx, int blocky, int blockN, int xblocks, const VideoInfo& vi)
{
  if (vi.IsPlanar()) drawBoxPlanar(dst, blockx, blocky, blockN, xblocks, vi);
  else drawBoxYUY2(dst, blockx, blocky, blockN, xblocks);
}

void Draw(PVideoFrame& dst, int x1, int y1, const char* s, const VideoInfo& vi)
{
  x1 *= 10;
  y1 = y1 * 20;
  DrawStringPlanar(const_cast<VideoInfo &>(vi), dst, x1, y1, s);
}

void setBlack(PVideoFrame& dst, const VideoInfo& vi)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int np = vi.IsYUY2() || vi.IsY() ? 1 : 3;

  for (int b = 0; b < np; ++b)
  {
    const int plane = planes[b];
    uint8_t* dstp = dst->GetWritePtr(plane);
    const int pitch = dst->GetPitch(plane);
    const size_t height = dst->GetHeight(plane);
    if (!vi.IsYUY2())
    {
      if (b == 0)
        memset(dstp, 0, pitch * height); // luma
      else {
        // chroma
        const int bits_per_pixel = vi.BitsPerComponent();
        if (bits_per_pixel == 8)
          memset(dstp, 128, pitch * height);
        else
          std::fill_n((uint16_t*)dstp, pitch * height / sizeof(uint16_t), 128 << (bits_per_pixel - 8));
      }
    }
    else
    {
      const int width = dst->GetRowSize(plane) >> 1;
      for (int y = 0; y < height; ++y)
      {
        for (int x = 0; x < width; ++x)
          ((uint16_t*)dstp)[x] = 0x8000;
        dstp += pitch;
      }
    }
  }
}


