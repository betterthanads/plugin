# ${PLUGNAME} - simple NPAPI plugin test case

include config.mk

PLUGNAME = np-bta
TEST = test.html
SRC = ${PLUGNAME}.c bta_api.c
OBJ = ${SRC:.c=.o}

all: options ${PLUGNAME}.so ${shell uname}

options:
	@echo ${PLUGNAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

${PLUGNAME}.so: ${OBJ}
	@echo LD $@
	@${CC} -v -shared -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f ${PLUGNAME}.so ${OBJ} Localized.rsrc
	@rm -rf ${PLUGNAME}.plugin

Linux:
	@chmod 755 ${PLUGNAME}.so
	@echo Setup: sudo ln -s ${shell pwd}/${PLUGNAME}.so /usr/lib/mozilla/plugins/${PLUGNAME}.so
	@echo Test: /usr/lib/webkit-1.0/libexec/GtkLauncher file://`pwd`/${TEST} # apt-get install libwebkit-1.0-1

Darwin:
	/Developer/Tools/Rez -o mac/Localized.rsrc -useDF mac/Localized.r
	mkdir -p ${PLUGNAME}.plugin/Contents/MacOS
	mkdir -p ${PLUGNAME}.plugin/Contents/Resources/English.lproj
	cp -r mac/Localized.rsrc ${PLUGNAME}.plugin/Contents/Resources/English.lproj
	cp -f mac/Info.plist ${PLUGNAME}.plugin/Contents
	cp -f ${PLUGNAME}.so ${PLUGNAME}.plugin/Contents/MacOS/${PLUGNAME}
	@echo Setup: sudo ln -s `pwd`/${PLUGNAME}.so /Library/Internet\\ Plug-Ins/${PLUGNAME}.plugin
	@echo Test: /Applications/Safari.app/Contents/MacOS/Safari ${TEST}
	@echo Test: /Applications/Firefox.app/Contents/MacOS/firefox ${TEST}
	@echo Test: /Applications/Opera.app/Contents/MacOS/Opera file://`pwd`/${TEST}

.PHONY: all options clean Darwin Linux
