LoadPlugin("c:\avsfilters\dgdecode.dll")
LoadPlugin("c:\avsfilters\tivtc.dll")
#load other needed plugins for filtering

d2vpath = "c:\vobs\ep1.d2v"

mpeg2source(d2vpath)
tfm(d2v=d2vpath,input="c:\vobs\ep1-tfm.txt")

# If your source is not anime or cartoon then add
# vfrDec=0  into the line below
tdecimate(mode=5,hybrid=2,input="c:\vobs\ep1-tdec.txt",tfmIn="c:\vobs\ep1-tfm.txt",mkvOut="c:\vobs\ep1-timecodes.txt")

# add other processing below, such as filtering
# cropping, resizing, color correction, etc...

