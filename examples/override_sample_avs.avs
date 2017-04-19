LoadPlugin("c:\avsfilters\dgdecode.dll")
LoadPlugin("c:\avsfilters\tivtc.dll")

d2vpath = "c:\mysource.d2v"

mpeg2source(d2vpath)
tfm(d2v=d2vpath,ovr="TFM-example_ovr_file.txt")

# replace "TFM-example_ovr_file.txt" with the name of your
# overrides file