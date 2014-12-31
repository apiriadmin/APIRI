.PHONY: all
all:

CPPFLAGS += -Os

%.o: %.c
	$(COMPILE.c) -I../libfpui/ -o '$@' '$<'

fpuiexec: fpuiexec.o
	$(LINK.c) $^ ../libfpui/libfpui.so -o $@
fpuiexec.o: fpuiexec.c ../libfpui/fpui.h

all: fpuiexec

clean:
	rm -f *.o