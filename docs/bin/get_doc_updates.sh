#!/bin/bash

echo "Updating local doc files using contect from https://datareel.com/clib/"

echo "Updating example programs"
files=$(ls -1rat --color=none ../examples/database/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/examples/database/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../examples/general/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/examples/general/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../examples/socket/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/examples/socket/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../examples/thread/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/examples/thread/${fname}?format=text $f
done

echo "Updating classes"
php write_url_to_html.php https://datareel.com/clib/classes/irefs.php?format=text ../classes/irefs.htm
php write_url_to_html.php https://datareel.com/clib/classes/refs.php?format=text ../classes/refs.htm

files=$(ls -1rat --color=none ../classes/database/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/classes/database/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../classes/general/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/classes/general/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../classes/socket/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/classes/socket/${fname}?format=text $f
done

files=$(ls -1rat --color=none ../classes/thread/*.htm)
for f in ${files}; do
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/classes/thread/${fname}?format=text $f
done

echo "Updating utils"
files=$(ls -1rat --color=none ../utils/*.htm)
for f in ${files}; do
    if [ "$(basename $f)" == "utils.htm" ]; then continue; fi
    if [ "$(basename $f)" == "p_lchild.htm" ]; then continue; fi
    if [ "$(basename $f)" == "p_rchild.htm" ]; then continue; fi
    fname=$(basename $f | sed s/'.htm$'/'.php'/g)
    php write_url_to_html.php https://datareel.com/clib/utils/${fname}?format=text $f
done
