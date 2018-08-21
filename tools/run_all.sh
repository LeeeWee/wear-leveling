#!/bin/sh
WORK_DIR=~/Workspace/Projects/wear-leveling
WEARCOUNT_DIR=${WORK_DIR}/tools/wearcount
MEMTRACKER_DIR=${WORK_DIR}/tools/memtracker
PLOTWRITES_DIR=${WORK_DIR}/tools/plot_memory_writes
OUTPUT_DIR=${WORK_DIR}/output

MALLOC_FLAG='-DWALLOC'
MEMOPS_FILE=${OUTPUT_DIR}/MemOpCollector/mibench/network_dijkstra.memops
WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/wearcount.out
ALLOC_DISTRIBUTION_OUTFILE=${OUTPUT_DIR}/wearcount/alloc_distribution.out
MEMWRITES_PLOT_FIGURE=${WORK_DIR}/output/wearcount/mibench/network_dijkstra_walloc.png
ALLOC_DISTRIBUTION_PLOT_FIGURE=${WORK_DIR}/output/wearcount/mibench/network_dijkstra_walloc_distribution.png

#cd wearcount_dir
echo cd ${WEARCOUNT_DIR}
cd ${WEARCOUNT_DIR}

#compile memory_realloc
make MALLOC_FLAG=${MALLOC_FLAG}

# #cd memtracker dir
# echo cd ${MEMTRACKER_DIR}
# cd ${MEMTRACKER_DIR}
# make

# #execute memory_realloc and collect the metadata writes
# echo pin -t obj-intel64/metadata_writes.so -- ${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE}
# pin -t obj-intel64/metadata_writes.so -- ${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE}

# execute memory_realoc
echo memory reallocating...
${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE} 

# execute wearcount
# echo cd ${WEARCOUNT_DIR}
# cd ${WEARCOUNT_DIR}
${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE}

# execute allocate_dustribution
${WEARCOUNT_DIR}/allocate_distribution

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
python3 plot_memory_writes.py -i ${WEARCOUNT_OUTFILE} -o ${MEMWRITES_PLOT_FIGURE}
echo python3 plot_memory_writes.py -i ${ALLOC_DISTRIBUTION_OUTFILE} -o ${ALLOC_DISTRIBUTION_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${ALLOC_DISTRIBUTION_OUTFILE} -o ${ALLOC_DISTRIBUTION_PLOT_FIGURE}





