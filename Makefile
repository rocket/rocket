# default target
all:

.PHONY: all clean

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

LIB_OBJS = \
	lib/data.o \
	lib/device.o \
	lib/track.o

all: lib/librocket.a

example_bass/example_bass$X: CPPFLAGS += -Iexample_bass/include
example_bass/example_bass$X: CXXFLAGS += $(SDL_CFLAGS)
example_bass/example_bass$X: LDLIBS += -Lexample_bass/lib -lbass
example_bass/example_bass$X: LDLIBS += $(OPENGL_LIBS) $(SDL_LIBS)

clean:
	$(RM) $(LIB_OBJS) lib/librocket.a

lib/librocket.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

example_bass/example_bass$X: example_bass/example_bass.cpp lib/librocket.a
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
