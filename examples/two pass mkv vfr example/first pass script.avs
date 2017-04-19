LoadPlugin("c:\avsfilters\dgdecode.dll")
LoadPlugin("c:\avsfilters\tivtc.dll")

d2vpath = "c:\vobs\ep1.d2v"

mpeg2source(d2vpath)
tfm(d2v=d2vpath,output="c:\vobs\ep1-tfm.txt")
tdecimate(mode=4,output="c:\vobs\ep1-tdec.txt")

# this next line assumes the source is dvd 720x480
# and crops it down to 32x32 to save processing time
# since we're only interested in letting tfm/tdecimate
# gather info and not the actual result

crop(344,224,-344,-224)