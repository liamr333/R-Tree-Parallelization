CC = gcc
CFLAGS = -Wall -g
LIBS = -lm -pthread

clean:
        rm -f main

main:
        $(CC) $(CFLAGS) main.c -o main $(LIBS)
