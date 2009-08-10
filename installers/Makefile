
# this will build a self-contained installer script

FILES=npbetter32.so npbetter64.so

all: ${FILES} install_script.sh
	rm -f data.tar.gz
	tar zcvf data.tar.gz ${FILES}
	cat install_script.sh |sed `md5sum data.tar.gz | awk '{print "s/MD5SUM/" $$1 "/"}'` > installer.bin
	sed 's/"/\\"/g' ../LICENSE > LICENSE.tmp
	sed -i '/LICENSE:/ r LICENSE.tmp' installer.bin
	sed -i `wc -l installer.bin | awk '{print "s/SCRIPT_LINES/" (($$1)+1) "/"}'` installer.bin
	cat data.tar.gz >> installer.bin
	rm -f LICENSE.tmp data.tar.gz
