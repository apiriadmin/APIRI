SOURCES := atc_sc.c crc_ccitt.c

LIBRARIES := -L../libfpui -L../../../fio/src/fioapi -L../../../tod/src \
		-lfpui -lfio -ltod -lpthread

INCLUDES := ../FrontPanelDriver/front_panel.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -Wall -I../ -I../libfpui -I../../../fio/src/fioapi \
	-I../../../tod/src -c $<

## Targets
TARGET := atc_sc

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -g -O -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
