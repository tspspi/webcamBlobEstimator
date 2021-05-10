#!/bin/sh

rm peaks.dat
rm test*.jpg
rm test*.dat
../../bin/webcamBlobEstimator /dev/video2 test 0 500000000 1000000 10 10.21.0.10
