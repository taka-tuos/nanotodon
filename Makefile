TARGET		= nanotodon
OBJS_TARGET	= nanotodon.o

CFLAGS = -g
LDFLAGS = 
LIBS = -lcurl -ljson-c -lncursesw -lpthread -lm 

include Makefile.in
