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

#include <windows.h>
#include "internal.h"

AVSValue __cdecl Create_TFM(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_TDecimate(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_MergeHints(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_FieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_CFieldDiff(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_FrameDiff(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_CFrameDiff(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_ShowCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_IsCombedTIVTC(AVSValue args, void* user_data, IScriptEnvironment* env);
AVSValue __cdecl Create_RequestLinear(AVSValue args, void* user_data, IScriptEnvironment* env);

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env) 
{
    env->AddFunction("TFM", "c[order]i[field]i[mode]i[PP]i[ovr]s[input]s[output]s[outputC]s" \
							"[debug]b[display]b[slow]i[mChroma]b[cNum]i[cthresh]i[MI]i" \
							"[chroma]b[blockx]i[blocky]i[y0]i[y1]i[mthresh]i[clip2]c[d2v]s" \
							"[ovrDefault]i[flags]i[scthresh]f[micout]i[micmatching]i[trimIn]s" \
							"[hint]b[metric]i[batch]b[ubsco]b[mmsco]b[opt]i", Create_TFM, 0);
	env->AddFunction("TDecimate", "c[mode]i[cycleR]i[cycle]i[rate]f[dupThresh]f[vidThresh]f" \
							"[sceneThresh]f[hybrid]i[vidDetect]i[conCycle]i[conCycleTP]i" \
							"[ovr]s[output]s[input]s[tfmIn]s[mkvOut]s[nt]i[blockx]i" \
							"[blocky]i[debug]b[display]b[vfrDec]i[batch]b[tcfv1]b[se]b" \
							"[chroma]b[exPP]b[maxndl]i[m2PA]b[denoise]b[noblend]b[ssd]b" \
							"[hint]b[clip2]c[sdlim]i[opt]i", Create_TDecimate, 0);
    env->AddFunction("MergeHints", "c[hintClip]c[debug]b", Create_MergeHints, 0);
	env->AddFunction("FieldDiff", "c[nt]i[chroma]b[display]b[debug]b[sse]b[opt]i", 
							Create_FieldDiff, 0);
	env->AddFunction("CFieldDiff", "c[nt]i[chroma]b[debug]b[sse]b[opt]i", Create_CFieldDiff, 0);
	env->AddFunction("FrameDiff", "c[mode]i[prevf]b[nt]i[blockx]i[blocky]i[chroma]b[thresh]f" \
							"[display]i[debug]b[norm]b[denoise]b[ssd]b[opt]i", Create_FrameDiff, 0);
	env->AddFunction("CFrameDiff", "c[mode]i[prevf]b[nt]i[blockx]i[blocky]i[chroma]b[debug]b" \
							"[norm]b[denoise]b[ssd]b[rpos]b[opt]i", Create_CFrameDiff, 0);
	env->AddFunction("ShowCombedTIVTC", "c[cthresh]i[chroma]b[MI]i[blockx]i[blocky]i[metric]i" \
							"[debug]b[display]i[fill]b[opt]i", Create_ShowCombedTIVTC, 0);
	env->AddFunction("IsCombedTIVTC", "c[cthresh]i[MI]i[chroma]b[blockx]i[blocky]i[metric]i" \
							"[opt]i", Create_IsCombedTIVTC, 0);
	env->AddFunction("RequestLinear", "c[rlim]i[clim]i[elim]i[rall]b[debug]b", 
							Create_RequestLinear, 0);
	return 0;
}