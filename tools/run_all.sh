#!/bin/sh
WORK_DIR=~/Workspace/Projects/wear-leveling
WEARCOUNT_DIR=${WORK_DIR}/tools/wearcount
MEMTRACKER_DIR=${WORK_DIR}/tools/memtracker
PLOTWRITES_DIR=${WORK_DIR}/tools/plot_memory_writes

MALLOC_FLAG='-DALLOC_NAME=ffmalloc -DFREE_NAME=basefree'
MEMOPS_FILE=${WORK_DIR}/output/MemOpCollector/mibench/network_patricia.memops
MEMWRITES_PLOT_FIGURE=${WORK_DIR}/output/wearcount/mibench/network_patricia_ffmalloc.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile memory_realloc
make MALLOC_FLAG=${MALLOC_FLAG}

#cd memtracker dir
echo cd ${MEMTRACKER_DIR}
cd ${MEMTRACKER_DIR}
make

#execute memory_realloc and collect the metadata writes
echo pin -t obj-intel64/metadata_writes.so -- ${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE}
pin -t obj-intel64/metadata_writes.so -- ${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE}

#execute wearcount
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}
${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE}

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
python3 plot_memory_writes.py ${MEMWRITES_PLOT_FIGURE}





