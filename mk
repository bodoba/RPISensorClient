#!/bin/sh 

build_host=piorange


git commit . -m "update" ; git push && ssh -x $build_host "(cd RPISensorClient && git pull && cmake . && make )"
