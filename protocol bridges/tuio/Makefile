
CC=g++
CFLAGS=-c -Wall
CPPFLAGS=-std=c++11
SOURCES=tuio_mapper.cpp
OBJECTS=$(SRC:%.c=%.o)
LDLIBS=-L/usr/local/lib -lmpr -llo -I/usr/local/include/lo -I/usr/local/include/mpr
EXECUTABLE=tuio_mapper

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CPPFLAGS) $(SOURCES) $(LDLIBS) $(OBJECTS) -o $@

clean:
	rm -rf *.o tuio_mapper