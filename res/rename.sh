#!/bin/sh

function subst_header () {
    TMP=.temp
    sed -e "s:/include <$2>/include <$3>" <$1 >$TMP
    mv $1 .$1.save
    mv $TMP $1
}

git mv $1 $2
find -type f -name \*.c -o -name \*.h -o -name \*.S -exec subst_header '{}' $1 $2 \;
