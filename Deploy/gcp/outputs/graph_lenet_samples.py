# importing the required module
import matplotlib.pyplot as plt
import sys
import os
import re

if len(sys.argv) != 3:
    print("error: run python3 graph_lenet_samples.py <input_file> <churn_percentage> .. ")
    sys.exit(-1)

arg_file = sys.argv[1]
churn_percentage = sys.argv[2]

data_time = []
data_samples = []

def clear_output(path):
    time_taken = 0
    tmp_lines = []
    with open(path) as f:
        lines = f.readlines()
        for l in lines:
            if re.search("#.*BenchmarkMetric:.*", l):
                continue
            elif re.search(".*BenchmarkMetric:.*", l):
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
ax.axhline(y = mean_of_samples_per_second, xmin = 0, xmax = data_time[-1], linestyle = (5, (10, 3)), linewidth=1.5, label = "débito médio", color="slategray")
ax.annotate(str(round(mean_of_samples_per_second,1)), xy=(8840,97.2), color="slategray")

def churn_lines(fail_data, recover_data):
    i = 0
    for data in fail_data:
        if i == 0: 
            ax.axvline(x = data, linestyle = 'dashed', dashes=(3, 5), linewidth=1.7, color = 'red', label = 'introdução de falhas')
        else:
            ax.axvline(x = data, linestyle = 'dashed', dashes=(3, 5), linewidth=1.7, color = 'red')
        i = i + 1

    i = 0
    for data in recover_data:
        if i == 0: 
            ax.axvline(x = data, linestyle = 'dashed', dashes=(3, 5), linewidth=1.7, color = 'darkgreen', label = 'entrada de nodos')
        else:
            ax.axvline(x = data, linestyle = 'dashed', dashes=(3, 5), linewidth=1.7, color = 'darkgreen')
        i = i + 1
    


fail_data_1=[1920,3720,5520,7320]
recover_data_1=[2280,4080,5820,7680]

fail_data_3=[1800,3600,5400,7260]
recover_data_3=[2160,3960,5820,7680]

fail_data_5=[1800,3600,5460,7200]
recover_data_5=[2220,4020,5880,7800]

graph_title='Desempenho - '

if churn_percentage == '1':
    churn_lines(fail_data_1, recover_data_1)
    graph_title = graph_title + '1% de falhas'
elif churn_percentage == '3':
    churn_lines(fail_data_3, recover_data_3)
    graph_title = graph_title + '3% de falhas'
elif churn_percentage == '5':
    churn_lines(fail_data_5, recover_data_5)
    graph_title = graph_title + '5% de falhas'
else:
    print("Invalid Churn Percentage!")


# ax.margins(tight=True)
ax.legend()

# naming the x axis
plt.xlabel('Tempo (s)')
# naming the y axis
plt.ylabel('Débito (imagens/s)')

# giving a title to my graph
plt.title(graph_title)

# function to show the plot
plt.show()