#!/bin/sh
BENCHMARK_DIR=~/Workspace/Projects/wear-leveling/benchmark/mibench
MALLOCTRACE_OUTDIR=~/Workspace/Projects/wear-leveling/output/memtracker/memtrack-out/mibench

PINTOOL=~/Workspace/Projects/wear-leveling/tools/memtracker/obj-intel64/memtracker.so

# if MALLOCTRACE_OUTDIR exists, delete and mkdir again
[ -d "$MALLOCTRACE_OUTDIR" ] && rm -r "$MALLOCTRACE_OUTDIR"
mkdir "$MALLOCTRACE_OUTDIR"


# output malloctrace for input program
malloctrace() {
	echo $1
	outname=${1:${#BENCHMARK_DIR}+1} # get file name
	outname=${outname//\//_}
	suffix=.memtrack
	# out=${MALLOCTRACE_OUTDIR}/${outname%_runme_large.sh}
	out=${MALLOCTRACE_OUTDIR}/${outname%_runme_small.sh}
	
	# read content in runme_large.sh
	i=0
	while read line
	do
		if [ $i -gt 0 ]; then # ignore the first line
			# outfile=${out}$i${suffix}
			outfile=${out}${suffix}
			echo "pin -t memtracker.so -o $outfile -- $line"
			pin -t $PINTOOL -o $outfile -- $line
		fi
	i=$((i+1))	
	done < $1
}

# iterate all dirs and find all runme_large and runme_small file
check_alldir() {
	for file in `ls -A $1`
	do 
		if [ -d "$1/$file" ]; then
			cd $1/$file
			# if [ -e "runme_large.sh" ]; then
			# 	echo "$1/$file"
			# 	runme_large=$1/$file/runme_large.sh
			# 	malloctrace $runme_large
			# fi

			if [ -e "runme_small.sh" ]; then
				echo "$1/$file"
				runme_small=$1/$file/runme_small.sh
				malloctrace $runme_small
			fi

			check_alldir "$1/$file"

		fi
	done			
}

check_alldir $BENCHMARK_DIR 
