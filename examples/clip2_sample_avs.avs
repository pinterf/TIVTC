LoadPlugin("c:\avsfilters\dgdecode.dll")
LoadPlugin("c:\avsfilters\tivtc.dll")

d2vpath = "c:\mysource.d2v"

mpeg2source(d2vpath)             #assume tff source
deint = last.bob().selecteven()  #simple cubic deinterlacing (keep top field)
tfm(PP=5,clip2=deint)            #motion masked post-processing since PP > 4