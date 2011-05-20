#!/bin/sh

git mv $1 $2
find -type f -name \*.c -o -name \*.h -o -name \*.S -exec res/subst.sh '{}' $1 $2 \;
