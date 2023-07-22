CXXFLAGS=-g -MMD -std=c++17
BINS=eval cheat anticheat
SRCS=lang.cpp $(BINS:=.cpp)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

all: $(BINS)
$(BINS): %: %.o lang.o
	g++ -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) eval

-include *.d
