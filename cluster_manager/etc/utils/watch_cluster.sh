#!/bin/bash
# Script ran from root's crontab to watch cluster

if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

if [ -z "${CFGFILE}" ]; then export CFGFILE=/etc/drcm/my_cluster_conf/my_cluster_info.sh; fi
. ${CFGFILE}
if [ -z "${DRCM_server}" ]; then export DRCM_server="/usr/sbin/drcm_server"; fi
if [ -z "${GREP}" ]; then export GREP="grep"; fi

# Source the alert script
. /etc/drcm/utils/send_alert.sh

if [ ! -z "${1}" ]; then export CLUSTER_NAME="${1}"; fi
if [ ! -z "${2}" ]; then export SUBJECT="${2}"; fi
if [ ! -z "${3}" ]; then export BODY="${1}"; fi


${DRCM_server} --client --command=cm_stat &> ${VARdir}/cm_stat.$$
${GREP} 'Cluster status: HEALTHY' ${VARdir}/cm_stat.$$
if [ $? -eq 0 ]; then
    echo "All cluster nodes and resources are healthy" 
    if [ -f ${VARdir}/cm_stat.$$ ]; then rm -f ${VARdir}/cm_stat.$$; fi
    exit 0
fi

if [ -z "${CLUSTER_NAME}" ]; then export CLUSTER_NAME=$(hostname -s); fi
if [ -z "${SUBJECT}" ]; then export SUBJECT="[!ACTION!] CM Alert Message From ${CLUSTER_NAME}"; fi

cat /dev/null > ${VARdir}/alert_mesg.$$

if [ ! -z "${BODY}" ]; then
    if [ -f "${BODY}" ]; then
	cat ${BODY} >> ${VARdir}/alert_mesg.$$
	echo "" >> ${VARdir}/alert_mesg.$$
    else
	echo "${BODY}" >> ${VARdir}/alert_mesg.$$
	echo "" >> ${VARdir}/alert_mesg.$$
    fi
fi

cat ${VARdir}/cm_stat.$$ >> ${VARdir}/alert_mesg.$$

if [ -z "${BODY}" ]; then export BODY="${VARdir}/alert_mesg.$$"; fi

echo "DRCM cluster reporting failuers"
echo "Sending alert message"
send_alert "${SUBJECT}" "${BODY}"

if [ -f ${VARdir}/alert_mesg.$$ ]; then rm -f ${VARdir}/alert_mesg.$$; fi
if [ -f ${VARdir}/cm_stat.$$ ]; then rm -f ${VARdir}/cm_stat.$$; fi

exit 0
