# npbetter version
VERSION = 0.9

# mac code is completely untested and nonexistant
ifeq (${shell uname}, Darwin)
  CPPFLAGS = -DVERSION=\"${VERSION}\" -DWEBKIT_DARWIN_SDK
  LDFLAGS = -dynamiclib #-framework Carbon -framework CoreFoundation -framework WebKit
  
	SYS_SRC = bta_osx.c
else
  INCS = -I/usr/include/xulrunner-1.9.1/stable
  CPPFLAGS = -DVERSION=\"${VERSION}\" -DXULRUNNER_SDK -DMOZ_X11 -DXP_UNIX
  LDFLAGS = -lXpm

  SYS_SRC = bta_xwin.c
endif
#CFLAGS = -g -pedantic -Wall -O2 ${INCS} ${CPPFLAGS} -DDEBUG
CFLAGS = -O2 -fPIC ${INCS} ${CPPFLAGS}

# compiler and linker
CC = gcc
