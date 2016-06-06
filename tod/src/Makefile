SOURCES := tod.c

LIBRARIES := 

INCLUDES := -I. -I../../include 

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O2 -fPIC -Wall $(INCLUDES) -c $<

## Targets
TARGET := libtod.so

all: $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) -shared -fPIC -O2 $(INCLUDES) $(LIBRARIES) -o $@ $(OBJECTS)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
