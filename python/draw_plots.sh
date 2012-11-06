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
	echo "USAGE: $0 dut_id [...]"
	exit 1
fi

shopt -s extglob

for DUTRAW in "$@"; do

	DUT=`echo "$DUTRAW"|sed -s 's/.*\///'`

gnuplot -p << EOF
set term wxt 0
set xlabel "f [MHz]"
set ylabel "Pout [dBm]"
set title "$DUT - Frequency sweep"
set grid
plot \
`template "log/${DUT}_freq_sweep_*dbm.log" '\"${FILE}\" using \(\\$1/1e6\):2 title \"Pin = ${NUM} dBm\",'` \
1/0 notitle lt -3 

set term wxt 1
set xlabel "Pin [dBm]"
set ylabel "Pout [dBm]"
set title "$DUT - Power ramp"
plot \
`template "log/${DUT}_power_ramp_*hz.log" '\"${FILE}\" title \"f = ${NUM} Hz\",'` \
x notitle lt 0 

set term wxt 2
set xlabel "f [kHz]"
set ylabel "Pout [dBm]"
set title "$DUT - Channel filter"
plot \
`template "log/${DUT}_channel_filter_*([0-9])hz.log" '\"${FILE}\" using \(\(\\$1-${NUM}\)/1e3\):2 title \"f = ${NUM} Hz\",'` \
1/0 notitle lt -3 

set term wxt 3
set xlabel "t [samples]"
set ylabel "Pout [dBm]"
set title "$DUT - Settle time"
plot \
`template "log/${DUT}_settle_time_m10dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 10 dBm, f = ${NUM} Hz\",'` \
`template "log/${DUT}_settle_time_m50dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 50 dBm, f = ${NUM} Hz\",'` \
`template "log/${DUT}_settle_time_m90dbm_*([0-9])hz.log" '\"${FILE}\" title \"Pin = 90 dBm, f = ${NUM} Hz\",'` \
1/0 notitle lt -3 
EOF

done
