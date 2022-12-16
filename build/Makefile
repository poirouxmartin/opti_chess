

run:
	# Windows
	# g++ ../*.cpp -o opti_chess.exe -Ofast -static -I ../include/ -L ../lib/ -lraylib -lpthread -lopengl32 -lgdi32 -lwinmm -march=native -std=c++20
	# g++ ../*.cpp -o opti_chess.exe -O3 -flto -static -I ../include/ -L ../lib/ -lraylib -lpthread -lopengl32 -lgdi32 -lwinmm -march=native -mtune=native -std=c++20
	g++ ../*.cpp -o opti_chess.exe -Ofast -flto -static -I ../include/ -L ../lib/ -lraylib -lpthread -lopengl32 -lgdi32 -lwinmm -march=native -mtune=native -march=skylake -std=c++20 -fopenmp -mavx2 -fstrict-aliasing
	# -fopenmp -march=skylake -mavx2 -funroll-loops -fstrict-aliasing


	./opti_chess

	# Linux after installing raylib
	# g++ ../*.cpp -Wall -o opti_chess -Ofast -march=native -std=c++2a -lraylib -lpthread -ldl
	#./opti_chess

build-release:
	# Linux after installing raylib
	cd build
	g++ *.cpp -Wall -o build/opti_chess -Ofast -march=native -std=c++2a -lraylib -lpthread -ldl

build-debug:
	# Linux after installing raylib
	cd build
	g++ *.cpp -Wall -g -o build/opti_chess -march=native -std=c++2a -lraylib -lpthread -ldl

all: build-release run
