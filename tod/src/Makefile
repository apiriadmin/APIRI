SOURCES := tod.c

LIBRARIES := 

INCLUDES := ./tod.h ./atc.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -fPIC -Wall -c $<

## Targets
TARGET := libtod.so

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -shared -fPIC -O -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
