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

# Seach for the priority stats in drop receiver component
drop_priorirties = df.loc[(df["ComponentName"] == "drop_receiver") & (df["StatisticName"] == "priority")]

# Create the histogram data
port_priority_stats = {
    "priority" : ["0", "1", "2", "3", "4", "5", "6" ,"7"],
    "packets": [drop_priorirties["Bin0:0-0.u64"].iloc[-1], drop_priorirties["Bin1:1-1.u64"].iloc[-1], 
                drop_priorirties["Bin2:2-2.u64"].iloc[-1], drop_priorirties["Bin3:3-3.u64"].iloc[-1], 
                drop_priorirties["Bin4:4-4.u64"].iloc[-1], drop_priorirties["Bin5:5-5.u64"].iloc[-1], 
                drop_priorirties["Bin6:6-6.u64"].iloc[-1], drop_priorirties["Bin7:7-7.u64"].iloc[-1]]
}

# Create new frame with dropped priority data
ingress_port_frame = pandas.DataFrame(port_priority_stats, columns= ['priority', 'packets'])

# Create the histogram graph
plt.bar(ingress_port_frame.priority, height=ingress_port_frame["packets"])
ax = plt.gca()
ax.set_ylabel("Number of packets")
ax.set_xlabel("Priority")
ax.set_title("Dropped packets priority")

# Dropped the last entry because the last entry is a summary
# It is not needed for the in time graph
drop_priorirties.drop(drop_priorirties.tail(1).index, inplace = True)

# Graph the dropped priorities by time
plt.figure(2)
for priority in range(0, 8):
    priority_key = "Bin%d:%d-%d.u64" % (priority, priority, priority)
    plt.plot(drop_priorirties["SimTime"] / 1000, drop_priorirties[priority_key])

# Show the graph
ax = plt.gca()
ax.legend(["Pri 0", "Pri 1", "Pri 2", "Pri 3", "Pri 4", "Pri 5", "Pri 6", "Pri 7"])
ax.set_ylabel("Dropped packets")
ax.set_xlabel("Time (ns)")
ax.set_title("Dropped packets by priority in time")


plt.figure(3)
# Search for dropped destinations in drop receiver
drop_destinations = df.loc[(df["ComponentName"] == "drop_receiver") & (df["StatisticName"] == "destination")]

# Create histogram data for the graph
port_drops = {"port": [], "packets": []}
for i in range(0, ports):
    # Adding ports
    port_drops["port"].append(str(i))
    key = "Bin%d:%d-%d.u64" % (i, i, i)
    # Number of dropped packets for that port
    port_drops["packets"].append(drop_destinations[key].iloc[0])

# Create frame to graph
port_drops_frame = pandas.DataFrame(port_drops, columns= ['port','packets'])

# Show the graph
plt.bar(port_drops_frame.port, height=port_drops_frame["packets"])
ax = plt.gca()
ax.set_ylabel("Number of packets")
ax.set_xlabel("Destination")
ax.set_title("Dropped packets destination")

plt.figure(4)

# Search for dropped packet during time
dropped_packets = df.loc[(df["ComponentName"] == "drop_receiver") & (df["StatisticName"] == "dropped_packets")]
dropped_packets.drop(dropped_packets.tail(1).index, inplace = True)

# Show results
plt.plot(dropped_packets["SimTime"] / 1000, dropped_packets["Sum.u32"])
ax = plt.gca()
ax.set_ylabel("Dropped Packets")
ax.set_xlabel("Time (ns)")
ax.set_title("Dropped packets")

plt.figure(5)
# Search for dropped destinations in drop receiver
drop_destinations = df.loc[(df["ComponentName"] == "drop_receiver") & (df["StatisticName"] == "source")]

# Create histogram data for the graph
port_drops = {"port": [], "packets": []}
for i in range(0, ports):
    # Adding ports
    port_drops["port"].append(str(i))
    key = "Bin%d:%d-%d.u64" % (i, i, i)
    # Number of dropped packets for that port
    port_drops["packets"].append(drop_destinations[key].iloc[0])

# Create frame to graph
port_drops_frame = pandas.DataFrame(port_drops, columns= ['port','packets'])

# Show the graph
plt.bar(port_drops_frame.port, height=port_drops_frame["packets"])
ax = plt.gca()
ax.set_ylabel("Number of packets")
ax.set_xlabel("Source")
ax.set_title("Dropped packets source")

plt.show()