# SimCoupe Makefile
# ToDo: add file dependencies
TARGET=simcoupe
CC=gcc
CXX=g++

EXTERN=../Extern
BASE=../Base
SDL=.

.SUFFIXES: .cpp .c

CFLAGS=-Wall -O2 -I${SDL} -I${BASE} $(shell sdl-config --cflags)
CXXFLAGS=${CFLAGS}
LIBS= -lm -lz $(shell sdl-config --libs)

SRC_FILES=${BASE}/ATA.cpp ${BASE}/Atom.cpp ${BASE}/CDisk.cpp ${BASE}/CDrive.cpp ${BASE}/Clock.cpp ${BASE}/CPU.cpp ${BASE}/CScreen.cpp ${BASE}/CStream.cpp ${BASE}/Debug.cpp ${BASE}/Disassem.cpp ${BASE}/Font.cpp ${BASE}/Frame.cpp ${BASE}/IO.cpp ${BASE}/Main.cpp ${BASE}/Memory.cpp ${BASE}/Mouse.cpp ${BASE}/Options.cpp ${BASE}/PNG.cpp ${BASE}/Profile.cpp ${BASE}/Util.cpp \
	${SDL}/Display.cpp ${SDL}/Floppy.cpp ${SDL}/Input.cpp ${SDL}/MIDI.cpp ${SDL}/OSD.cpp ${SDL}/Parallel.cpp ${SDL}/Serial.cpp ${SDL}/Sound.cpp ${SDL}/UI.cpp ${SDL}/Video.cpp \
	${EXTERN}/SAASound.cpp

OBJS=$(SRC_FILES:%.cpp=%.o) ${EXTERN}/unzip.o


all:	${TARGET}

${TARGET}:	${OBJS} Makefile
	${CXX} -o ${TARGET} ${CFLAGS} ${OBJS} ${LIBS}

.cpp.o:
	${CXX} -o $@ -c $< ${CXXFLAGS}

.c.o:
	${CC} -o $@ -c $< ${CFLAGS}

clean:
	rm -f ${TARGET} ${OBJS}
