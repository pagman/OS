OBJS	= main.o pickers.o segment.o validation.o
SOURCE	= main.c pickers.c segment.c validation.c
HEADER	= defines.h pickers.h segment.h semun.h validation.h
OUT	= os
CC	 = gcc
FLAGS	 = -g -c -Wall
LFLAGS	 = -lpthread

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c 

pickers.o: pickers.c
	$(CC) $(FLAGS) pickers.c 

segment.o: segment.c
	$(CC) $(FLAGS) segment.c 

validation.o: validation.c
	$(CC) $(FLAGS) validation.c 


clean:
	rm -f $(OBJS) $(OUT)