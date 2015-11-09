# default target
all:

.PHONY: all clean editor

QMAKE ?= qmake

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
	UNAME_S := $(shell uname -s)

	ifeq ($(UNAME_S), Linux)
		CPPFLAGS += -DUSE_GETADDRINFO
		OPENGL_LIBS = -lGL -lGLU
	else ifeq ($(UNAME_S), Darwin)
		CPPFLAGS += -DUSE_GETADDRINFO
		OPENGL_LIBS = -framework OpenGL
	else
		OPENGL_LIBS = -lGL -lGLU
	endif

	SDL_CFLAGS = $(shell sdl-config --cflags)
	SDL_LIBS = $(shell sdl-config --libs)
	LDLIBS += -lm
endif

LIB_OBJS = \
	lib/device.o \
	lib/track.o

all: lib/librocket.a lib/librocket-player.a editor

example_bass/%$X: CPPFLAGS += -Iexample_bass/include
example_bass/%$X: CXXFLAGS += $(SDL_CFLAGS)
example_bass/%$X: LDLIBS += -Lexample_bass/lib -lbass
example_bass/%$X: LDLIBS += $(OPENGL_LIBS) $(SDL_LIBS)

clean:
	$(RM) $(LIB_OBJS) lib/librocket.a lib/librocket-player.a
	$(RM) example_bass/example_bass$X example_bass/example_bass-player$X
	if test -e editor/Makefile; then $(MAKE) -C editor clean; fi;
	$(RM) editor/editor editor/Makefile

lib/librocket.a: $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.player.o : %.c
	$(COMPILE.c) -DSYNC_PLAYER $(OUTPUT_OPTION) $<

lib/librocket-player.a: $(LIB_OBJS:.o=.player.o)
	$(AR) $(ARFLAGS) $@ $^

example_bass/example_bass$X: example_bass/example_bass.cpp lib/librocket.a
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@

example_bass/example_bass-player$X: example_bass/example_bass.cpp lib/librocket-player.a
	$(LINK.cpp) -DSYNC_PLAYER $^ $(LOADLIBES) $(LDLIBS) -o $@

editor/Makefile: editor/editor.pro
	cd editor && $(QMAKE) editor.pro -o Makefile

editor: editor/Makefile
	$(MAKE) -C editor
