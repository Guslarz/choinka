CC = gcc


all: main.c
	$(CC) main.c -lpthread -o run

clean:
	rm -f *.o run