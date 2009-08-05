
# this will build a self-contained installer script

FILES=npbetter.so

all: ${FILES} install_script.sh
	rm -f data.tar.gz
	tar zcvf data.tar.gz ${FILES}
	cat install_script.sh data.tar.gz |sed `md5sum data.tar.gz | awk '{print "s/MD5SUM/" $$1 "/"}'` > installer.sh

npbetter.so:
	cd ..; make
	cp ../npbetter.so .
