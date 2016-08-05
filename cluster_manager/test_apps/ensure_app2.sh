#!/bin/bash

pid=$(ps -ef | grep test_app2.sh | grep -v grep | awk '{ print $2 }')
if [ -z $pid ]; then ${HOME}/test_apps/start_app2.sh; fi
exit 0
