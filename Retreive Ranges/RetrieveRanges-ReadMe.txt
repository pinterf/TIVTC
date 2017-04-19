
Retrive Ranges - ReadMe (02/19/2005)

RetrieveRanges is a commandline program that takes a mkv v1 timecode file as input and
outputs the ranges in the original video stream that matched each fps in the final stream.
This means you can use tfm/tdecimate to identify film and video sections in a file
by using them in mkv vfr timecode output mode and then using the timecode file as input
to this program.

Usage:


  Syntax =>  retrieveranges timecodefile.txt orig_fps end_frame [out_fps] >outfile.txt


  Examples =>  retrieveranges tc.txt 29.970 12000 >ranges.txt

               retrieveranges tc.txt 29.970 12000 29.970 >ranges.txt

        The first example would output all ranges, while the second would output only
        the 29.970 fps ranges.


  Parameters:

     orig_fps = fps of original video

     end_frame = just set this equal to the # of frames in the original undecimated stream

     out_fps = optional, if set it will only output ranges that match this fps

     >outfile.txt = dumps the std output into outfile.txt