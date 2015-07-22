all: dig yell listen
	mkdir -p bin

dig: dig.c
	gcc -o bin/dig dig.c $(shell pkg-config --cflags --libs libmongoc-1.0)

yell: yell.c
	gcc -o bin/yell yell.c $(shell pkg-config --cflags --libs libmongoc-1.0)

listen: listen.c
	gcc -o bin/listen listen.c $(shell pkg-config --cflags --libs libmongoc-1.0)