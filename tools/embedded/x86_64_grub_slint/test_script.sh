#!/bin/sh
echo RADIX_USER_SH_SCRIPT_OK
which nano
nano --version
echo RADIX_RASH_PATH_EXEC_OK
/usr/bin/nano-tiny --version
/usr/bin/nano-full --version
/usr/bin/nano --version
/usr/bin/nano-tiny --radix-smoke /tmp/nano-tiny.txt
/usr/bin/nano-full --radix-smoke /tmp/nano-full.txt
cat /tmp/nano-tiny.txt
cat /tmp/nano-full.txt
tput cup 0 0
echo RADIX_USER_RADSH_EXIT_OK
echo radix | wc
echo RADIX_USER_RADSH_EXIT_OK
