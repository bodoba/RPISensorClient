#!/bin/sh 

build_host=orangepi


git commit . -m "update" ; git push && ssh -x $build_host "(cd RPISensorClient && git pull && cmake . && make )"
