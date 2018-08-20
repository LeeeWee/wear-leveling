build: memory_realloc.cpp
	g++ -g -O0 -o memory_realloc memory_realloc.cpp ~/Workspace/Projects/wear-leveling/tools/MemOpCollector/memop_collector.o ~/Workspace/Projects/wear-leveling/malloc/basemalloc/basemalloc.o \
	~/Workspace/Projects/wear-leveling/malloc/galloc/galloc.cpp \
	-I ~/Workspace/Tools/boost_1_67_0/ \
	-I ~/Workspace/Projects/wear-leveling/malloc \
	-I ~/Workspace/Projects/wear-leveling/malloc/nvmalloc/include \
	-I ~/Workspace/Projects/wear-leveling/malloc/walloc/include \
	-L ~/Workspace/Projects/wear-leveling/malloc/nvmalloc/lib \
	-L ~/Workspace/Projects/wear-leveling/malloc/walloc/lib \
	-I ~/Workspace/Projects/wear-leveling/tools/MemOpCollector \
	-L ~/Workspace/Tools/boost_1_67_0/stage/lib/ \
	-lboost_serialization -lnvmalloc -lwalloc -static $(MALLOC_FLAG)

	g++ -g -o wearcount wearcount.cpp ~/Workspace/Projects/wear-leveling/tools/MemOpCollector/memop_collector.o ~/Workspace/Projects/wear-leveling/malloc/basemalloc/basemalloc.o \
	~/Workspace/Projects/wear-leveling/malloc/galloc/galloc.cpp \
	-I ~/Workspace/Tools/boost_1_67_0/ \
	-I ~/Workspace/Projects/wear-leveling/malloc \
	-I ~/Workspace/Projects/wear-leveling/malloc/nvmalloc/include \
	-I ~/Workspace/Projects/wear-leveling/tools/MemOpCollector \
	-L ~/Workspace/Tools/boost_1_67_0/stage/lib/ \
	-lboost_serialization -static -std=c++11
clean:
	rm memory_realloc wearcount