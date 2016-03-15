SOURCES := atcapi-test.c

FIO_DIR := ../fio/src/fioapi
FPU_DIR := ../fpu/FrontPanelSystem/libfpui
TOD_DIR := ../tod/src

LIBRARIES := -L$(FIO_DIR) -L$(FPU_DIR) -L$(TOD_DIR) -lfio -lfpui -ltod

OBJECTS := $(SOURCES:.c=.o)

.c.o:
	$(CC) -g -O -Wall -I../ -I$(FIO_DIR) -I$(FPU_DIR) -I$(TOD_DIR) -c $<

## Targets
TARGET := atcapi-test

all: $(TARGET)

$(TARGET) : $(OBJECTS) $(INCLUDES)
	$(CC) -g -O2 -o $@ $(OBJECTS) $(LIBRARIES)

install:
	cp $(TARGET) $(HOME)/bin
	chmod 755 $(HOME)/bin/$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(HOME)/bin/$(TARGET)
