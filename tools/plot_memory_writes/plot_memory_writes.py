import matplotlib.pyplot as plt
import sys

def read_memory_writes_file(filepath):
    metadata_writes, data_writes = [], []
    with open(filepath, "r") as f:
        first_line = f.readline()
        if first_line[0:8] == "metadata":
            for line in f:
                if line == "\n":
                    break
                count = line.split(",")[1]
                metadata_writes.append(int(count))
            
            data_line = f.readline() # skip the line, "data:"
            if data_line[0:4] != "data":
                print("wrong data line!")
            else:
                for line in f:
                    if line == "\n":
                        break
                    count = line.split(",")[1]
                    data_writes.append(int(count))
        
        elif first_line[0:4] == "data":
            for line in f:
                if line == "\n":
                        break
                count = line.split(",")[1]
                data_writes.append(int(count))\
        
        else:
            print("wrong format!")

    return metadata_writes, data_writes


if __name__ == '__main__':
    filepath = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/wearcount.out"
    if (len(sys.argv) > 1):
        output = sys.argv[1]
    else:
        output = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/mibench/network_dijkstra_ffmalloc.png"

    metadata_writes, data_writes = read_memory_writes_file(filepath)
    # merge_writes = [mw + dw for mw, dw in zip(metadata_writes, data_writes)]
    # print("metadata_writes count: " + str(len(metadata_writes)) + "\n")
    # print("data_writes count: " + str(len(data_writes)) + "\n")

    if (len(metadata_writes) != 0):
        # plot
        fig, axes = plt.subplots(nrows=1, ncols=2, figsize=(9, 4))
        axes[0].plot(range(len(metadata_writes)), metadata_writes, color='black', linewidth=0.8)
        axes[0].set_xlabel('block number')
        axes[0].set_ylabel('number of metadata writes(in 64 bytes)')

        axes[1].plot(range(len(data_writes)), data_writes, color='black', linewidth=0.8)
        axes[1].set_xlabel('block number')
        axes[1].set_ylabel('number of data writes(in 64 bytes)')

        for ax in axes:
            ax.yaxis.grid(True, linestyle='dotted') # y轴添加网格线
            ax.tick_params(direction='in') # 刻度线朝内
        
        plt.show()
        fig.savefig(output)
    
    else:
        fig = plt.figure()
        ax = fig.add_subplot(1, 1, 1)
        plt.plot(range(len(data_writes)), data_writes, color='black', label='metadata writes', linewidth=0.8)
        plt.xlabel('block number')
        plt.ylabel('number of data writes(in 64 bytes)')
        plt.legend() # 显示图例
        # plt.axes().yaxis.grid(True)
        ax.yaxis.grid(True, linestyle='dotted') # y轴添加网格线
        ax.tick_params(direction='in') # 刻度线朝内
        plt.show()
        fig.savefig(output)

    # fig = plt.figure()
    # ax = fig.add_subplot(1, 1, 1)
    # plt.plot(range(len(metadata_writes)), metadata_writes, color='black', label='metadata writes', linewidth=0.8)
    # plt.plot(range(len(merge_writes)), merge_writes, color='red', label='data and metadata writes', linewidth=0.8)
    # plt.xlabel('block number')
    # plt.ylabel('number of writes(in 64 bytes)')
    # plt.legend() # 显示图例
    # # plt.axes().yaxis.grid(True)
    # ax.yaxis.grid(True, linestyle='dotted') # y轴添加网格线
    # ax.tick_params(direction='in') # 刻度线朝内
    # plt.show()
    # fig.savefig(output)