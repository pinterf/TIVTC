# TIVTC v1.0.14 (20190207) # TDeInt v1.2 (20200505)

This is a modernization effort on tritical's TIVTC (v1.0.5) and TDeInt (v1.1.1) plugin for Avisynth by pinterf

# TDeint (see change log of TIVTC later) 

** TDeInt v1.2 (20200505 - work in progress) **
- Add AviSynth+ V8 interface support: passing frame properties
- Add planar YV16 and YV24 color spaces (The Big Work)
  result: YV16 output is identical with YUY2 (but a bit slower at the moment)
- Fix mode=0 for yuy2 (asm code was completely off)
- Fix mode=0 (general), luma was never processed in CheckedComb
- Fix crash with AviSynth+ versions (in general: when frame buffer alignment is more than 16 bytes)
- TDeint: refactor, code clean, c++17 conformity, keep C and SSE2
- Inline assembler code ported to intrinsics and C code. 
- Add some more SSE2 (MMX and ISSE code kept but is not active)
- x64 version is compilable!
- Add ClangCL, and XP configurations to the solutions.
  For some magic, TIVTC cannot be built with the v142 toolset and VS2019 16.5.2 :) :(
    INTERNAL COMPILER ERROR in 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.25.28610\bin\HostX86\x64\CL.exe'
    1>        Please choose the Technical Support command on the Visual C++
    1>        Help menu, or open the Technical Support help file for more information
    1>    The command exited with code -1073741819.

# TIVTC v1.0.14 (20190207):

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
