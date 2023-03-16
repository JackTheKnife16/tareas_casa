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


# Creating multiple plot with all the PQ priorities
fig, ax = plt.subplots(2, 4, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
j = 0
for priority in range(0, 8):
    # PQ index to search
    pq_index = "pq_%s_%d" % (port, priority)
    # print(i, j, pq_index)

    # Search data for this PQ
    parent_util = df.loc[(df["ComponentName"] == pq_index) & (df["StatisticName"] == "parent_util")]
    bytes_util = df.loc[(df["ComponentName"] == pq_index) & (df["StatisticName"] == "bytes_util")]
    utilization = df.loc[(df["ComponentName"] == pq_index) & (df["StatisticName"] == "utilization")]

    # Drop last entry because it is a summary
    parent_util.drop(parent_util.tail(1).index, inplace = True)
    bytes_util.drop(bytes_util.tail(1).index, inplace = True)
    utilization.drop(utilization.tail(1).index, inplace = True)

    # Plot parent util in time
    p1 = ax[i, j].plot(parent_util["SimTime"] / 1000, parent_util["Sum.u32"])
    # Plot bytes in use in time
    p2 = ax[i, j].plot(bytes_util["SimTime"] / 1000, bytes_util["Sum.u32"])
    # Plot utilization in time
    p3 = ax[i, j].plot(utilization["SimTime"] / 1000, utilization["Sum.u32"])
    ax[i, j].legend(["Parent", "Bytes", "Child"])
    ax[i, j].set_ylabel("Utilization")
    ax[i, j].set_xlabel("Time (ns)")
    ax[i, j].set_title("Physical queue port %s priority %d" % (port, priority))
    
    # Index management for the plot
    if priority == 3:
        i = 1
    
    j += 1

    if j == 4:
        j = 0

plt.figure(2)

# QG index to search
qg_index = "qg_%s" % (port)
# print(qg_index)

# Search data for this QG
parent_util = df.loc[(df["ComponentName"] == qg_index) & (df["StatisticName"] == "parent_util")]
bytes_util = df.loc[(df["ComponentName"] == qg_index) & (df["StatisticName"] == "bytes_util")]
utilization = df.loc[(df["ComponentName"] == qg_index) & (df["StatisticName"] == "utilization")]

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
ax.set_title("Queue group port %s" % (port))

# Plot all the MGs
fig, ax = plt.subplots(1, 4, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
# Iterate in the MGs (from 4 to 7)
for mg in range(4, 8):
    # MG index to search
    mg_index = "mg_%d" % (mg)
    # print(mg_index)

    # Search data for this MG
    parent_util = df.loc[(df["ComponentName"] == mg_index) & (df["StatisticName"] == "parent_util")]
    bytes_util = df.loc[(df["ComponentName"] == mg_index) & (df["StatisticName"] == "bytes_util")]
    utilization = df.loc[(df["ComponentName"] == mg_index) & (df["StatisticName"] == "utilization")]

    # Drop last entry because it is a summary
    parent_util.drop(parent_util.tail(1).index, inplace = True)
    bytes_util.drop(bytes_util.tail(1).index, inplace = True)
    utilization.drop(utilization.tail(1).index, inplace = True)

    # plot parent utilization, bytes utilization, and utilization
    ax[i].plot(parent_util["SimTime"] / 1000, parent_util["Sum.u32"])
    ax[i].plot(bytes_util["SimTime"] / 1000, bytes_util["Sum.u32"])
    ax[i].plot(utilization["SimTime"] / 1000, utilization["Sum.u32"])

    # Show graph
    ax[i].legend(["Parent", "Bytes", "Child"])
    ax[i].set_ylabel("Utilization")
    ax[i].set_xlabel("Time (ns)")
    ax[i].set_title("Memory group %d" % (mg))

    i += 1

plt.show()

