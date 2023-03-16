#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Dropped packet stats")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--ports', required=True,
                                help="Number of ports in the model.")
required_arguments.add_argument('-w', '--wred', required=True,
                                help="WRED component name in stats.")
args = parser.parse_args()

print("Reading: ", args.stats) 
df = pandas.read_csv(args.stats)
ports = int(args.ports)

# Remove unnecessary columns
try:
    del df["StatisticSubId"]
    del df["Rank"]
    del df["SumSQ.u32"]
except:
    print("Skipping delete columns")

plt.figure(1)

# Search for dropped packet during time
ce_packets = df.loc[(df["ComponentName"] == args.wred) & (df["StatisticName"] == "ce_packets")]
ce_packets.drop(ce_packets.tail(1).index, inplace = True)

# Show results
plt.plot(ce_packets["SimTime"] / 1000, ce_packets["Sum.u32"])
ax = plt.gca()
ax.set_ylabel("Congestion Experienced Packets")
ax.set_xlabel("Time (ns)")
ax.set_title("Packets Marked as CE")

plt.figure(2)
# Search for dropped destinations in drop receiver
ce_packets_by_dest = df.loc[(df["ComponentName"] == args.wred) & (df["StatisticName"] == "ce_packets_by_dest")]

# Create histogram data for the graph
ce_packets = {"port": [], "packets": []}
for i in range(0, ports):
    # Adding ports
    ce_packets["port"].append(str(i))
    key = "Bin%d:%d-%d.u64" % (i, i, i)
    # Number of dropped packets for that port
    ce_packets["packets"].append(ce_packets_by_dest[key].iloc[0])

# Create frame to graph
port_ce_dest_frame = pandas.DataFrame(ce_packets, columns= ['port','packets'])

# Show the graph
plt.bar(port_ce_dest_frame.port, height=port_ce_dest_frame["packets"])
ax = plt.gca()
ax.set_ylabel("Congestion Experienced Packets")
ax.set_xlabel("Destination")
ax.set_title("Packets Marked as CE by Destination")

plt.figure(3)
# Search for dropped destinations in drop receiver
ce_packets_by_src = df.loc[(df["ComponentName"] == args.wred) & (df["StatisticName"] == "ce_packets_by_src")]

# Create histogram data for the graph
ce_packets = {"port": [], "packets": []}
for i in range(0, ports):
    # Adding ports
    ce_packets["port"].append(str(i))
    key = "Bin%d:%d-%d.u64" % (i, i, i)
    # Number of dropped packets for that port
    ce_packets["packets"].append(ce_packets_by_src[key].iloc[0])

# Create frame to graph
port_ce_src_frame = pandas.DataFrame(ce_packets, columns= ['port','packets'])

# Show the graph
plt.bar(port_ce_src_frame.port, height=port_ce_src_frame["packets"])
ax = plt.gca()
ax.set_ylabel("Congestion Experienced Packets")
ax.set_xlabel("Source")
ax.set_title("Packets Marked as CE by Source")

plt.show()