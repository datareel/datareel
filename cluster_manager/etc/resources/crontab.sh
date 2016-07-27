#!/bin/bash
# DR cluster resource script for adding or removing crontabs

. ~/.bashrc

# All crontab resouce scripts must use the following args:
#
# ${1} = CM template file, example:  /etc/drcm/crontabs/cm_system_crons
# ${2} = crontab install location, example: /etc/cron.d
# ${3} = actions: start|stop

error_level=0

if [ -z ${1} ]; then
    echo "ERROR - You must specify a template_file"
    error_level=1;
fi

if [ -z ${2} ]; then
    echo "ERROR - You must specify an install_location"
    error_level=1;
fi

if [ -z ${3} ]; then
    echo "ERROR - You must specify an action"
    error_level=1;
fi

if [ $error_level -ne 0 ]; then
    echo "Usage: ${0} template_file install_location start|stop"
    exit $error_level
fi

case "${3}" in
    start)
	cp -fv ${1} ${2}/$(basename ${1})
	if [ ! -f ${2}/$(basename ${1}) ]; then
	    echo "ERROR - Cannot copy ${1} to ${2}"
	    error_level=1;
	else
	    echo "Installed ${2}/$(basename ${1}) crontab"
	    touch ${2}/$(basename ${1})
	    touch /etc/crontab
	fi
        ;;
    stop)
	if [ -f ${2}/$(basename ${1}) ]; then
	    rm -fv ${2}/$(basename ${1})
	    if [ -f ${2}/$(basename ${1}) ]; then
		echo "ERROR - Cannot remove ${2}"
		error_level=1;
	    else
		echo "Removed ${2}/$(basename ${1}) crontab"
		touch /etc/crontab
	    fi
	fi
        ;;
    *)
        echo "ERROR - Action not valid ${3}"
        exit 1
esac

exit $error_level

# End of resource script
