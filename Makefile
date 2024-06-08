TARGET = nanotodon
OBJS = nanotodon.o sbuf.o squeue.o utils.o config.o messages.o

# Use $XDG_CONFIG_HOME or ~/.config dir to save config files
CFLAGS += -g -DSUPPORT_XDG_BASE_DIR -D_XOPEN_SOURCE -D_DEFAULT_SOURCE
LDLIBS += -lcurl -lpthread -lm

# default
default : $(TARGET)

# rules
$(TARGET) : $(OBJS) Makefile
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LDLIBS) 
	
# commands
clean :
	-rm -f *.o $(TARGET)
