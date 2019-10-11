#!/bin/bash

set -ex

if [ ! -d "../../include" ]; then
	mkdir ../../include
fi

if [ ! -d "../../include/gtaskque" ]; then
	mkdir ../../include/gtaskque
fi

cp *.h ../../include/gtaskque/
