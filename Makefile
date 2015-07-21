all: dig yell listen

dig: dig.c
	gcc -o dig dig.c $(shell pkg-config --cflags --libs libmongoc-1.0)

yell: yell.c
	gcc -o yell yell.c $(shell pkg-config --cflags --libs libmongoc-1.0)

listen: listen.c
	gcc -o listen listen.c $(shell pkg-config --cflags --libs libmongoc-1.0)