import matplotlib.pyplot as plt

def read_memory_writes_file(filepath):
    metadata_writes, data_writes = [], []
    with open(filepath, "r") as f:
        metadata_line = f.readline() # skip the first line, "metadata:"
        if metadata_line[0:8] != "metadata":
            print("wrong metadata line!")
        for line in f:
            if line == "\n":
                break
            count = line.split(",")[1]
            metadata_writes.append(int(count))

        data_line = f.readline() # skip the line, "data:"
        if data_line[0:4] != "data":
            print("wrong data line!")
        for line in f:
            if line == "\n":
                break
            count = line.split(",")[1]
            data_writes.append(int(count))
    return metadata_writes, data_writes


if __name__ == '__main__':
    filepath = "/home/liwei/Workspace/Projects/wear-leveling/output/wearcount/wearcount.out"
    metadata_writes, data_writes = read_memory_writes_file(filepath)
    merge_writes = [mw + dw for mw, dw in zip(metadata_writes, data_writes)]
    print("metadata_writes count: " + str(len(metadata_writes)) + "\n")
    print("data_writes count: " + str(len(data_writes)) + "\n")

    # plot
    plt.plot(range(len(metadata_writes)), metadata_writes, color='black', label='metadata writes')
    plt.plot(range(len(merge_writes)), merge_writes, color='red', label='data and metadata writes')
    plt.xlabel('block number')
    plt.ylabel('number of writes(in 64 bytes)')
    plt.show()