#!/usr/bin/sh
GMONDIR=`dirname $1`
echo ${GMONDIR}
gprof $1 ${GMONDIR}/gmon.out > ${GMONDIR}/prof.out.txt

