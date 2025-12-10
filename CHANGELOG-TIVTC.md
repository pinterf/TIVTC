## TIVTC

**v1.0.30 (20251210)
- TDecimate: add dclip parameter - a denoised clip for metrics calculation only.
  See #27: https://github.com/pinterf/TIVTC/issues/27
- TDecimate: Allow hybrid=1 scenechange decimation strategy when hybrid=0, new parameter "sceneDec"
  For mode 0 and 1.
  See #50: https://github.com/pinterf/TIVTC/pull/50 (flossy83)
  See also #27 for mode 1 extension
- TDecimate: Alternate strategy for TDecimate mode 1, lowDec parameter
  See #58: https://github.com/pinterf/TIVTC/pull/58 (flossy83)
- TFM: (#27) add dclip as an optional denoised clip for analyzing

**v1.0.29 (20240302)
- TDecimate: allow noblend=true when hybrid=1, noblend default value is false when hybrid=1 to keep 
  backward compatibility.
- TDecimate to fill new frame properties:
  TDecimateCycleMetrics (float array), TDecimateCycleMetricsPrev, TDecimateCycleMetricsNext,
  TDecimateCycleFrameNums (int array), TDecimateCycleFrameNumsPrev, TDecimateCycleFrameNumsNext,
  TDecimateCycleBlendStatus
  Issue #48: https://github.com/pinterf/TIVTC/issues/48
- Fix Issue #46: TDecimate(hybrid=3) blending with wrong frame
  https://github.com/pinterf/TIVTC/issues/46

**v1.0.28 (20231210)
- Request #43: (https://github.com/pinterf/TIVTC/issues/43)
  TDecimate debug parameters displayDecimation, displayOpt.
  Able to show << or >> instead of ** on debug display by the distance from the last decimated frame compared to displayOpt.
- Fix minor display glitch (regression since v1.0.19pack): display=true would duplicate the 
  most bottom frame info text to the top position of the next column.

**v1.0.27 (20230511)

- Fix #40: TDecimate: frame properties were not inherited at specific modes (20230511)
- Implement #39: CFrameDiff, FrameDiff: add offset parameter (int, default 1), to compare frames more than 1 distance away.
  offset value must be >=1, already existing prevf parameter is used to set the direction.

**v1.0.27test (20220915)
  (bugfixes, frame property support backport from VapourSynth version -20220915)
- Fix: TDecimate mode 0,1 crash in 10+bits in blend (dubhater)
- Fixes in Mode 0,1 when clip2 is different format (dubhater)
- Fix: slow C code was used in calcMetricCycle.blurframe (dubhater)
- Fix: V14(?) regression TDecimate fullInfo was always false (dubhater), ("Don't know what it affected")
- MacOS build fixes (akarin)
- mingw build fixes
- Source code: refactorings, backported from VapourSynth port (dubhater, https://github.com/dubhater/vapoursynth-tivtc), 
- Source code: backport other simplification changes.
- Frame properties (priority over burned-into-image hints). Similarly to bitmapped hints, they are used when parameter "hint" is true.
  Frame properties set by TFM: TFMMatch, _Combed, TFMD2VFilm, TFMField, TFMMics, TFMPP
  Frame properties read by TFM and TDecimate: TFMMatch, TFMD2VFilm, TFMField
  Frame properties read by TFMPP: TFMField, _Combed
  Frame properties read by TDecimate creation: TFMPP (but if Avisynth variable "TFMPPValue" exists then the variable has priority over it)
  Frame properties never read: TFMMics
  Traditionally, TFM is using a 32 bit "hint" encoded in the lsb bits of the first 32 pixels of a frame.
  When Avisynth interface V8 is available, then frame properties are used instead of the image destructing bit hints.
  - Bitmapped hint replacement frame property names are: TFMMatch, _Combed, TFMD2VFilm, TFMField.
      Mapping:
        Bits 0-2: (ISP, ISC, ISN, ISB, ISU, ISDB, ISDT) are filled upon match (TFMMatch) and combed (_Combed) and field (TFMField)
          ISP: match == 0
          ISC: match == 1 && combed < 2
          ISN: match == 2
          ISB: match == 3
          ISU: match == 4
          ISDB: match == 1 && combed > 1 && field == 0
          ISDT: match == 1 && combed > 1 && field == 1
        Bit 3: set to 1 if field == 1 (TFMField 0 or 1)
        Bit 4: set to 1 if combed > 1 (_Combed)
        Bit 5: set to 1 if d2vfilm is true (TFMD2VFilm)
  - Other frame properties:
      TFMMics: array[6] of integer
      TFMPP: integer
  Frame properties set by TDecimate:
    TDecimateCycleStart (not used)
    TDecimateCycleMaxBlockDiff uint64_t[] (not used)
    TDecimateOriginalFrame (not used)
    _DurationNum, _DurationDen (not used yet, for variable frame rate (?), backport from VapourSynth port is in progress)

**v1.0.26 (20210222)**

- Fix: TDecimate YV16 possible crash in metrics calculation

**v1.0.25 (20201214)**

- Fix: TFM, TDecimate and others: treat parameter 'chroma' as "false" for greyscale clips


**v1.0.24 (20201214)**

- Fix: TFM: do not give error on greyscale clip

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

**v1.0.13 (skipped)**
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
