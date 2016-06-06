SOURCES := timesrc.c

LIBRARIES :=

INCLUDES := -I. -I../../include

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -O2 -Wall -c $(INCLUDES) $<

## Targets
TARGET := timesrc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -O2 $(INCLUDES) $(LIBRARIES)-o $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(TARGET)
