

all: native web

native: main.cpp
	g++ main.cpp -Wall -Wextra -Wpedantic -DFFMPEG_ENABLE -Iraylib/include -Lraylib/lib -lraylib -lgdi32 -lwinmm -std=c++20 -o musulizer.exe

web: main.cpp
	emcc -o ./web/musulizer.html main.cpp -Os -Wall ./libraylib.web.a -Iraylib/include -L. -s USE_GLFW=3 -s ASYNCIFY --preload-file resources --shell-file .\shell.html -DPLATFORM_WEB
