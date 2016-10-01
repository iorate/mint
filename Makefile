all: mint.exe

mint.exe: mint.cpp
	g++ -O2 -static -mwindows -o $@ $^ -lboost_program_options-mt

clean:
	rm -f mint.exe
