CC = gcc


all: main.c
	$(CC) main.c -lpthread -o run -Wall

clean:
	rm -f *.o run