#!/bin/sh

OWNNET="10.21.0.1/24"
SSG="10.21.0.10"

SSG_FRQ="/usr/home/tsp/githubRepos/rawsockscpitools/bin/siglentssg3021x_setfrq"
SSG_POW="/usr/home/tsp/githubRepos/rawsockscpitools/bin/siglentssg3021x_setpow"
SSG_RFON="/usr/home/tsp/githubRepos/rawsockscpitools/bin/siglentssg3021x_rfonoff"

VDEV="/dev/video2"

FRQSTART=1000000
FRQEND=501000000
FRQSTEP=1000000

SSGPOW=10

# Configure our network ...

sudo ifconfig ue0 ${OWNNET}

# Check if the SSG is availalbe

ping -c4 ${SSG} > /dev/null 2>/dev/null
if [ ! $? -eq 0 ]; then
	echo "SSG not available, aborting"
	return 1
fi

echo "Setting up SSG"
echo "   Frequency: ${FRQSTART} Hz"
${SSG_FRQ} ${SSG} ${FRQSTART}
sleep 2
echo "   Power: ${SSGPOW} dBm"
${SSG_POW} ${SSG} ${SSGPOW}
sleep 2

echo "Reference measurement (RF off)"
${SSG_RFON} ${SSG} off
sleep 2
../../bin/webcamBlobEstimator ${VDEV} dat0 > peaks.dat
cp dat0-raw.jpg current-raw.jpg
cp dat0-cluster.jpg current-cluster.jpg
okular current-raw.jpg &
PIDDISPLAYRAW=$!
okular current-cluster.jpg &
PIDDISPLAYCLUSTER=$!
sleep 5

echo "RF on"
${SSG_RFON} ${SSG} on
sleep 2

FRQ=${FRQSTART}
while [ ${FRQ} -lt ${FRQEND} ]; do
	echo "Frequency: ${FRQ} Hz"
	${SSG_FRQ} ${SSG} ${FRQ}
	sleep 2

	../../bin/webcamBlobEstimator ${VDEV} dat${FRQ} >> peaks.dat
	cp dat${FRQ}-raw.jpg current-raw.jpg
	cp dat${FRQ}-cluster.jpg current-cluster.jpg

	FRQ=`expr ${FRQ} + ${FRQSTEP}`
done

echo "RF off"
${SSG_RFON} ${SSG} off

kill ${PIDDISPLAYRAW}
kill ${PIDDISPLAYCLUSTER}
