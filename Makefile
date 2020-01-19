TARGET		= nanotodon
OBJS_TARGET	= nanotodon.o config.o

CFLAGS = -g -DSUPPORT_XDG_BASE_DIR
LDFLAGS = 
LIBS = -lcurl -ljson-c -lncursesw -lpthread -lm 

include Makefile.in
