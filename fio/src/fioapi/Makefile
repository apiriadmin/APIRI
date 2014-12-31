SOURCES := fio.c

LIBRARIES := 

INCLUDES :=

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O2 -fPIC -Wall -c $<

## Targets
TARGET := libfio.so

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -shared -fPIC -O2 -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
