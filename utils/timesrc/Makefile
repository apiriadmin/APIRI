SOURCES := timesrc.c

LIBRARIES := -latc -L../../lib

INCLUDES := -I../../include

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -O2 -Wall -c $(INCLUDES) $<

## Targets
TARGET := timesrc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -O2 -o $@ $(OBJECTS) $(LIBRARIES)

clean:
	rm -f $(OBJECTS) $(TARGET)
