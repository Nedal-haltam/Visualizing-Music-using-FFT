

.PHONY: all build run clean

all: build run

build: main.c
	gcc main.c -Wall -Wextra -Wpedantic -Iraylib/include -Lraylib/lib -lraylib -lgdi32 -lwinmm -o musulizer.exe
	
run: musulizer.exe
	.\musulizer.exe

clean:
	del /Q musulizer.exe 2>nul || exit 0