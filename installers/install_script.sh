#!/bin/sh

echo -e "
LICENSE:
--- END LICENSE ---
"

tail -n +SCRIPT_LINES "$0" > /tmp/data.tar.gz
trap 'rm -f $/tmp/data.tar.gz; exit 1' HUP INT QUIT TERM

WHOAMI=`whoami`
INSTALLTO="$HOME/.mozilla/plugins/"
if [[ "X$WHOAMI" == "Xroot" ]]; then
	echo "You are root, trying system-wide install..."

	if [ -d "/usr/lib/mozilla-firefox/plugins/" ]; then
		INSTALLTO="/usr/lib/mozilla-firefox/plugins/"
	fi

	if [ -d "/opt/netscape/plugins/" ]; then
		INSTALLTO="/opt/netscape/plugins/"
	fi

	if [ -d "/usr/lib/nsbrowser/plugins/" ]; then
	  INSTALLTO="/usr/lib/nsbrowser/plugins/"
	fi

	if [ "X$INSTALLTO" == "X$HOME/.mozilla/plugins/" ]; then
		echo "Couldn't figure out where to do system-wide install, falling back to your home dir"
	fi
fi

ARCH=`uname -m`
if [[ "X$ARCH" == "Xx86_64" ]]; then
	ARCH=64
else
	ARCH=32
fi

echo "  The BetterThanAds Plugin is released under the GNU GPLv3, displayed above.
  If you agree to the terms of the license and wish to install the $ARCH-bit binary
  into $INSTALLTO, type 'I agree' to continue installation.
	
  If you do not agree, or this path is incorrect, press CTRL-C NOW to abort.  
	
  NOTE: Run this script using root permissions for a system-wide install.
"

echo -n "  Agree to license? [type 'I agree' or press ctrl-c to quit]: "
read -r ACCPT_LICENSE
ACCPT_LICENSE=`echo $ACCPT_LICENSE | sed 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'`;

echo -e "  -----\n"
if [[ "X$ACCPT_LICENSE" == "Xi agree" ]]; then

	which md5sum >/dev/null 2>&1
	if [[ $? -eq 0 ]]; then
		echo -e -n '  Verifying archive with md5sum...\n   '
		echo "MD5SUM  /tmp/data.tar.gz" | md5sum --check 2>/dev/null
		if [[ $? -ne 0 ]]; then
			echo '    Verification failed. Please re-download this file and try again.'
			exit 1
		fi
	else
		echo "  Cannot verify archive (md5sum not installed)"
		echo "    Continuing anyway..."
	fi

	cd /tmp
	echo "  Extracting files..."
	tar zxf data.tar.gz
	mkdir -p $INSTALLTO
	echo "  Copying npbetter$ARCH.so to $INSTALLTO ..."
	cp -f npbetter$ARCH.so $INSTALLTO
	rm -f data.tar.gz npbetter??.so
	echo -e "\n\n  Installation complete."
	echo -e "  Activate Plugin at: http://betterthanads.com/activate/"

	firefox http://betterthanads.com/activate/ &
else
  rm /tmp/data.tar.gz
	echo -e "\n    You must agree to the license (using the words \"I agree\") to install.\n\n"
fi

exit 0

