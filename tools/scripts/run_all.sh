#!/bin/sh
WORK_DIR=~/Workspace/Projects/wear-leveling
WEARCOUNT_DIR=${WORK_DIR}/tools/wearcount
MEMTRACKER_DIR=${WORK_DIR}/tools/memtracker
PLOTWRITES_DIR=${WORK_DIR}/tools/plot_memory_writes
OUTPUT_DIR=${WORK_DIR}/output

MALLOC_FLAG='-DGLIBC_MALLOC'
MEMOPS_FILE=${OUTPUT_DIR}/MemOpCollector/mediabench2/jpg2000enc.memops
WEARCOUNT_OUTFILE=${OUTPUT_DIR}/wearcount/wearcount.out
ALLOC_DISTRIBUTION_OUTFILE=${OUTPUT_DIR}/wearcount/alloc_distribution.out
MEMWRITES_PLOT_FIGURE=${WORK_DIR}/output/wearcount/mediabench2/jpg2000enc_malloc.png
ALLOC_DISTRIBUTION_PLOT_FIGURE=${WORK_DIR}/output/wearcount/mediabench2/jpg2000enc_malloc_distribution.png

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
${WEARCOUNT_DIR}/memory_realloc ${MEMOPS_FILE} -rl

# execute wearcount
# echo cd ${WEARCOUNT_DIR}
# cd ${WEARCOUNT_DIR}
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo ${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE} -malloc -rl
    ${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE} -malloc -rl
else 
    echo ${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE} -rl
    ${WEARCOUNT_DIR}/wearcount ${MEMOPS_FILE} -rl
fi

# execute allocate_dustribution
if [ ${MALLOC_FLAG} == "-DGLIBC_MALLOC" ]
then 
    echo ${WEARCOUNT_DIR}/allocate_distribution -malloc
    ${WEARCOUNT_DIR}/allocate_distribution -malloc
else 
    echo ${WEARCOUNT_DIR}/allocate_distribution 
    ${WEARCOUNT_DIR}/allocate_distribution 
fi

#plot memory writes
echo cd ${PLOTWRITES_DIR}
cd ${PLOTWRITES_DIR}
python3 plot_memory_writes.py -i ${WEARCOUNT_OUTFILE} -o ${MEMWRITES_PLOT_FIGURE}
echo python3 plot_memory_writes.py -i ${ALLOC_DISTRIBUTION_OUTFILE} -o ${ALLOC_DISTRIBUTION_PLOT_FIGURE}
python3 plot_memory_writes.py -i ${ALLOC_DISTRIBUTION_OUTFILE} -o ${ALLOC_DISTRIBUTION_PLOT_FIGURE}





