


all: build run

build: main.cpp
	g++ main.cpp -Wall -Wextra -Wpedantic -Iraylib/include -Lraylib/lib -lraylib -lgdi32 -lwinmm -std=c++20 -o musulizer.exe

run: musulizer.exe
	.\musulizer.exe

clean:
	del /Q musulizer.exe 2>nul || exit 0