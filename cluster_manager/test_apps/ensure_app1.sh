#!/bin/bash

pid=$(ps -ef | grep test_app1.sh | grep -v grep | awk '{ print $2 }')
if [ -z $pid ]; then ${HOME}/test_apps/start_app1.sh; fi
exit 0
