SOURCES := libfpui.c

LIBRARIES := 

INCLUDES := ./fpui.h ../FrontPanelDriver/front_panel.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -fPIC -Wall -I../ -I../FrontPanelDriver -c $<

## Targets
TARGET := libfpui.so.1.0

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -shared -Wl,-soname,libfpui.so.1 -fPIC -O -o $@ $(OBJECTS) $(LIBRARIES)
	ln -s $@ libfpui.so.1
	ln -s $@ libfpui.so

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
