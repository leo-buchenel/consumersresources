import glob
import numpy as np

matrix_folder_path = "./matrices/Nr25_Nc25"

for a in glob.glob(matrix_folder_path + "/RandTrix*.txt"):
    print(a)