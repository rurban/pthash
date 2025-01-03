INCS = -Iinclude -Iexternal/essentials/include -I. -Iexternal/xxHash
example-inc: src/example-inc.cpp pthash-example.hpp external/essentials/include/essentials.hpp
	$(CXX) -g -o$@ $(INCS) src/example-inc.cpp

# emacs flymake-mode
check-syntax:
	test -n "$(CHK_SOURCES)" && \
	  nice $(CXX) $(INCS) -O0 -o /dev/null -S $(CHK_SOURCES)
.PHONY: check-syntax
