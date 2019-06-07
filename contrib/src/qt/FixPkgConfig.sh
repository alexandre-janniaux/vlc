pattern="\([^[:space:]]*\)/lib\([^[:space:]]*\)\.so"
replace="-L\1/ -l\2"
sed -i.orig -e "s|$pattern|$replace|g" $*
