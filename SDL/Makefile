# *** LEGACY MAKEFILE - use CMake build system in parent folder instead ***
#
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
USE_RESID=0  # libresid will probably need manual building

.SUFFIXES: .cpp .c

CFLAGS=-O2 -Wall -I${SDL} -I${BASE} -I${EXTERN} $(shell sdl-config --cflags)
CXXFLAGS=${CFLAGS} -Wall
LIBS=-L. -lm $(shell sdl-config --libs --cflags)

BASE_SRC=$(wildcard ${BASE}/*.cpp)
SDL_SRC=$(wildcard ${SDL}/*.cpp)
EXTERN_SRC=$(wildcard ${EXTERN}/*.c)

ifeq (${USE_OPENGL},1)
CFLAGS += -DUSE_OPENGL
ifeq (,$(findstring OpenGL,$(LIBS)))
LIBS += -lGL
endif
endif

ifeq (${USE_ZLIB},1)
CFLAGS += -DUSE_ZLIB
LIBS += -lz
endif

ifeq (${USE_RESID},1)
CFLAGS += -DUSE_RESID
LIBS += -lresid
endif

ifneq (,$(findstring -framework Carbon,$(LIBS)))
CC=cc
CXX=cc
endif

BASE_OBJS=$(BASE_SRC:%.cpp=%.o)
SDL_OBJS=$(SDL_SRC:%.cpp=%.o)
EXTERN_OBJS=$(EXTERN_SRC:%.c=%.o)

all:	${TARGET}

${TARGET}:	${BASE_OBJS} ${SDL_OBJS} ${EXTERN_OBJS} Makefile
	${CXX} -o ${TARGET} ${CXXFLAGS} ${BASE_OBJS} ${SDL_OBJS} ${EXTERN_OBJS} ${LIBS}

.cpp.o:
	${CXX} -o $@ -c $< ${CXXFLAGS}

.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

clean:
	rm -f ${TARGET} ${BASE}/*.o ${SDL}/*.o ${EXTERN}/*.o
