OUT=group
FLAGS=-std=c++0x -g -o ${OUT}
IN=group.cpp
all:
	g++ ${FLAGS} -D_DEBUG ${IN}
release:
	g++ ${FLAGS} -Wall -Wextra ${IN}
clean:
	rm ${OUT}
