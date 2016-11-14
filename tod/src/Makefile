SOURCES := tod.c

LIBRARIES := 

INCLUDES := -I. -I../../include 

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O2 -fPIC -Wall $(INCLUDES) -c $<

## Targets
TARGET := libtod.so.1.0

all: $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) -shared -Wl,-soname,libtod.so.1 -fPIC -O2 $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS)
	ln -s $@ libtod.so.1
	ln -s $@ libtod.so

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
