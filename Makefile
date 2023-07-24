CXXFLAGS=-g -MMD -std=c++17
BINS=eval cheat anticheat
SRCS=lang.cpp $(BINS:=.cpp)
SUBMITFILES=$(SRCS:.cpp=-submit.cpp)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(SRCS:.cpp=.d)

all: $(BINS)
$(BINS): %: %.o lang.o
	g++ -o $@ $^

$(SUBMITFILES): %-submit.cpp: %.cpp
	rm -f $@
	echo '#include <bits/stdc++.h>' > $@
	cpp -imacros bits/stdc++.h -include lang.cpp $< | sed -E 's/^#.+$$//;/^\s*$$/d' >> $@

.PHONY: clean
clean:
	rm -f $(BINS) $(OBJS) $(DEPS) $(SUBMITFILES) eval

-include *.d
