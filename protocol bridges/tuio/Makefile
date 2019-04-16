
CC=gcc
CFLAGS=-c -Wall
SOURCES=tuio_mapper.c
OBJECTS=$(SRC:%.c=%.o)
LDLIBS=-L/usr/local/lib -lmapper -llo -I/usr/local/include/lo -I/usr/local/include/mapper
EXECUTABLE=tuio_mapper

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(SOURCES) $(LDLIBS) $(OBJECTS) -o $@

clean:
	rm -rf *.o tuio_mapper