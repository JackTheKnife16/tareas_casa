#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Memory Accounting stats")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--port', required=True,
                                help="Port to check the MA stats.")
args = parser.parse_args()

port = args.port

print("Reading: ", args.stats)
df = pandas.read_csv(args.stats)

# Remove unnecessary columns
try:
    del df["StatisticSubId"]
    del df["Rank"]
    del df["SumSQ.u32"]
except:
    print("Skipping delete columns")

plt.figure(1)

# QG index to search
lq_index = "lq_%s" % (port)
# print(lq_index)

# Search data for this QG
parent_util = df.loc[(df["ComponentName"] == lq_index) & (df["StatisticName"] == "parent_util")]
bytes_util = df.loc[(df["ComponentName"] == lq_index) & (df["StatisticName"] == "bytes_util")]
utilization = df.loc[(df["ComponentName"] == lq_index) & (df["StatisticName"] == "utilization")]

# Drop last entry because it is a summary
parent_util.drop(parent_util.tail(1).index, inplace = True)
bytes_util.drop(bytes_util.tail(1).index, inplace = True)
utilization.drop(utilization.tail(1).index, inplace = True)

# plot parent utilization, bytes utilization, and utilization
plt.plot(parent_util["SimTime"] / 1000, parent_util["Sum.u32"])
plt.plot(bytes_util["SimTime"] / 1000, bytes_util["Sum.u32"])
plt.plot(utilization["SimTime"] / 1000, utilization["Sum.u32"])

# Show graph
ax = plt.gca()
ax.legend(["Parent", "Bytes", "Child"])
ax.set_ylabel("Utilization")
ax.set_xlabel("Time (ns)")
ax.set_title("L4S Queue port %s" % (port))

plt.show()

