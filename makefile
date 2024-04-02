PROGRAMS = main
CC = gcc
CFLAGS = -Wall -g
LIBS = -lm -pthread

all: $(PROGRAMS)

clean:
	rm -f *.o


math_utils.o: math_utils.c math_utils.h
	$(CC) $(CFLAGS) -c math_utils.c
	

r_tree.o: r_tree.c r_tree.h math_utils.h
	$(CC) $(CFLAGS) -c r_tree.c

choose_leaf.o: choose_leaf.c choose_leaf.h r_tree.h
	$(CC) $(CFLAGS) -c choose_leaf.c

pick_seeds.o: pick_seeds.c pick_seeds.h r_tree.h
	$(CC) $(CFLAGS) -c pick_seeds.c

main.o: main.c r_tree.h choose_leaf.h
	$(CC) $(CFLAGS) -c main.c


main: r_tree.o choose_leaf.o pick_seeds.o main.o math_utils.o
	$(CC) $(CFLAGS) -o main main.o choose_leaf.o pick_seeds.o r_tree.o math_utils.o $(LIBS)

