import sys
import math

def read_memory_writes_file(filepath):
    metadata_writes, data_writes, allocate_distribution = [], [], []
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
                data_writes.append(int(count))
        
        elif first_line[0:12] == "distribution":
            for line in f:
                if line == "\n":
                    break
                count = line.split(",")[1]
                allocate_distribution.append(int(count))

        else:
            print("wrong format!")

    return metadata_writes, data_writes, allocate_distribution


def calculate_cov_and_mdf(write_frequency):
    if len(write_frequency) == 0:
        print("Input writes list is empty")
    else:
        max, sum, cov = 0, 0, 0
        for write_count in write_frequency:
            if write_count > max:
                max = write_count
            sum += write_count
        
        average = float(sum)  / len(write_frequency)

        for write_count in write_frequency:
            cov += (write_count - average) * (write_count - average) 
        cov = (1 / average) * math.sqrt(cov / (len(write_frequency) - 1))

        mdf = float(max) / average

        print("coefficient_of_variation: " + str(cov))
        print("maximum_deviation_factor: " + str(mdf))



if __name__ == '__main__':
    if (len(sys.argv) < 2):
        print("usage: python3 plot_memory_writes.py -i input_path")
        exit(1)
    
    input = ""
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "-i":
            i += 1
            input = sys.argv[i]
        i += 1

    print("input: " + input)

    metadata_writes, data_writes, allocate_distribution = read_memory_writes_file(input)

    if len(data_writes) != 0:
        print("data writes: ")
        calculate_cov_and_mdf(data_writes)
    
    if len(allocate_distribution) != 0:
        print("allocate distribution: ")
        calculate_cov_and_mdf(allocate_distribution)