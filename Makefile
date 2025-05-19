


all: build run

build: main.cpp
# g++ main.cpp -Wall -Wextra -Wpedantic -Iraylib/include -Lraylib/lib -lraylib -lgdi32 -lwinmm -o musulizer.exe
	g++ main.cpp -Iraylib/include -Lraylib/lib -lraylib -lgdi32 -lwinmm -o musulizer.exe

run: musulizer.exe
	.\musulizer.exe

clean:
	del /Q musulizer.exe 2>nul || exit 0