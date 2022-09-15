## TDeint

**v1.8 (20201214) - pinterf**

- Fix: TDeint: ignore parameter 'chroma' and treat as false for greyscale input


**v1.7 (20200921) - pinterf**

- Fix: TDeint: crash when edeint is a 10+ bit clip

**v1.6 (20200611) - pinterf**

- Frame hints 10-16 bits
- Proper 16 bit combing detection

**v1.5 (20200513) - pinterf**

- Fix: mode=2 10-16 bit green screen
- Fix: mode=2 right side artifact regression in v1.4 (SSE2)

**v1.4 (20200512) - pinterf**

- 10-16 bit support
- Greyscale support
- Minor fixes on non-YV12 support
- fix crash when mode=2 and map>=3 and slow>0
- much more code clean and refactor

**v1.3 (20200508)**

- Add YV411 support, now all 8 bit planar YUV formats supported (except on debug display modes)
- more code clean and refactor
- Give error on greyscale or 10+ bit videos

**v1.2 (20200505)**

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

