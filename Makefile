# default target
all:

# default build flags
CFLAGS = -g -O2 -Wall

# user-defined config file (if available)
-include config.mak

ifdef COMSPEC
	X = .exe
	OPENGL_LIBS = -lopengl32 -lglu32
	SDL_LIBS = -lSDL
	LDLIBS += -lws2_32
else
	OPENGL_LIBS = -lGL -lGLU
	SDL_CFLAGS = $(shell sdl-config --cflags)
	SDL_LIBS = $(shell sdl-config --libs)
	LDLIBS += -lm
endif

SYNC_OBJS = \
	sync/data.o \
	sync/device.o \
	sync/track.o

all: lib/librocket.a

bin/example_bass$X: CPPFLAGS += -Iexample_bass/include
bin/example_bass$X: CXXFLAGS += $(SDL_CFLAGS)
bin/example_bass$X: LDLIBS += -Lexample_bass/lib -lbass
bin/example_bass$X: LDLIBS += $(OPENGL_LIBS) $(SDL_LIBS)

clean:
	$(RM) -rf $(SYNC_OBJS) lib bin

lib/librocket.a: $(SYNC_OBJS)
	@mkdir -p lib
	$(AR) $(ARFLAGS) $@ $^

bin/example_bass$X: example_bass/example_bass.cpp lib/librocket.a
	@mkdir -p bin
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
