# My Friend Pedro - PSP port (alpha 1.1)
# Build: make        -> EBOOT.PBP
#        make dist   -> dist/ (EBOOT.PBP + assets/), copy dist/* to PSP/GAME/Pedro/
TARGET = pedro
OBJS = src/game.o src/platform_sdl.o
INCDIR = src

CFLAGS = -O2 -G0 -Wall $(shell psp-pkg-config --cflags sdl2 SDL2_image SDL2_mixer)
LIBS = $(shell psp-pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = My Friend Pedro (alpha 1.1)
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_PIC1 = PIC1.PNG

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

dist: EBOOT.PBP
	mkdir -p dist
	cp EBOOT.PBP dist/
	cp -r assets dist/
