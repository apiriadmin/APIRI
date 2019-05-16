SOURCES := fioapi_test.c

LIBRARIES := -L../fioman/RPC -lfio

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -O2 -Wall -std=gnu99 -I../ -I../fioapi -c $<

## Targets
TARGET := fioapi_test

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -O2 -std=gnu99 -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
