#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Traffic stats")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--port_list', nargs='+', required=True,
                                help="List of ports to check stats. MAX 4 ports")
args = parser.parse_args()

print(args.port_list)

ports = args.port_list

print("Reading: ", args.stats)
df = pandas.read_csv(args.stats)

# Remove unnecessary columns
try:
    del df["StatisticSubId"]
    del df["Rank"]
    del df["BinsMinValue.u32"]
    del df["BinsMaxValue.u32"]
    del df["BinWidth.u32"]
    del df["TotalNumBins.u32"]
    del df["SumSQ.u32"]
    del df["NumActiveBins.u32"]
    del df["NumItemsCollected.u64"]
    del df["NumItemsBinned.u64"]
    del df["NumOutOfBounds-MinValue.u64"]
    del df["NumOutOfBounds-MaxValue.u64"]
except:
    print("Skipping delete columns")

# If there are more than 4 ports, the others are ignored
port_size = len(ports)
if port_size > 4:
    del ports[4:]
    port_size = len(ports)

# Plot for the priority being serviced at the time
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for port_fifo in ports:
    # Search key for egress port
    key_value = "l4s_port_fifos_%s:priority_queues" % (port_fifo)

    # Search for priorities being serviced by the egress port
    priority_queues_packets = df.loc[(df["ComponentName"] == key_value) & (df["StatisticName"] == "num_packets")]
    priority_queues_packets.drop(priority_queues_packets.tail(1).index, inplace = True)

    # Plot this in time
    priority_queues_packets["SimTime"] = priority_queues_packets["SimTime"] / 1000
    ax[i].plot(priority_queues_packets["SimTime"], priority_queues_packets["Sum.u32"])
    
    if i == 0:
        ax[i].set_ylabel("Packets in Queues")
    ax[i].legend(["0", "1", "2", "3", "4", "5", "6", "7"])
    ax[i].set_xlabel("Time (ns)")
    ax[i].set_title("Priority Queues %s Utilization (Packets)" % (port_fifo))

    i += 1


# Plot for the priority being serviced at the time
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for port_fifo in ports:
    # Search key for egress port
    key_value = "l4s_port_fifos_%s:priority_queues" % (port_fifo)

    # Search for priorities being serviced by the egress port
    priority_queues_packets = df.loc[(df["ComponentName"] == key_value) & (df["StatisticName"] == "num_bytes")]
    priority_queues_packets.drop(priority_queues_packets.tail(1).index, inplace = True)

    # Plot this in time
    priority_queues_packets["SimTime"] = priority_queues_packets["SimTime"] / 1000
    ax[i].plot(priority_queues_packets["SimTime"], priority_queues_packets["Sum.u32"])
    
    if i == 0:
        ax[i].set_ylabel("Bytes in Queues (B)")
    ax[i].legend(["0", "1", "2", "3", "4", "5", "6", "7"])
    ax[i].set_xlabel("Time (ns)")
    ax[i].set_title("Queues Port %s Utilization (Bytes)" % (port_fifo))

    i += 1

plt.show()