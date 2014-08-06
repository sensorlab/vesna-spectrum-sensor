#!/bin/bash

function template {
	GLOB=$1
	TEMPLATE=$2

	for FILE in $GLOB; do
		NUM=`echo $FILE|sed 's/.*_\(m\?[0-9]\+\)[a-z]\+.log/\1/;s/^m/-/'`
		echo -n `eval echo $TEMPLATE`
	done
}

if [ "$#" -lt 1 ]; then
	echo "USAGE: $0 prefix [...]"
	exit 1
fi

shopt -s extglob

for DUT in "$@"; do

	MAINLOG="$DUT.log"
	if [ ! -e "$MAINLOG" ]; then
		echo "$0: $MAINLOG does not exist" >&2
		continue
	fi

gnuplot -p << EOF
set term wxt 0
set xlabel "f [MHz]"
set ylabel "Pout [dBm]"
set title "$DUT - Frequency sweep"
set grid
plot \
`template "${DUT}_freq_sweep_*dbm.log" '\"${FILE}\" using \(\\$1/1e6\):2 title \"Pin = ${NUM} dBm\",'` \
1/0 notitle lt -3 

set term wxt 1
set xlabel "Pin [dBm]"
set ylabel "Pout [dBm]"
set title "$DUT - Power ramp"
plot \
`template "${DUT}_power_ramp_*hz.log" '\"${FILE}\" title \"f = ${NUM} Hz\",'` \
x notitle lt 0 

set term wxt 2
set xlabel "f [kHz]"
set ylabel "Pout [dBm]"
set title "$DUT - Channel filter"
plot \
`template "${DUT}_channel_filter_*([0-9])hz.log" '\"${FILE}\" using \(\(\\$1-${NUM}\)/1e3\):2 title \"f = ${NUM} Hz\",'` \
1/0 notitle lt -3 

set term wxt 3
set xlabel "t [samples]"
set ylabel "Pout [dBm]"
set title "$DUT - Settle time"
plot \
`template "${DUT}_settle_time_m10dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 10 dBm, f = ${NUM} Hz\",'` \
`template "${DUT}_settle_time_m50dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 50 dBm, f = ${NUM} Hz\",'` \
`template "${DUT}_settle_time_m90dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 90 dBm, f = ${NUM} Hz\",'` \
1/0 notitle lt -3 
EOF

done
