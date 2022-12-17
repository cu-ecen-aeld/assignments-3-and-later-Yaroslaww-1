#Makefile for aesdsocket

INCLUDES = ./aesdsocket.c
OUTPUT = ./aesdsocket

CROSS_COMPILE=
CC=$(CROSS_COMPILE)gcc

all: $(INCLUDES)
	$(CC) -Werror -Wall -g $(INCLUDES) -o $(OUTPUT)

clean:
	rm -f $(OUTPUT)
	rm -f *.o
