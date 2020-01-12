TARGET		= nanotodon
OBJS_TARGET	= nanotodon.o config.o

CFLAGS = -g
LDFLAGS = 
LIBS = -lc -lm -lcurl -ljson-c -lncursesw -lpthread

include Makefile.in
