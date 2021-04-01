CC=gcc
CFLAGS=-Wall -Wextra -Werror -g
LIBS=-lX11 -lGL
SOURCE=source

makima: $(SOURCE)/makima.c $(SOURCE)/makima.h
	$(CC) $(CFLAGS) $(LIBS) $(SOURCE)/makima.c $(SOURCE)/makima.h -o makima
