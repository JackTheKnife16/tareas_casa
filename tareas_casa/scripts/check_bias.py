#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import sys
import json
import os

group = sys.argv[1]
bias_number = sys.argv[2]

file_name = "qs_1_0/config/lookup_tables/%s_lut_%s.json" % (group, bias_number)

data = None

if os.path.isfile(file_name):
    with open(file_name) as jsonfile:
        data = json.load(jsonfile)
else:
    print("Error", file_name)
    exit(1)


result = []

for i in range(0,32):
    result.append([])
    for j in range(0,128):
        result[i].append(data[j*32+i])

csv_content = []

for row in result:
    line = ""
    for column in row:
        line += "%d," % (column)
    line += "\n"
    csv_content.append(line)

with open("result.csv", 'w') as outfile:
    outfile.writelines(csv_content)