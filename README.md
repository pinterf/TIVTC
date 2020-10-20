# TIVTC v1.0.23 (20201020) 
# TDeInt v1.7 (20200921)

This is a modernization effort on tritical's TIVTC (v1.0.5) and TDeInt (v1.1.1) plugin for Avisynth by pinterf

# TDeint (see change log of TIVTC later) 
** TDeInt v1.7 (20200921) - pinterf
- Fix: TDeint: crash when edeint is a 10+ bit clip

** TDeInt v1.6 (20200611) - pinterf
- Frame hints 10-16 bits
- Proper 16 bit combing detection

** TDeInt v1.5 (20200513) - pinterf
- Fix: mode=2 10-16 bit green screen
- Fix: mode=2 right side artifact regression in v1.4 (SSE2)

** TDeInt v1.4 (20200512) - pinterf
- 10-16 bit support
- Greyscale support
- Minor fixes on non-YV12 support
- fix crash when mode=2 and map>=3 and slow>0
- much more code clean and refactor

** TDeInt v1.3 (20200508) **
- Add YV411 support, now all 8 bit planar YUV formats supported (except on debug display modes)
- more code clean and refactor
- Give error on greyscale or 10+ bit videos

** TDeInt v1.2 (20200505) **
- Add AviSynth+ V8 interface support: passing frame properties
- Add planar YV16 and YV24 color spaces (The Big Work)
  result: YV16 output is identical with YUY2 (but a bit slower at the moment)
- Fix mode=0 for yuy2 (asm code was completely off)
- Fix mode=0 (general), luma was never processed in CheckedComb
- Fix crash with AviSynth+ versions (in general: when frame buffer alignment is more than 16 bytes)
- TDeint: refactor, code clean, c++17 conformity, keep C and SSE2
- Inline assembler code ported to intrinsics and C code. 
- Add some more SSE2 (MMX and ISSE code removed)
- x64 version is compilable!
- Add ClangCL, and XP configurations to the solutions.

# TIVTC v1.0.23 (20201020)

**v1.0.23 (20201020)**
- RequestLinear: fix: initial large frame number difference out of order frame requests
  caused by heavy multithreading could result in "internal error - frame not in cache"

**v1.0.22 (20200805)**
- TDecimate, FrameDiff: further fix of SAD based metric calculation for block size 32 (v15 regression)
  (report and fix by 299792458m)

**v1.0.21 (20200727)**
- TDecimate, FrameDiff: fix x-block size usage in metric calculation (v15 regression)
  (report and fix by 299792458m)

**v1.0.20 (20200622)**
- TFM: fix crash when PP=1 and display=true (v19 regression)

**v1.0.19 (20200611)**
- TIVTC filter: overall greyscale and 10-16 bit support
- "display" works for all colorspaces (not only for YUY2 and YV12)
  (v1.0.18 - no release)
- Fix: TFM: possible crash on YV16 for chroma=true (checkComb)
- Fix: TDecimate: fix mode=5 crash at the initializing stage due to an unallocated metric buffer (old bug)
- Fix: TFM: y0 (and y1) banding exclusion parameters are properly handled 
       (by knowing that a frame is processed between 2 and (y0-2) for internal algorithmic reasons)

**v1.0.17 (20200512)**
- Fix: TDecimate clip2 colorspace check
- Fix: Metric calculation (regression after v14)

**v1.0.16 (20200510)**
- Fix: TFMPP clip2 colorspace similarity check failed

**v1.0.15 (20200508)**
- Fix random crashes (due to old plugin assumed that Avisynth framebuffer alignment is at most 16 bytes)
- Other small fixes, which I do not know what affected
- Support planar YV411, YV16 and YV24 besides YV12 (YUY2 was not removed)
  (except on debug display modes)
- Huge refactor and code clean, made some parts common with TDeint, code un-duplicate-triplicate
- only C and SSE2, no MMX, no ISSE
- parameter opt=0 disables SSE2, 1-3 enables (was: 1:MMX 2:ISSE 3:SSE2)
- Add ClangCL, and XP configurations to the solutions. (note: MSVC can be quicker(!))
- Add AviSynth+ V8 interface support: passing frame properties
- Give error on greyscale or 10+ bit videos
- Todo: more refactor needed before moving to 10+ bit depth support

**v1.0.14 (20190207)**
- Fix: option slow=2 field<>0. Thanks to 299792458m. 
  Regression since 1.0.6 caused by bad assembly code reverse engineering. Tritical's original 1.0.5 was O.K.

**v1.0.13** (skipped)
**v1.0.12 (20190207)**
  (incomplete, 1.0.14 replaces)

**v1.0.11 (20180323)**
- Revert to pre-1.0.9 usehint detection: conflicted with mode 5 (sometimes bad clip length was reported)
  (reason of the workaround (crash at mysterious circumstances) was eliminated: mmx state was not cleared in mvtools2) 
- Fix: bad check for emptiness of orgOut parameter

**v1.0.10 (20180119)**
- integrate new orgOut parameter for TDecimate (by 8day) (see TDecimate - READ ME.txt)

**v1.0.9 (20170608)**
- Fix (workaround): Move frame hints detection from constructor into the first GetFrame (x64 build with 64 bit x264 crash under mysterious circumstances)
- Filters autoregister themselves as MT_SERIALIZED for Avisynth+, except MergeHints (MT_MULTI_INSTANCE)
  Note: for proper serialized behaviour under Avisynth+ MT, please use avs+ r2504 or later.

**v1.0.8 (20170429)**
- Fix: TFM PP=2 and PP=5 (Blend deint)

**v1.0.7 (20170427)**
- fix crash in FieldDiff (in new SIMD SSE2 rewrite)

**v1.0.6 (20170421) - pinterf**
- project migrated to VS 2015
- AVS 2.6 interface, no Avisynth 2.5.x support
- some fixes
- x64 port and readability: move all inline asm to simd intrinsics or C
- supports and requires SSE2
- MMX and ISSE is not supported, but kept in the source code for reference
- source code cleanups

**v1.0.5 (2008) - tritical**
- see old readmes

Future plans: support additional color spaces, now YV12 and YUY2 is supported

All credit goes to tritical, thanks for his work.

Useful links:

- General filter info: http://avisynth.nl/index.php/TIVTC
- This mod is based on the original code http://web.archive.org/web/20140420181748/http://bengal.missouri.edu/~kes25c/TIVTCv105.zip
- Project source: https://github.com/pinterf/TIVTC
- Doom9 topic: https://forum.doom9.org/showthread.php?t=82264
