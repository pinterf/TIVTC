/*
**                 Retrieve Ranges v1.1 - (January 17, 2006)
**
**   This tool parses a v1 mkv timecodes file and retrieves the frame ranges
**   in the original (undecimated) video stream that correspond to each
**   fps range in the timecode file.  NOTE:  this works by assuming all framerate
**   differences to the original are due only to decimation (removal of frames),
**   if this is not the case then this programs output will be useless.
**   
**   Copyright (C) 2005-2006 Kevin Stone
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

/*
** USAGE INFORMATION -
**
** BIG NOTE:  this program works by assuming framerate differences from the original
**            stream are due only to frame removal/decimation!  So if they are
**            due to something else this will not work correctly.
**
** SYNTAX:  retrieveranges tc.txt original_fps end_frame [out_fps] >outfile.txt
**
** PARAMETERS:  tc.txt       = Timecode file to parse (must be v1 format).
**              original_fps = Rate of original video (e.g. 29.970, 59.940, etc...).
**              end_frame    = Frame to stop range output on. (from # from original file)
**              out_fps      = This is optional, if given, only ranges that match 
**                                this fps will be output instead of all ranges. For 
**                                example if you only want 29.970 ranges or 23.976 ranges.
*/

#include <windows.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	if (argc < 4 || argc > 5)
	{
		printf("Error:  incorrect number of arguments!\n");
		printf("Syntax:  retrieveranges timecodefile.txt original_fps end_frame [out_fps] >outputfile.txt\n");
		printf("Example:  retrieveranges tc.txt 29.970 10000 29.970 >origVideoRanges.txt\n");
		printf("Info:  see the source file for further information.\n");
		printf("Quitting....\n");
		return 1;
	}
	FILE *inFile = fopen(argv[1],"r");
	if (inFile == NULL)
	{
		printf("Error:  unable to open timecodes file (%s)!\n", argv[1]);
		printf("Quitting...\n");
		return 2;
	}
	char line[1025], *p;
	fgets(line, 1024, inFile);
	if (strnicmp(line, "# timecode format v1", 20) != 0)
	{
		fclose(inFile);
		printf("Error:  timecode file is of unsupported format!\n");
		printf("Quitting...\n");
		return 3;
	}
	double rateA;
	while (fgets(line, 1024, inFile) != 0)
	{
		if (strnicmp(line, "assume ", 7) == 0)
		{
			p = line;
			while (*p++ != ' ');
			rateA = atof(p);
			break;
		}
	}
	unsigned lastF = 0, lastC = 0, startF, stopF, temp, temp2, eFrame = atoi(argv[3]);
	double rate, rateB = atof(argv[2]), outfps = argc == 5 ? atof(argv[4]) : -1.0;
	while (fgets(line, 1024, inFile) != 0 && lastC < eFrame)
	{
		if (line[0] < '0' || line[0] > '9') continue;
		sscanf(line,"%u,%u", &startF, &stopF);
		p = line;
		while (*p++ != ',');
		while (*p++ != ',');
		rate = atof(p);
		if (lastF < startF) 
		{
			temp = (int)((startF-1-lastF+1)*(rateB/rateA)+0.5f);
			temp2 = lastC+temp-1;
			if (temp2 > eFrame) temp2 = eFrame;
			if (outfps < 0 || rateA == outfps) printf("%u,%u,%3.3f\n", lastC, temp2, rateA);
			lastC += temp;
			if (lastC >= eFrame) goto jmpend;
		}
		temp = (int)((stopF-startF+1)*(rateB/rate)+0.5f);
		temp2 = lastC+temp-1;
		if (temp2 > eFrame) temp2 = eFrame;
		if (outfps < 0 || rate == outfps) printf("%u,%u,%3.3f\n", lastC, temp2, rate);
		lastC += temp;
		lastF = stopF+1;
	}
	if (lastC < eFrame)
	{
		if (outfps < 0 || rateA == outfps) printf("%u,%u,%3.3f\n", lastC, eFrame, rateA);
	}
jmpend:
	fclose(inFile);
	return 0;
}