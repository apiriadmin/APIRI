SOURCES := fio.c

LIBRARIES := 

INCLUDES :=

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O2 -fPIC -Wall -c $<

## Targets
TARGET := libfio.so.1.0

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -shared -Wl,-soname,libfio.so.1 -fPIC -O2 -o $@ $(OBJECTS) $(LIBRARIES)
	ln -s $@ libfio.so.1
	ln -s $@ libfio.so

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
