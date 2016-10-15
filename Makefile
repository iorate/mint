CXX=g++
CXXFLAGS=-std=c++14 -O2 -static -mwindows -municode
LDFLAGS=-lboost_filesystem-mt -lboost_system-mt
DESTDIR?=/
PROGRAM?=mint.exe

all: $(PROGRAM)

$(PROGRAM): mint.cpp
	$(CXX) $(CXXFLAGS) -o $(PROGRAM) $^ $(LDFLAGS)

install: $(PROGRAM)
	install -s $(PROGRAM) $(DESTDIR)

clean:
	rm -f $(PROGRAM)
