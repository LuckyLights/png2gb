CC = /usr/bin/g++

all: 
	$(CC) main.cpp -I . -O3 -o ./build/png2gb

install: all
	cp ./build/png2gb /usr/local/bin/png2gb