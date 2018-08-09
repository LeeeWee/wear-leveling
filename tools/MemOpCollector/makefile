build : MemOpCollector.h MemOpCollector.cpp 
	gcc -g -c -o memop_collector.o MemOpCollector.cpp -I ~/Workspace/Tools/boost_1_67_0/ -L ~/Workspace/Tools/boost_1_67_0/stage/lib/ -lboost_serialization -static
	g++ -g -o CollectMemOp collect_test.cpp memop_collector.o -I ~/Workspace/Tools/boost_1_67_0/ -L. -L ~/Workspace/Tools/boost_1_67_0/stage/lib/ -lboost_serialization -static
	g++ -g -o alloc_size_distribution.o alloc_size_distribution.cpp memop_collector.o -I ~/Workspace/Tools/boost_1_67_0/ -L. -L ~/Workspace/Tools/boost_1_67_0/stage/lib/ -lboost_serialization -static
clean : 
	rm memop_collector.o collect_test
