# importing the required module
import matplotlib.pyplot as plt
import sys
import os
import re

if len(sys.argv) != 2:
    print("error: run python3 graphdraw.py <output_file> .. ")
    sys.exit(-1)

arg_file = sys.argv[1]

data_time = []
data_samples = []

def clear_output(path):
    time_taken = 0
    tmp_lines = []
    with open(path) as f:
        lines = f.readlines()
        for l in lines:
            if re.search(".*BenchmarkMetric:.*", l):
                tmp_lines.append(l)
    
    for l in tmp_lines:
        val = re.match(".*time_taken\': ([0-9]*.[0-9]*),\'examples_per_second\': ([0-9]*.[0-9]*)", l)
        time_taken = time_taken + float(val.group(1))
        data_time.append(time_taken)
        data_samples.append(float(val.group(2)))


clear_output(arg_file)

sum_of_samples_per_second = sum(data_samples)
mean_of_samples_per_second = sum_of_samples_per_second/len(data_samples)
print(mean_of_samples_per_second)

fig, ax = plt.subplots()

# plotting the points 
ax.plot(data_time, data_samples, label = "débito")

# plotting the points 
ax.axhline(y = mean_of_samples_per_second, xmin = 0, xmax = data_time[-1], linestyle = 'dashed', label = "débito médio", color="red")
# ax.margins(tight=True)
ax.annotate(str(round(mean_of_samples_per_second,1)), xy=(-7,959), color="red")
ax.legend()
# naming the x axis
plt.xlabel('Tempo (s)')
# naming the y axis
plt.ylabel('Débito (imagens/s)')

# giving a title to my graph
plt.title('Desempenho Local')

# function to show the plot
# plt.show()