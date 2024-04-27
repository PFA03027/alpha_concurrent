#!/usr/bin/sh
GMONDIR=`dirname $1`
echo ${GMONDIR}
gprof $1 ${GMONDIR}/gmon.out > ${GMONDIR}/prof.out.txt
echo Profile result of $1 is ${GMONDIR}/prof.out.txt

