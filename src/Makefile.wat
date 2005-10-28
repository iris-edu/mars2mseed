#
# Wmake File for mars2mseed - For Watcom's wmake
# Use 'wmake -f Makefile.wat'

.BEFORE
	@set INCLUDE=.;$(%watcom)\H;$(%watcom)\H\NT
	@set LIB=.;$(%watcom)\LIB386

cc     = wcc386
cflags = -zq
lflags = OPT quiet OPT map LIBRARY ..\libmseed\libmseed.lib
cvars  = $+$(cvars)$- -DWIN32

BIN = ..\mars2mseed.exe

INCS = -I..\libmseed

all: $(BIN)

$(BIN):	mars2mseed.obj marsio.obj
	wlink $(lflags) name $(BIN) file {mars2mseed.obj marsio.obj}

# Source dependencies:
mars2mseed.obj:	mars2mseed.c marsio.h
marsio.obj:	marsio.c marsio.h

# How to compile sources:
.c.obj:
	$(cc) $(cflags) $(cvars) $(INCS) $[@ -fo=$@

# Clean-up directives:
clean:	.SYMBOLIC
	del *.obj *.map $(BIN)
