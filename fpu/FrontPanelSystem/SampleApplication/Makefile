SOURCES := SampleApplication.c

LIBRARIES := -L../libfpui -lpthread -lfpui

INCLUDES := ../FrontPanelDriver/front_panel.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -Wall -I../ -I../FrontPanelDriver -I../libfpui -c $<

## Targets
TARGET := SampleApplication

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -g -O -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
