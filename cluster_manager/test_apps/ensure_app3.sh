#!/bin/bash

pid=$(ps -ef | grep test_app3.sh | grep -v grep | awk '{ print $2 }')
if [ -z $pid ]; then ${HOME}/test_apps/start_app3.sh; fi
exit 0
