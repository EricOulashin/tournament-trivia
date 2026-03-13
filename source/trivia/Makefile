# Alternate project for a conio.h for Linux using Ncurses:
# https://github.com/casualsnek/linuxconio


CC	?= gcc
CXX	?= g++
SRC_ROOT	:=	../../..

# Common flags (shared by all targets)
# TODO: Update ODOORS_SRC to point to the directory where the OpenDoors SDK is located
# (i.e., for Synchronet, with the Synchronet Git repository cloned, ODOORS_SRC would be
# something like /sbbs/repo/src/odoors)
ODOORS_SRC	:= /sbbs/repo/src/odoors
COMMON_CFLAGS := $(CIOLIB-MT_CFLAGS) $(XPDEV-MT_CFLAGS) -DLINUX
LDFLAGS += $(CIOLIB-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

# XSDK/console flags (for trivsync and trivsrv)
CFLAGS += $(COMMON_CFLAGS) -D__CONSOLE__
CPPFLAGS += $(CFLAGS) -std=c++20

# OpenDoors flags (for triv32 - no __CONSOLE__)
OD_CFLAGS := $(COMMON_CFLAGS)
OD_CPPFLAGS := $(OD_CFLAGS) -std=c++20

TRIVIA_OBJ_LIST = commands.o player.o reginfo.o threads.o

all: trivsrv trivsync triv32 trivconfig regtriv

trivsync: ../intrnode/door.cpp platxsdk.o xsdk.o xsdkvars.o xsdkwrap.o intrnode.o linuxPlatformDefs.o gamesrv.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -L${ODOORS_SRC}/libs-`uname` -lncurses -lrt -lpthread -o trivsync ../intrnode/door.cpp platxsdk.o xsdk.o xsdkvars.o xsdkwrap.o intrnode.o linuxPlatformDefs.o gamesrv.o

trivsrv: trivia.cpp ../intrnode/gamesrv.h commands.o player.o reginfo.o threads.o gamesrv.o linuxPlatformDefs.o intrnode.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -lrt -lpthread -o trivsrv trivia.cpp commands.o player.o reginfo.o threads.o gamesrv.o linuxPlatformDefs.o intrnode.o

triv32: door_od.o platod.o intrnode.o linuxPlatformDefs.o gamesrv.o
	$(CXX) $(OD_CPPFLAGS) $(LDFLAGS) -o triv32 door_od.o platod.o intrnode.o linuxPlatformDefs.o gamesrv.o -L${ODOORS_SRC}/libs-`uname` -lODoors -lrt -lpthread

# OpenDoors-specific object files (compiled without __CONSOLE__)
door_od.o: ../intrnode/door.cpp ../intrnode/door.h ../intrnode/intrnode.h ../intrnode/opendoor.h
	$(CXX) $(OD_CPPFLAGS) $(LDFLAGS) -c -o door_od.o ../intrnode/door.cpp

platod.o: ../intrnode/platod.cpp doorset.h ../intrnode/door.h ../intrnode/intrnode.h ../intrnode/opendoor.h
	$(CXX) $(OD_CPPFLAGS) $(LDFLAGS) -c -o platod.o ../intrnode/platod.cpp

# Shared object files
linuxPlatformDefs.o: ../intrnode/linuxPlatformDefs.cpp
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o linuxPlatformDefs.o ../intrnode/linuxPlatformDefs.cpp

player.o: player.cpp trivia.h ../intrnode/gamesrv.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o player.o player.cpp

reginfo.o: reginfo.cpp trivia.h ../intrnode/gamesrv.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o reginfo.o reginfo.cpp

threads.o: threads.cpp trivia.h ../intrnode/gamesrv.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o threads.o threads.cpp

commands.o: commands.cpp commands.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o commands.o commands.cpp

intrnode.o: ../intrnode/intrnode.cpp ../intrnode/intrnode.h ../intrnode/platformDefs.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o intrnode.o ../intrnode/intrnode.cpp

gamesrv.o: ../intrnode/gamesrv.cpp ../intrnode/gamesrv.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o gamesrv.o ../intrnode/gamesrv.cpp

platlocl.o: ../intrnode/platlocl.cpp ../intrnode/door.h ../intrnode/intrnode.h
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -c -o platlocl.o ../intrnode/platlocl.cpp

platxsdk.o: ../intrnode/platxsdk.cpp ../intrnode/door.h ../intrnode/intrnode.h ../intrnode/opendoor.h
	$(CXX) $(CPPFLAGS) -D__COLORS $(LDFLAGS) -c -o platxsdk.o ../intrnode/platxsdk.cpp

xsdk.o: ../intrnode/xsdk.c ../intrnode/xsdk.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o xsdk.o ../intrnode/xsdk.c

xsdkvars.o: ../intrnode/xsdkvars.c ../intrnode/xsdkinet.h ../intrnode/xsdkdefs.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o xsdkvars.o ../intrnode/xsdkvars.c

xsdkwrap.o: ../intrnode/xsdkwrap.c ../intrnode/xsdkwrap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o xsdkwrap.o ../intrnode/xsdkwrap.c


trivconfig: trivconfig.cpp
	$(CXX) -std=c++20 -DLINUX -o trivconfig trivconfig.cpp -lncurses

regtriv: ../../keygen/regtriv.cpp
	$(CXX) -std=c++20 -DLINUX -o regtriv ../../keygen/regtriv.cpp

clean:
	rm -f *.o
	rm -f trivsrv
	rm -f trivsync
	rm -f triv32
	rm -f trivconfig
	rm -f regtriv
