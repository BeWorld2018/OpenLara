
CC = ppc-morphos-gcc-11
CXX = ppc-morphos-g++-11

AMIGADATE = $(shell date +"%-d.%-m.%Y")

TARGET = openlara

SDL_CFLAGS := $(shell sdl2-config --cflags) -I/gg/usr/local/include
SDL_LDFLAGS := $(shell sdl2-config --libs)

CFLAGS+= -O2 -DNDEBUG -ffast-math -fomit-frame-pointer -fsigned-char -I../../ -Wno-unused-variable -D__SDL2__ $(SDL_CFLAGS) -Wall
CXXFLAGS+= $(CFLAGS) -D__AMIGADATE__=\"$(AMIGADATE)\" -std=gnu++11 -fno-exceptions -fno-rtti
LDFLAGS+= $(SDL_LDFLAGS) -lpthread 

OBJS_C   :=	main.o\
     ../../libs/stb_vorbis/stb_vorbis.o \
     ../../libs/minimp3/minimp3.o \
     ../../libs/tinf/tinflate.o

all: $(TARGET)

$(TARGET): $(OBJS_C)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS_C) 
	rm -f $(TARGET) $(TARGET).s 

dump:
	ppc-morphos-objdump --disassemble-all --reloc $(TARGET) >$(TARGET).s
