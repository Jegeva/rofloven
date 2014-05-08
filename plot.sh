#! /bin/sh

cat $1 | tr -d '\r\0' | grep -v '^$' | grep -v eflo > data.csv
gnuplot --persist data.plot
