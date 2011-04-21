#! /bin/sh

cd ..
ARCHNAME=cosec`date +%g.%m.%d`.tar
tar cf $ARCHNAME cosec
gzip $ARCHNAME
GZIPNAME="$ARCHNAME".gz
mv $GZIPNAME cosec_hist


