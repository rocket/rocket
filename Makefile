# default target
all:

# default build flags
CFLAGS = -g -O2 -Wall

# user-defined config file (if available)
-include config.mak

SYNC_OBJS = \
	sync/data.o \
	sync/device.o \
	sync/track.o

all: lib/librocket.a

clean:
	$(RM) -rf $(SYNC_OBJS) lib

lib/librocket.a: $(SYNC_OBJS)
	@mkdir -p lib
	$(AR) $(ARFLAGS) $@ $<
