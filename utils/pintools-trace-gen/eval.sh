#!/bin/bash

PIN=/home/gvaumour/Dev/apps/pin/pin-2.14-71313-gcc.4.4.7-linux/pin
#zlib_flux= /home/gvaumour/Dev/apps/traces-pintools/zlib_flux/convert_flux

$PIN -ifeellucky -injection child -t ./obj-intel64/roeval.so -- ./test/helloWorld/test
