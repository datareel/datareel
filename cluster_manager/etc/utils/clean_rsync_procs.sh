#!/bin/bash

if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

HUNGPROCS=$(ps eaxo etime,pid,comm | grep rsync |  grep -v " 00:" | grep -- - | awk '{ print $2 }')

for p in ${HUNGPROCS}
do
    echo "Killing proc ${p}"
    kill -9 ${p}
done

OLDPROCS=$(ps eaxo etime,pid,comm | grep rsync |  grep -v " 00:" | grep -v -- - | awk '{ print $2 }')

for p in ${OLDPROCS}
do
    echo "Checking proc ${p}"
    OLDPROCSTIME=$(ps eaxo etime,pid,comm | grep rsync |  grep ${p} | awk -F: '{ print $3 }' | sed s/' '//g)
    if [ "${OLDPROCSTIME}" != "" ]
    then
	TIME=$(ps eaxo etime,pid,comm | grep rsync |  grep ${p} | awk '{ print $1 }' | sed s/' '//g)
	echo "Proc ${p} beening running for more than 60 minutes [${TIME}]"
	echo "Killing proc ${p}"
	kill -9 ${p}
    fi
done

exit 0
