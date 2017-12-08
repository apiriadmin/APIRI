SOURCES := fio.c

LIBRARIES := 

INCLUDES :=

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) $(EXTRA_CFLAGS) -g -O2 -fPIC -Wall -c $<

## Targets
TARGET := libfio.so.1.0

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) $(EXTRA_CFLAGS) -shared -Wl,-soname,libfio.so.1.0 -fPIC -O2 -o $@ $(OBJECTS) $(LIBRARIES)
	ln -sf $@ libfio.so.1
	ln -sf $@ libfio.so

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
