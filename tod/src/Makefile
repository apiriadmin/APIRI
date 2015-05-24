SOURCES := tod.c

LIBRARIES := 

INCLUDES := ./tod.h ./atc.h

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -fPIC -Wall -c $<

## Targets
TARGET := libtod.so.1.0

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -shared -Wl,-soname,libtod.so.1 -fPIC -O -o $@ $(OBJECTS) $(LIBRARIES)
	ln -s $@ libtod.so.1
	ln -s $@ libtod.so

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
