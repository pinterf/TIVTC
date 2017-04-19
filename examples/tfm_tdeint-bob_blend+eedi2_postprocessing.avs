LoadPlugin("c:\avsfilters\dgdecode.dll")
LoadPlugin("c:\avsfilters\tivtc.dll")
LoadPlugin("c:\avsfilters\tdeint.dll")
LoadPlugin("c:\avsfilters\eedi2.dll")

d2vpath = "c:\vobs\ep1.d2v"

mpeg2source(d2vpath)
interp = separatefields().eedi2(field=-2)
deint = tdeint(mode=2,edeint=interp)
tfm(d2v=d2vpath,clip2=deint)

# If you are using a source that doesn't correctly set the field order
# (mpeg2source() does correctly set it), then you will need to determine
# the field order yourself and either add an assumetff() or assumebff()
# after the source is loaded or specify the "order" parameter for tdeint/tfm
# and the field parameter for eedi2.