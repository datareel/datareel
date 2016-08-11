#!/bin/bash
# ----------------------------------------------------------- 
# BASH Script
# Operating System(s): RHEL/CENTOS
# Run Level(s): 3, 5
# Shell: BASH shell
# Original Author(s): DataReel Software Development
# File Creation Date: 07/17/2016 
# Date Last Modified: 08/11/2016
#
# Version control: 3.20
#
# Contributor(s):
# ----------------------------------------------------------- 
# ------------- Program Description and Details ------------- 
# ----------------------------------------------------------- 
# This file is part of the DataReel software distribution.
#
# Datareel is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. 
#
# Datareel software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the DataReel software distribution.  If not, see
# <http://www.gnu.org/licenses/>.
#
# Include script used to text and email system or process alerts.
# Cluster script to email or text alerts is designed to be used by
# all users. Users can customize by setting ENV vars in their
# ~/.bashrc file. ENV vars to set:
#
# List to send alerts too:
#
## EMAILLIST="/etc/drcm/my_cluster_conf/cm_alert_list.sh"
#
# DIR to write process and temp data too:
#
## export VARdir="$HOME/var"
#
# Timespan between alert messages to prevent a flood of email and/or text 
# messages. Default setting is every 4 hours between messages
#
## export ALERTTIMESPANHOURS="4"
#
# 
# Process files used to track time between alert meassgaes:
#
## export TIMEFILE="${VARdir}/cm_alert.timefile"
## export INITFILE="${VARdir}/cm_alert.initfile"
#
# Send email, text message or both:
#
## export SENDEMAIL="TRUE"
## export SENDTEXT="TRUE"
#
# Once this script is sourced in a calling script use
# the send_alert() function with SUBJECT and BODY args:
#
# source /etc/drcm/utils/send_alert.sh
# 
#    
# send_alert "CM Alert" "CM alert message info..."
#
# NOTE The 2nd arg, BODY, can be a string or a file:
#
# send_alert "CM Alert" /tmp/my_alert_message.txt
#
# ----------------------------------------------------------- 

# Get the user's bashrc for overrides
if [ -f ~/.bashrc ]; then . ~/.bashrc; fi

if [ -z "${EMAILLIST}" ]; then export EMAILLIST="/etc/drcm/my_cluster_conf/cm_alert_list.sh"; fi
if [ -z "${VARdir}" ]; then export VARdir="$HOME/var"; fi

# This is the timespan between alert messages to prevent a flood of email or text messages.
# The default is 4 hours between messages for all callers. Any process can override 
# if the system or process needs to set more or less frequently.  
if [ -z "${ALERTTIMESPANHOURS}" ]; then export ALERTTIMESPANHOURS="4"; fi
if [ -z "${TIMEFILE}" ]; then export TIMEFILE="${VARdir}/cm_alert.timefile"; fi
if [ -z "${INITFILE}" ]; then export INITFILE="${VARdir}/cm_alert.initfile"; fi

SENDALERT="TRUE"
if [ -z "${SENDEMAIL}" ]; then export SENDEMAIL="TRUE"; fi
if [ -z "${SENDTEXT}" ]; then export SENDTEXT="TRUE"; fi

# This should be set in PATH to /bin/mail
if [ -z "${UNIXMAILPROGRAM}" ]; then export UNIXMAILPROGRAM="mail"; fi 

function send_alert() {
    # Caller must provide SUBJECT and BODY aguments
    # 
    # SUBJECT="Error Message"
    # BODY="Short text string or file name"
    #
    # source ~/Desktop/meetings/email_alert.sh
    # email_alert "$SUBJECT" "$BODY"
    #
    # Caller can also provide an option email list as arg #3
    #
    # EMAILlist="root, sysadmins, backupusers"
    # email_logs "$SUBJECT" "$BODY" "$EMAILlist"

    if [ ! -d "${VARdir}" ]; then mkdir -p  "${VARdir}"; fi

    if [ ! -f ${TIMEFILE} ]
    then
	date +%s > ${TIMEFILE}
	chmod 666 ${TIMEFILE}
    fi
    
    LASTTIME=$(cat ${TIMEFILE})
    CURRTIME=$(date +%s)
    
    TIMESPAN_SECS=$(echo "${ALERTTIMESPANHOURS} * 3600" | bc)
    ELAPSED_SECS=$(echo "${CURRTIME} - ${LASTTIME}" | bc)
    
    HRS=$(echo "${ELAPSED_SECS} / 3600" | bc)
    MIN=$(echo "${ELAPSED_SECS} % 3600 / 60" | bc)
    SEC=$(echo "${ELAPSED_SECS} % 3600 % 60" | bc)

    echo "CM send alert called by ${USER}"
    echo "send alert process DIR = ${VARdir}"
    
    # Setup the comma delimited the admins list from host-based config file
    if [ "${EMAILlist}" == "" ]
    then 
	if [ -f ${EMAILLIST} ]
	then
	    echo "Reading list ${EMAILLIST}"
	    source ${EMAILLIST}
	else
	    echo "INFO - No alert list file ${EMAILLIST}"
	    echo "INFO - Writing default alert list file"
	    touch ${EMAILLIST}
	    echo "# Set your comma delimited alert lists here" >> ${EMAILLIST}
	    echo "" >> ${EMAILLIST}
	    echo '##export EMAILlist="root,name@example.com"' >> ${EMAILLIST}
	    echo "" >> ${EMAILLIST}
	    echo '##export TEXTlist="555-555-5555@mobile_domain.com"' >> ${EMAILLIST} 
	    echo "" >> ${EMAILLIST}
	    echo "# List of domains for mobile phones:" >> ${EMAILLIST}
	    echo '# Verizon Wireless: (mobile#)@vtext.com' >> ${EMAILLIST}
	    echo '# AT&T: (mobile#)@txt.att.net' >> ${EMAILLIST}
	    echo '# Cingular: (mobile#)@mycingular.com' >> ${EMAILLIST}
	    echo '# Nextel: (mobile#)@messaging.nextel.com' >> ${EMAILLIST}
	    echo '# T-Mobile: (mobile#)@tmomail.net' >> ${EMAILLIST}
	    echo '# Sprint: (mobile#)@messaging.sprintpcs.com' >> ${EMAILLIST}
	    echo '# Tracfone: (mobile#)@cingularme.com' >> ${EMAILLIST}
	    echo "" >> ${EMAILLIST}
	    echo "" >> ${EMAILLIST}
	    source ${EMAILLIST}
	fi
    fi
    
    # Set the comma delimited admin list here if not set above
    if [ -z "${EMAILlist}" ]; then export EMAILlist="root"; fi

    echo -n "Our last alert email was sent: "
    if [ $HRS -gt 0 ]
    then
	echo -n "$HRS hours "
    fi
    if [ $MIN -gt 0 ]
    then
	echo -n "$MIN minutes "
    fi
    if [ $SEC -gt 0 ]
    then
	if [ $MIN -gt 0 ]
	then
	    echo -n "and $SEC seconds "
	elif [ $HRS -gt 0 ]
	then
	    echo -n "and $SEC  seconds "
	else
	    echo -n "$SEC seconds "
	fi
    fi
    echo "ago." 

    if [ ${ELAPSED_SECS} -lt ${TIMESPAN_SECS} ] && [ -f ${INITFILE} ]
    then
	echo "INFO - No alert message will be sent."
	echo "We are only sending alerts every ${ALERTTIMESPANHOURS} hours."
	SENDALERT="FALSE"
    else
	date +%s > ${INITFILE}
	chmod 666 ${INITFILE}
	echo "Sending alert email..."
	date +%s > ${TIMEFILE}
	chmod 666 ${TIMEFILE}
	SENDALERT="TRUE"
    fi

    if [ "${SENDALERT^^}" == "FALSE" ]; then return; fi

    SUBJECT="${1}"
    BODY="${2}"
    HOSTNAME=$(hostname -s)
    PROC=$(ps -ef | grep $$ | grep -v grep)

    if [ ! -z "${3}" ]
	then
	EMAILlist="${3}"
    fi

    if [ -z "${SUBJECT}" ]
	then
	SUBJECT="[!ACTION!] CM Alert Message From ${HOSTNAME}"
    fi

    if [ -z "${BODY}" ]
	then
	cat /dev/null > ${VARdir}/body.$$
	echo "CM Alert message from process: " >> ${VARdir}/body.$$
 	echo "${PROC}" >> ${VARdir}/body.$$
	BODY="${VARdir}/body.$$"
    fi

    if [ ! -f "${BODY}" ]
	then
	cat /dev/null > ${VARdir}/body.$$
	echo "${BODY}" >> ${VARdir}/body.$$
	BODY="${VARdir}/body.$$"
    fi

    if [ ! -z "${EMAILlist}" ] && [ "${SENDEMAIL^^}" == "TRUE" ]
    then
	echo "Emailing message to: $EMAILlist"
	${UNIXMAILPROGRAM} -s "$SUBJECT" $EMAILlist < $BODY 
    fi

    if [ ! -z "$TEXTlist" ]  && [ "${SENDTEXT^^}" == "TRUE" ]
    then
	echo "Text messaging to: ${TEXTlist}"
	${UNIXMAILPROGRAM} -s "$SUBJECT" ${TEXTlist} < ${BODY} 
    fi
    
    # Remove any temp files
    if [ -f ${VARdir}/body.$$ ]; then rm -f ${VARdir}/body.$$; fi
}

# ----------------------------------------------------------- 
# ******************************* 
# ********* End of File ********* 
# ******************************* 
