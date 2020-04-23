import os
import sys
from datetime import datetime
from separate_file import separate_file, remove_files
import numpy as np
import os

file_name = sys.argv[1]
CORES = int(sys.argv[2])
commands = np.array([" ".join(f) for f in np.loadtxt(file_name, dtype='U')])

commands_per_core = np.array_split(np.random.shuffle(commands), CORES)

for i in range(CORES):
    core_command = '('
    for j in range(len(commands_per_core[i])):
        split_files = commands_per_core[i][j].split(">")
        command = split_files[0]
        log_file = split_files[1][:-1]
        err_file = split_files[2]
        core_command+='nohup sh -c "'+command+'">'+log_file[1:]+"2>"+err_file+"; wait;"
    core_command=core_command[:-7]+') &'
    os.system(core_command)