SRCS=main.cpp
OBJS=$(SRCS:.cpp=.o)
OUTPUT=encode
CXXFLAGS=-I. -O2 -Wall -std=c++11
LDFLAGS=
LIBS=-lavformat -lavcodec -lavutil -lswscale
CXX=g++

all: $(OUTPUT)

clean:
	rm -f $(OUTPUT) $(OBJS) test.mov

$(OUTPUT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(OUTPUT) $(OBJS) $(LIBS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<
