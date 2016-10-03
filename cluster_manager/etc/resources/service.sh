#!/bin/bash
# DR cluster resource script for watching cluster services

. ~/.bashrc

# All service resouce scripts must use the following args:
#
# ${1} = Service: httpd
# ${2} = Actions: start|stop|restart|watch

error_level=0

if [ -z ${1} ]; then
    echo "ERROR - You must specify a service name"
    error_level=1;
fi

if [ -z ${2} ]; then
    echo "ERROR - You must specify an action"
    error_level=1;
fi

if [ $error_level -ne 0 ]; then
    echo "Usage: ${0} service start|stop|restart|watch"
    exit $error_level
fi

is_active=0
is_systemd=1
if [ ! -d /etc/systemd ]; then is_systemd=0; fi

function IsActive() {
    is_active=0;
    if [ $is_systemd -eq 1 ]; then
	systemctl status ${servicename} | grep 'Active:' | grep ' active' &> /dev/null
    else
	service ${servicename} status | grep -i "is running" &> /dev/null
    fi
    if [ $? -eq 0 ]; then is_active=1; fi
}

function IsDead() {
    if [ $is_systemd -eq 1 ]; then
	systemctl status ${servicename} | grep 'Active:' | grep -E " inactive|failed" &> /dev/null
    else
	service ${servicename} status | grep -i "is stopped" &> /dev/null
    fi
    if [ $? -eq 0 ]; then 
	is_active=0 
    else
	is_active=1 
    fi
}

function Start() {
    error_level=0
    IsActive
    if [ $is_active -eq 0 ]; then
	if [ $is_systemd -eq 1 ]; then
	    systemctl start ${servicename} &> /dev/null
	else
	    service ${servicename} start &> /dev/null
	fi
	if [ $? -ne 0 ]; then error_level=1; fi 
    fi

}

function Stop() {
    error_level=0
    IsDead
    if [ $is_active -eq 1 ]; then
	if [ $is_systemd -eq 1 ]; then
	    systemctl stop ${servicename} &> /dev/null
	else
	    service ${servicename} stop &> /dev/null
	fi
	if [ $? -ne 0 ]; then error_level=1; fi 
    fi
}

function Restart() {
    error_level=0
    IsActive
    if [ $is_active -eq 1 ]; then
	if [ $is_systemd -eq 1 ]; then
	    systemctl restart ${servicename} &> /dev/null
	else
	    service ${servicename} restart &> /dev/null
	fi
	if [ $? -ne 0 ]; then error_level=1; fi 
    else
	Start
    fi
}

function Reload() {
    error_level=0
    IsActive
    if [ $is_active -eq 1 ]; then
	if [ $is_systemd -eq 1 ]; then
	    systemctl reload ${servicename} &> /dev/null
	else
	    service ${servicename} reload &> /dev/null
	fi
	if [ $? -ne 0 ]; then error_level=1; fi 
    else
	Start
    fi
}

function Watch() {
    error_level=0
    IsDead
    if [ $is_active -eq 0 ]; then
	if [ $is_systemd -eq 1 ]; then
	    systemctl start ${servicename} &> /dev/null
	else
	    service ${servicename} start &> /dev/null
	fi
	if [ $? -ne 0 ]; then error_level=1; fi 
    fi
}

servicename="${1}"
case "${2}" in
    start)
	echo "Starting ${servicename}"
	Start
	if [ $error_level -ne 0 ]; then echo "ERROR - Could not start ${servicename}"; fi
        ;;
    stop)
	echo "Stopping ${servicename}"
	Stop
	if [ $error_level -ne 0 ]; then echo "ERROR - Could not stop ${servicename}"; fi
        ;;
    restart)
	echo "Restarting ${servicename}"
	Restart
	if [ $error_level -ne 0 ]; then echo "ERROR - Could not restart ${servicename}"; fi
        ;;
    reload)
	echo "Reloading ${servicename}"
	Restart
	if [ $error_level -ne 0 ]; then echo "ERROR - Could not reload ${servicename}"; fi
        ;;
    watch)
	# Cluster manager checking ${servicename}
	Watch
	if [ $error_level -ne 0 ]; then 
	    echo "ERROR - ${servicename} has stopped and cluster manager could not restart"
	fi
        ;;
    status)
	if [ $is_systemd -eq 1 ]; then
	    systemctl status ${servicename}
	else
	    service ${servicename} status
	fi
        ;;


    *)
        echo "ERROR - Action not valid ${2}"
        exit 1
esac

exit $error_level

# End of resource script
