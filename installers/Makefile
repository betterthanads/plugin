
# this will build a self-contained installer script

FILES=npbetter.so
LICENSE=${shell sed 's/.$$/\\n/' license.txt}

all: ${FILES} install_script.sh
	rm -f data.tar.gz
	tar zcvf data.tar.gz ${FILES}
	cat install_script.sh |sed `md5sum data.tar.gz | awk '{print "s/MD5SUM/" $$1 "/"}'` > installer.sh
	sed -i 's%LICENSE%${LICENSE}%' installer.sh
	sed -i `wc -l installer.sh | awk '{print "s/SCRIPT_LINES/" (($$1)+1) "/"}'` installer.sh
	cat data.tar.gz >> installer.sh

clean:
	rm -f data.tar.gz installer.sh npbetter.so

npbetter.so:
	cd ..; make
	cp ../npbetter.so .
