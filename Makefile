TARGET		= nanotodon
OBJS_TARGET	= nanotodon.o config.o

CFLAGS = -g -DSUPPORT_XDG_BASE_DIR
LDFLAGS = 
LIBS = -lc -lm -lcurl -ljson-c -lncursesw -lpthread

include Makefile.in
