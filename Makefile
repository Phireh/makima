CC=gcc
CFLAGS=-Wall -Wextra -Werror -g
LIBS=-lX11 -lGL
SOURCE=source

makima: $(SOURCE)/makima.c $(SOURCE)/makima.h
	$(CC) $(CFLAGS) $(SOURCE)/makima.c $(SOURCE)/makima.h $(LIBS) -o makima
