#!/bin/sh

tail -n +40 "$0" > /tmp/data.tar.gz
trap 'rm -f $/tmp/data.tar.gz; exit 1' HUP INT QUIT TERM

which md5sum >/dev/null 2>&1
if [[ $? -eq 0 ]]; then
	echo -e -n 'Verifying archive with md5sum...\n   '
	echo "MD5SUM  /tmp/data.tar.gz" | md5sum --check 2>/dev/null
	if [[ $? -ne 0 ]]; then
		echo 'Verification failed. Please re-download this file and try again.'
		exit 1
	fi
else
	echo 'Cannot verify archive (md5sum not installed)'
fi

WHOAMI=`whoami`
INSTALLTO='~/.mozilla/plugins/'
if [[ "X$WHOAMI" == "Xroot" ]]; then
	INSTALLTO="/usr/lib/mozilla-firefox/plugins/"
fi

echo -e "\nThis script will install the BetterThanAds plugin into $INSTALLTO"
echo -e "\n   ---  Press Ctrl-C NOW to abort  ---"
sleep 3

cd /tmp
tar zxf data.tar.gz
mkdir -p $INSTALLTO
cp -f npbetter.so $INSTALLTO
echo -e "\n\nInstallation complete."

rm -f /tmp/data.tar.gz

firefox http://betterthanads.com/activate/ &

exit 0

