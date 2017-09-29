PREFIX   = /usr/local/pilot/bin/
CC       = $(PREFIX)m68k-palmos-g++
STRIP    = $(PREFIX)m68k-palmos-strip --strip-unneeded
PILRC    = pilrc
BUILDPRC = $(PREFIX)build-prc
PRC2BIN  = $(PREFIX)prc2bin
OBJS     = score.o fw.o
EXEC     = score
CCFLAGS = -Os -Wall -palmos3.5 -fno-exceptions -fno-rtti

$(EXEC).prc: $(EXEC) $(EXEC).def bin.stamp
	$(BUILDPRC) $(EXEC).def $(EXEC) *.bin

$(EXEC): $(OBJS)
	$(CC) $(OBJS) $(CCFLAGS) -o $(EXEC) -lnoexcept
	$(STRIP) $(EXEC)

bin.stamp: $(EXEC).rcp
	$(PILRC) $(EXEC).rcp

# compile rules
.SUFFIXES: .cc .o

.cc.o:
	$(CC) -c $(CCFLAGS) $<

clean:
	rm -f *.[oa] *.bin *.grc *~ $(EXEC)

distclean:
	rm -f *.[oa] *.bin *.[pg]rc *~ $(EXEC)

