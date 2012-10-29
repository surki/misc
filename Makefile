EXECUTABLE= memleak_test
SRCS=$(wildcard *.cpp)
HEADERS=$(wildcard *.h)
OBJS=$(SRCS:.cpp=.o)
CPPFLAGS=-g -O0
CC= g++

all: memleak_test

%.o: %.cpp
	$(CC) -c -g -o $@ $<

clean:
	rm -f $(EXECUTABLE)
	rm -f $(OBJS)

$(EXECUTABLE): $(OBJS) $(HEADERS)
	$(CC) -g -o $@ $(OBJS) -lm -lGL -lX11 -lXinerama
