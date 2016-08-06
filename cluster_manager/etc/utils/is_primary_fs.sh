#!/bin/bash

if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

if [ -z "${CFGFILE}" ]; then export CFGFILE=/etc/drcm/my_cluster_conf/my_cluster_info.sh; fi
. ${CFGFILE}
if [ -z "${IP}" ]; then export IP="/usr/sbin/ip"; fi
if [ -z "${GREP}" ]; then export GREP="/usr/bin/grep"; fi
if [ -z "${PRIMARY_FILESYSTEMS_ETH}" ]; then exit 1; fi
if [ -z "${PRIMARY_FILESYSTEMS_IP}" ]; then exit 1; fi

${IP} add show ${PRIMARY_FILESYSTEMS_ETH} | ${GREP} inet | ${GREP} ${PRIMARY_FILESYSTEMS_IP} > /dev/null
if [ $? -eq 0 ]; then
    echo "TRUE"
else
    echo "FALSE"
fi

exit 0
