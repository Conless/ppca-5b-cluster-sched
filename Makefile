CXXFLAGS=-g -MMD -std=c++17
BINS=eval cheat anticheat checkans normalize \
    cheat-baseline1-spin-and-swap \
	cheat-baseline1-dead-code
SRCS=lang.cpp $(BINS:=.cpp)
SUBMITFILES=$(BINS:=-submit.cpp)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

all: $(BINS) $(SUBMITFILES)
$(BINS): %: %.o lang.o
	$(CXX) -o $@ $^

$(SUBMITFILES): %-submit.cpp: %.cpp %
	rm -f $@
	echo '#include <bits/stdc++.h>' > $@
	cpp -imacros bits/stdc++.h -include lang.cpp $< | sed -E 's/^#.+$$//;/^\s*$$/d' >> $@

.PHONY: clean
clean:
	rm -f $(BINS) $(OBJS) $(DEPS) $(SUBMITFILES)

-include *.d
