#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Ingress and Egress port bandwidth in time")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--port', required=True,
                                help="por number to check.")
args = parser.parse_args()

stats_csv = args.stats
port = int(args.port)

print("Reading: ", stats_csv)
df = pandas.read_csv(stats_csv)

# Remove unnecessary columns
try:
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

# Create the filter to get the given ingress port
ingress_port_search = "ingress_port_%d" % (port)
# Search using pandas
bytes_sent_ingress = df.loc[(df["ComponentName"] == ingress_port_search) & (df["StatisticName"] == "bytes_sent")]
# Drop last entry because it is the summary
bytes_sent_ingress.drop(bytes_sent_ingress.tail(1).index, inplace = True)
if len(bytes_sent_ingress) != 0:
    # Values used for adjustment
    first_ingress_time = bytes_sent_ingress.iloc[0]["SimTime"]
    first_ingress_bytes = bytes_sent_ingress.iloc[0]["Sum.u32"]

    # Drop the first row
    bytes_sent_ingress.drop(bytes_sent_ingress.index[0], inplace = True)
    # Remove the adjustment from the results
    ingress_time_data = bytes_sent_ingress["SimTime"] - first_ingress_time
    ingress_bytes_data = bytes_sent_ingress["Sum.u32"] - first_ingress_bytes

    # Convert from ps --> ns, so the results are Gb/s
    ingress_time_data = ingress_time_data / 1000

    # Compute ingress bandwidth
    ingress_bw = (ingress_bytes_data * 8)/(ingress_time_data)

    # Show results
    plt.figure(1)
    plt.plot(bytes_sent_ingress["SimTime"] / 1000, ingress_bw)
    ax = plt.gca()
    ax.set_ylabel("Bandwidth Gb/s")
    ax.set_xlabel("Time (ns)")
    ax.set_title("Bandwidth in Time Ingress Port %d (Gb/s)" % (port))


# Create the filter to get the given egress port
egress_port_search = "egress_port_%d" % (port)
# Search using pandas
bytes_sent_egress = df.loc[(df["ComponentName"] == egress_port_search) & (df["StatisticName"] == "bytes_sent")]
# Drop last entry because it is the summary
bytes_sent_egress.drop(bytes_sent_egress.tail(1).index, inplace = True)
if len(bytes_sent_egress) != 0:
    # Values used for adjustment
    first_egress_time = bytes_sent_egress.iloc[0]["SimTime"]
    first_egress_bytes = bytes_sent_egress.iloc[0]["Sum.u32"]

    # Drop the first row
    bytes_sent_egress.drop(bytes_sent_egress.index[0], inplace = True)

    # Remove the adjustment from the results
    egress_time_data = bytes_sent_egress["SimTime"] - first_egress_time
    egress_bytes_data = bytes_sent_egress["Sum.u32"] - first_egress_bytes

    # Convert from ps --> ns, so the results are Gb/s
    egress_time_data = egress_time_data / 1000

    egress_bw = (egress_bytes_data * 8)/(egress_time_data)
    
    # Show results
    plt.figure(2)
    plt.plot(bytes_sent_egress["SimTime"] / 1000, egress_bw)
    ax = plt.gca()
    ax.set_ylabel("Bandwidth (Gb/s)")
    ax.set_xlabel("Time (ns)")
    ax.ticklabel_format(useOffset=False)
    ax.set_title("Bandwidth in Time Egress Port %d (Gb/s)" % (port))

plt.figure(3)
priorities_legend = []
for pri in range(8):
    # Priority to filter
    priority_search = "pri_%d" % (pri)
    # Search using pandas
    bytes_sent_priority = df.loc[(df["ComponentName"] == egress_port_search) & 
        (df["StatisticName"] == "bytes_sent_by_priority") & 
        (df["StatisticSubId"] == priority_search)]
    # Drop last entry because it is the summary
    bytes_sent_priority.drop(bytes_sent_priority.tail(1).index, inplace = True)
    if (len(bytes_sent_priority) < 3):
        continue
    # Values used for adjustment
    first_egress_time = bytes_sent_priority.iloc[0]["SimTime"]
    first_egress_bytes = bytes_sent_priority.iloc[0]["Sum.u32"]

    # Drop the first row
    bytes_sent_priority.drop(bytes_sent_priority.index[0], inplace = True)

    # Remove the adjustment from the results
    egress_time_data = bytes_sent_priority["SimTime"] - first_egress_time
    egress_bytes_data = bytes_sent_priority["Sum.u32"] - first_egress_bytes

    # Convert from ps --> ns, so the results are Gb/s
    egress_time_data = egress_time_data / 1000

    egress_bw = (egress_bytes_data * 8)/(egress_time_data)
    priorities_legend.append(priority_search)
    # Show results
    plt.plot(bytes_sent_priority["SimTime"] / 1000, egress_bw)

ax = plt.gca()
ax.legend(priorities_legend)
ax.set_ylabel("Bandwidth (Gb/s)")
ax.set_xlabel("Time (ns)")
ax.set_title("Bandwidth in Time by Priority Egress Port %d (Gb/s)" % (port))

plt.show()