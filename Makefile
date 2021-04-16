CC=gcc
CFLAGS=-Wall -Wextra -Werror -g
LIBS=-lX11 -lGL
SOURCE=source

makima: $(SOURCE)/makima.c $(SOURCE)/makima.h $(SOURCE)/makima_input.h
	$(CC) $(CFLAGS) $(SOURCE)/makima.c $(LIBS) -o makima
