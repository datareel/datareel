#!/bin/bash

cat /dev/null > /tmp/test_app3.out

while :
do
    date >> /tmp/test_app3.out
    sleep 1
done
