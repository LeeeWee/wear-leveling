#!/bin/sh
WORK_DIR=~/Workspace/Projects/wear-leveling
WEARCOUNT_DIR=${WORK_DIR}/tools/wearcount
PLOTWRITES_DIR=${WORK_DIR}/tools/plot_memory_writes
OUTPUT_DIR=${WORK_DIR}/output


########################################  DGLIBC_MALLOC  ###################################################
MALLOC_FLAG='-DGLIBC_MALLOC'
WORKLOAD=${WEARCOUNT_DIR}/workload
WORKLOAD_WEARCOUNT=${WEARCOUNT_DIR}/workload_wearcount
WORKLOAD_WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/workload_wearcount.out
WORKLOAD_WEARCOUNT_PLOT_FIGURE=${WORK_DIR}/output/wearcount/workload/workload_wearcount_malloc.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile workload and workload_wearcount
make MALLOC_FLAG=${MALLOC_FLAG}

# execute workload
echo workload
${WORKLOAD}

# execute workload_wearcount
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo WORKLOAD_WEARCOUNT -malloc
    ${WORKLOAD_WEARCOUNT} -malloc 
else 
    echo WORKLOAD_WEARCOUNT
    ${WORKLOAD_WEARCOUNT}
fi

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
echo python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}



########################################  WALLOC  ###################################################
MALLOC_FLAG='-DWALLOC'
WORKLOAD=${WEARCOUNT_DIR}/workload
WORKLOAD_WEARCOUNT=${WEARCOUNT_DIR}/workload_wearcount
WORKLOAD_WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/workload_wearcount.out
WORKLOAD_WEARCOUNT_PLOT_FIGURE=${WORK_DIR}/output/wearcount/workload/workload_wearcount_walloc.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile workload and workload_wearcount
make MALLOC_FLAG=${MALLOC_FLAG}

# execute workload
echo workload
${WORKLOAD}

# execute workload_wearcount
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo WORKLOAD_WEARCOUNT -malloc
    ${WORKLOAD_WEARCOUNT} -malloc 
else 
    echo WORKLOAD_WEARCOUNT
    ${WORKLOAD_WEARCOUNT}
fi

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
echo python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}


########################################  NVMALLOC  ###################################################
MALLOC_FLAG='-DNVMALLOC'
WORKLOAD=${WEARCOUNT_DIR}/workload
WORKLOAD_WEARCOUNT=${WEARCOUNT_DIR}/workload_wearcount
WORKLOAD_WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/workload_wearcount.out
WORKLOAD_WEARCOUNT_PLOT_FIGURE=${WORK_DIR}/output/wearcount/workload/workload_wearcount_nvmalloc.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile workload and workload_wearcount
make MALLOC_FLAG=${MALLOC_FLAG}

# execute workload
echo workload
${WORKLOAD}

# execute workload_wearcount
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo WORKLOAD_WEARCOUNT -malloc
    ${WORKLOAD_WEARCOUNT} -malloc 
else 
    echo WORKLOAD_WEARCOUNT
    ${WORKLOAD_WEARCOUNT}
fi

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
echo python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}


########################################  WALLOC  ###################################################
MALLOC_FLAG='-DNEWALLOC'
WORKLOAD=${WEARCOUNT_DIR}/workload
WORKLOAD_WEARCOUNT=${WEARCOUNT_DIR}/workload_wearcount
WORKLOAD_WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/workload_wearcount.out
WORKLOAD_WEARCOUNT_PLOT_FIGURE=${WORK_DIR}/output/wearcount/workload/workload_wearcount_newalloc.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile workload and workload_wearcount
make MALLOC_FLAG=${MALLOC_FLAG}

# execute workload
echo workload
${WORKLOAD}

# execute workload_wearcount
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo WORKLOAD_WEARCOUNT -malloc
    ${WORKLOAD_WEARCOUNT} -malloc 
else 
    echo WORKLOAD_WEARCOUNT
    ${WORKLOAD_WEARCOUNT}
fi

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
echo python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${WORKLOAD_WEARCOUNT_OUTFILE} -o ${WORKLOAD_WEARCOUNT_PLOT_FIGURE}







