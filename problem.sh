#!/usr/bin/sh

# check argc
if [ $# -ne 1 ]
then
    echo "usage: $0 proof.ll"
    exit
fi

# <$1 sed -n "N;/\ndefine.* i8 @pino/l"
# # match from pattern to left of , to pattern to right of , then print
# <$1 sed -n "/@pino.*{/,/}/p"
# match from pino to the end of function, or first load. if not last line, continue. if contains `load`, print, otherwise don't.
<"$1" sed -n -E '/@pino/,/(}|load)/H; $!d; x;
      	     	 /load/{p;Q};
		 z; a all good; p' | bat -l ll --paging never --color always
