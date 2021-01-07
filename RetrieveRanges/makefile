#linux/gcc makefile sample
OUTPUTFILE=RetrieveRanges
INSTALLDIR=binaries

# Default target
.PHONY: all
all: $(OUTPUTFILE)

# Build RetrieveRanges from RetrieveRanges .cpp
$(OUTPUTFILE): RetrieveRanges.cpp
	g++ -o RetrieveRanges RetrieveRanges.cpp

# Copy to the binaries subdirectory
.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(OUTPUTFILE) $(INSTALLDIR)

# Delete
.PHONY: clean 
clean:
	rm -f $(OUTPUTFILE)