CXXFLAGS=-g -MMD -std=c++17
SRCS=eval.cpp lang.cpp
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

all: eval
eval: $(OBJS)
	g++ -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) eval

-include *.d
