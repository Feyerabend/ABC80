#!/bin/sh
V=/Users/setlonnert/Documents/abc80/utanfor/tolkjamforelse/../../dis/04/host/abc80host
r=$2; shift 2
case "$*" in *@5000*) K= ;; *) K="-l /Users/setlonnert/Documents/abc80/utanfor/tolkjamforelse/ut/a_alla.krets@5000" ;; esac
case $r in
    11273)     exec "$V" -r /Users/setlonnert/Documents/abc80/utanfor/tolkjamforelse/ut/a_alla.rom $K "$@" ;;
    basicii80) exec "$V" -r /Users/setlonnert/Documents/abc80/utanfor/tolkjamforelse/ut/b_alla.rom "$@" ;;
    *)         exec "$V" -r $r "$@" ;;
esac
