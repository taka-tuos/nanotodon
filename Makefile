TARGET		= nanotodon
OBJS_TARGET	= nanotodon.o config.o messages.o

CFLAGS = -g
# optimization
#CFLAGS+= -O2
# Use $XDG_CONFIG_HOME or ~/.config dir to save config files
CFLAGS+= -DSUPPORT_XDG_BASE_DIR

LDFLAGS = 
ifeq ($(shell uname -s),Darwin)
	LIBS = -lcurl -ljson-c -lncurses -lpthread -lm
else
	LIBS = -lcurl -ljson-c -lncursesw -lpthread -lm
endif

# for pkgsrc build
#CFLAGS+= -I/usr/pkg/include -I/usr/pkg/include/ncursesw -DNCURSES_WIDECHAR
#LDFLAGS+= -L/usr/pkg/lib -Wl,-R/usr/pkg/lib

include Makefile.in
