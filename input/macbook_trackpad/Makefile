
CC=gcc
CFLAGS=-c -Wall
SOURCES=macbook_trackpad_mapper.c
OBJECTS=$(SRC:%.c=%.o)
LDLIBS=-L/usr/local/lib -lmapper -I/usr/local/include/mapper -F/System/Library/PrivateFrameworks -framework MultitouchSupport
EXECUTABLE=macbook_trackpad_mapper

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(SOURCES) $(LDLIBS) $(OBJECTS) -o $@

clean:
	rm -rf *.o macbook_trackpad_mapper