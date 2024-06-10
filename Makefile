TARGET = nanotodon
OBJS = nanotodon.o sbuf.o squeue.o utils.o config.o messages.o

# Use $XDG_CONFIG_HOME or ~/.config dir to save config files
CFLAGS += -DSUPPORT_XDG_BASE_DIR -D_GNU_SOURCE -D_BSD_SOURCE -D_NETBSD_SOURCE
LDLIBS += -lcurl -lpthread -lm

# for pkgsrc 
#CFLAGS += -I/usr/pkg/include
#LDFLAGS += -L/usr/pkg/lib -Wl,-R/usr/pkg/lib

# default
default : $(TARGET)

# rules
$(TARGET) : $(OBJS) Makefile
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LDLIBS) 
	
# commands
clean :
	-rm -f *.o $(TARGET)
