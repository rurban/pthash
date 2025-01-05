INCS = -Iinclude -Iexternal/essentials/include -I. -Iexternal/xxHash
CXXFLAGS = -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fPIC
ALL_H = $(shell find include -name \*.hpp)
.PHONY: test clean check-syntax
test: example-inc
	./example-inc

pthash-example.hpp: example-c
	./example-c
example-inc: src/example-inc.cpp pthash-example.hpp $(ALL_H) external/essentials/include/essentials.hpp
	$(CXX) $(CXXFLAGS) -o$@ $(INCS) src/example-inc.cpp
example-c: src/example-c.cpp $(ALL_H) external/essentials/include/essentials.hpp
	$(CXX) $(CXXFLAGS) -o$@ $(INCS) src/example-c.cpp

# emacs flymake-mode
check-syntax:
	test -n "$(CHK_SOURCES)" && \
	  nice $(CXX) $(INCS) -O0 -o /dev/null -S $(CHK_SOURCES)
.PHONY: check-syntax

clean:
	-rm -f example-inc
