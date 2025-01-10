INCS = -Iinclude -Iexternal/bits/include -Iexternal/bits/external/essentials/include -I.
CXXFLAGS = -std=c++17 -Wall -Wextra -Wno-missing-braces -Wno-unused-function -march=native -mbmi2 -msse4.2 -g -fPIC
CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
DESTDIR =
PREFIX ?= /usr/local
ALL_H = $(shell find external include -name \*.hpp)
.PHONY: all test bench lint clean check-syntax
all: example-inc example-c example pthash
test: example-inc
	./example-inc

pthash-example.hpp: example-c
	./example-c
example-inc: src/example-inc.cpp pthash-example.hpp $(ALL_H)
	$(CXX) $(CXXFLAGS) -O0 -o $@ $(INCS) src/example-inc.cpp
example-c: src/example-c.cpp $(ALL_H)
	$(CXX) $(CXXFLAGS) -O0 -o $@ $(INCS) src/example-c.cpp
example: src/example.cpp $(ALL_H)
	$(CXX) $(CXXFLAGS) -O1 -o $@ $(INCS) src/example.cpp
pthash: src/build.cpp $(ALL_H)
	$(CXX) $(CXXFLAGS) -O3 -o $@ $(INCS) src/build.cpp

bench: pthash

lint:
	cppcheck include/pthash.hpp include/pthash-static.hpp src/*.cpp

# emacs flymake-mode
check-syntax:
	test -n "$(CHK_SOURCES)" && \
	  nice $(CXX) $(INCS) -O0 -o /dev/null -S $(CHK_SOURCES)

pthash.1: pthash pthash.1.inc Makefile
	help2man --no-discard-stderr -s 1 -N -o $@ -i pthash.1.inc ./pthash

TAGS: $(ALL_H)
	etags $(ALL_H)

install: pthash pthash.1
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp pthash $(DESTDIR)$(PREFIX)/bin/
	mkdir -p $(DESTDIR)$(PREFIX)/include/pthash
	rsync -av $(ALL_H) $(DESTDIR)$(PREFIX)/include/pthash/
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/pthash
	cp README.md $(DESTDIR)$(PREFIX)/share/doc/pthash/
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp pthash.1 $(DESTDIR)$(PREFIX)/share/man/man1/

clean:
	-rm -f pthash-example.hpp example-inc example-c example pthash pthash-example.bin
