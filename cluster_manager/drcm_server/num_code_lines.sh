#!/bin/bash

FILES1=$(find -name "*.cpp" -print)
FILES2=$(find -name "*.h" -print)
for f in $FILES1 $FILES2
do
    numlines=$(cat ${f} | wc -l)
    let total=total+numlines
done

echo $total
