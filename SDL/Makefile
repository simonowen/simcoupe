# SimCoupe - A SAM Coupe emulator
#
# SDL Makefile
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

TARGET=simcoupe
CC=gcc
CXX=g++

SDL=.
BASE=../Base
EXTERN=../Extern

USE_OPENGL=0
USE_ZLIB=1
USE_SAASOUND=1

.SUFFIXES: .cpp .c

CFLAGS=-O2 -I${SDL} -I${BASE} -I${EXTERN} $(shell sdl-config --cflags)
CXXFLAGS=${CFLAGS} -Wall
LIBS=-L. -L/usr/X11R6/lib -lm $(shell sdl-config --libs --cflags)

BASE_SRC=${BASE}/ATA.cpp ${BASE}/Atom.cpp ${BASE}/CDisk.cpp \
 ${BASE}/CDrive.cpp ${BASE}/Clock.cpp ${BASE}/CPU.cpp ${BASE}/CScreen.cpp \
 ${BASE}/CStream.cpp ${BASE}/Debug.cpp ${BASE}/Disassem.cpp ${BASE}/Font.cpp \
 ${BASE}/Frame.cpp ${BASE}/GUI.cpp ${BASE}/GUIDlg.cpp ${BASE}/GUIIcons.cpp \
 ${BASE}/IO.cpp ${BASE}/Main.cpp ${BASE}/Memory.cpp ${BASE}/Mouse.cpp \
 ${BASE}/Options.cpp ${BASE}/PNG.cpp ${BASE}/Profile.cpp ${BASE}/Util.cpp \
 ${EXTERN}/SAASound.cpp
 
SDL_SRC=${SDL}/Display.cpp ${SDL}/Floppy.cpp ${SDL}/Input.cpp ${SDL}/MIDI.cpp \
 ${SDL}/OSD.cpp ${SDL}/Parallel.cpp ${SDL}/Serial.cpp ${SDL}/Sound.cpp \
 ${SDL}/UI.cpp ${SDL}/Video.cpp

ifeq (${USE_OPENGL},1)
CFLAGS += -DUSE_OPENGL
LIBS += -lGL
endif

ifeq (${USE_ZLIB},1)
CFLAGS += -DUSE_ZLIB
LIBS += -lz
endif

ifeq (${USE_SAASOUND},1)
CFLAGS += -DUSE_SAASOUND
LIBS += -lSAASound
endif

BASE_OBJS=$(BASE_SRC:%.cpp=%.o) ${EXTERN}/unzip.o
SDL_OBJS=$(SDL_SRC:%.cpp=%.o)

all:	${TARGET}

${TARGET}:	${BASE_OBJS} ${SDL_OBJS} Makefile
	${CXX} -o ${TARGET} ${CXXFLAGS} ${BASE_OBJS} ${SDL_OBJS} ${LIBS}

.cpp.o:
	${CXX} -o $@ -c $< ${CXXFLAGS}

.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

clean:
	rm -f ${TARGET} ${BASE}/*.o ${SDL}/*.o ${EXTERN}/*.o
