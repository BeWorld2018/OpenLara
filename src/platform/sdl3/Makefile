OBJS_C   :=	main.o\
     ../../libs/stb_vorbis/stb_vorbis.o \
     ../../libs/minimp3/minimp3.o \
     ../../libs/tinf/tinflate.o
	 
CC = ppc-morphos-gcc-11
CXX = ppc-morphos-g++-11

TARGET = openlara

SDL_CFLAGS := -noixemul -I/gg/usr/local/include
SDL_LDFLAGS := -noixemul -L/gg/usr/local/lib -lSDL3 -lGL -lm -lpthread

CFLAGS+= -std=gnu++11 -fsigned-char -O1 -g -I../../ -fno-exceptions -Wno-unused-variable -fno-rtti  -D__SDL3__ $(SDL_CFLAGS) -Wall
#-ffunction-sections -fdata-sections -Wl,--gc-sections
CXXFLAGS+= $(CFLAGS)
LDFLAGS+= $(SDL_LDFLAGS) 

all: $(TARGET)

$(TARGET): $(OBJS_C)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJS_C) 
	rm -f $(TARGET) $(TARGET).s 

dump:
	ppc-morphos-objdump --disassemble-all --reloc $(TARGET) >$(TARGET).s
