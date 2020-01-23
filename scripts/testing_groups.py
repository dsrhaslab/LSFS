import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np
from collections import defaultdict
from subprocess import Popen
from string import Template
import json 
import time
import os
import regex as re
import datetime
import random
import shutil
import argparse
import yaml
import time
import pickle
import statistics

parser = argparse.ArgumentParser()
parser.add_argument('-e', help="Include Execution",
                    action='store_true')
parser.add_argument('-a', help="Include Analisis",
                    action='store_true')
parser.add_argument('-r', '--remote', help="Remote Execution",
                    action='store_true')
parser.add_argument('-d', help="Draw final graph",
                    action='store_true')
parser.add_argument("-c", "--config", help="config file to use (conf.yaml default)", default="conf.yaml")
args = vars(parser.parse_args())

with open(args['config'], 'r') as stream:
    conf = yaml.safe_load(stream)

logging_directory = conf['main_confs']['logging_dir']


dirs = [os.path.join(logging_directory, o) for o in os.listdir(logging_directory) 
                  if os.path.isdir(os.path.join(logging_directory,o))]

iterations = []

def sorting(filenames):
   return sorted(filenames, key=lambda x: int(re.findall(r'(\d+)\.json$', x)[0]))

for directory in dirs:
   it = 0
   peer = (re.findall(r'\d+$', directory))[0]
   filenames = [os.path.join(directory, o) for o in os.listdir(directory)]
   
   for filename in sorting(filenames):

      with open(filename, "r") as file:
         try:
            data = json.loads(file.read())
            nr_groups = data['nr_groups']
            if nr_groups == 16:
               iterations.append(it)
               break
         except Exception as e:
            print(str(e))

      it += 1

print(iterations)
print(statistics.mean(iterations))

