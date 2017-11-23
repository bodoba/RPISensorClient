#!/bin/sh 

svn ci -m "" && ssh -x david "(cd RPI/DHT11 && svn up && cmake . && make )"
