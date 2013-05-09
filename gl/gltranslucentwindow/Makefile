EXECUTABLE= gltranslucentwindow
SRCS=$(wildcard *.cpp)
HEADERS=$(wildcard *.h)
OBJS=$(SRCS:.cpp=.o)
CPPFLAGS=-g -O0
CC= g++

all: gltranslucentwindow

%.o: %.cpp $(HEADERS)
	$(CC) -c -g -o $@ $<

clean:
	rm -f $(EXECUTABLE)
	rm -f $(OBJS)

$(EXECUTABLE): $(OBJS)
	$(CC) -g -o $@ $(OBJS) -lm -lGL -lX11 -lXrender
