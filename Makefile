
# mint
#
# Copyright iorate 2016-2017.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CXX := g++
CXXFLAGS := -O2 -std=c++14 -Wall -Wextra -Wno-missing-field-initializers -pedantic-errors -Inonsugar -static -municode -mwindows
LDFLAGS := -lboost_filesystem-mt -lboost_system-mt
DESTDIR := /
PROGRAM := mint.exe

all: $(PROGRAM)

$(PROGRAM): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

install: $(PROGRAM)
	install -s $(PROGRAM) $(DESTDIR)

clean:
	rm -f $(PROGRAM)
