SOURCES := FrontPanelManager.c Routing.c ViewportManager.c VirtualTerminal.c tohex.c xprintf.c

LIBRARIES := -lpthread

INCLUDES := ../FrontPanelDriver/front_panel.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -Wall -I../ -I../FrontPanelDriver -c $<

## Targets
TARGET := FrontPanelManager

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -g -O -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)

test:	VirtualTerminal.c VirtualTerminalTest.c xprintf.c
	$(CC) -g -O -Wall -I../ -I../FrontPanelDriver -o VirtualTerminalTest $?
