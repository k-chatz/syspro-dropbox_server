OBJS	= main.o list.o
SOURCE	= main.c list.c
HEADER	= list.h
OUT	= dropbox_server
CC	 = gcc
FLAGS	 = -c -Wall

all: $(OBJS)
	$(CC) $(OBJS) -o $(OUT)

main.o: main.c
	$(CC) $(FLAGS) main.c

list.o: list.c
	$(CC) $(FLAGS) list.c

clean:
	rm -f $(OBJS) $(OUT)
