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

# Plot the number of ports given
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for ingress_port in ports:
    # Search index for ingress port
    port_index = "ingress_port_%s" % (ingress_port)

    # Search for the priorities sent by ingress ports
    current_port = df.loc[(df["ComponentName"] == port_index) & (df["StatisticName"] == "priorities")]

    # Create the data to plot
    port_priority_stats = {
        "priority" : ["0", "1", "2", "3", "4", "5", "6" ,"7"],
        "packets": [current_port["Bin0:0-0.u64"].iloc[0], current_port["Bin1:1-1.u64"].iloc[0], 
                    current_port["Bin2:2-2.u64"].iloc[0], current_port["Bin3:3-3.u64"].iloc[0], 
                    current_port["Bin4:4-4.u64"].iloc[0], current_port["Bin5:5-5.u64"].iloc[0], 
                    current_port["Bin6:6-6.u64"].iloc[0], current_port["Bin7:7-7.u64"].iloc[0]]
    }

    # Create the data frame
    ingress_port_frame = pandas.DataFrame(port_priority_stats, columns= ['priority','packets'])
    print(ingress_port_frame)
    # Plot the results for this port
    p1 = ax[i].bar(ingress_port_frame.priority, height=ingress_port_frame["packets"])
    if i == 0:
        ax[i].set_ylabel("Number of packets")
    ax[i].set_xlabel("Priority")
    ax[i].set_title("Ingress Port %s" % (ingress_port))

    i += 1

# Graph title
plt.suptitle("Priority distribution ingress ports", size=16)

# Plot the number of ports given
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for egress_port in ports:
    # Search index for ingress port
    port_index = "egress_port_%s" % (egress_port)

    # Search for the priorities sent by ingress ports
    current_port = df.loc[(df["ComponentName"] == port_index) & (df["StatisticName"] == "priorities")]

    # Create the data to plot
    port_priority_stats = {
        "priority" : ["0", "1", "2", "3", "4", "5", "6" ,"7"],
        "packets": [current_port["Bin0:0-0.u64"].iloc[0], current_port["Bin1:1-1.u64"].iloc[0], 
                    current_port["Bin2:2-2.u64"].iloc[0], current_port["Bin3:3-3.u64"].iloc[0], 
                    current_port["Bin4:4-4.u64"].iloc[0], current_port["Bin5:5-5.u64"].iloc[0], 
                    current_port["Bin6:6-6.u64"].iloc[0], current_port["Bin7:7-7.u64"].iloc[0]]
    }

    # Create the data frame
    egress_port_frame = pandas.DataFrame(port_priority_stats, columns= ['priority','packets'])

    # Plot the results for this port
    p1 = ax[i].bar(egress_port_frame.priority, height=egress_port_frame["packets"])
    if i == 0:
        ax[i].set_ylabel("Number of packets")
    ax[i].set_xlabel("Priority")
    ax[i].set_title("Egress Port %s" % (egress_port))

    i += 1

# Graph title
plt.suptitle("Priority distribution egress ports", size=16)

# Plot for the priority being serviced at the time
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for egress_port in ports:
    # Search key for egress port
    key_value = "egress_port_%s" % (egress_port)

    # Search for priorities being serviced by the egress port
    egress_port_priorities = df.loc[(df["ComponentName"] == key_value) & (df["StatisticName"] == "current_priority")]
    egress_port_priorities.drop(egress_port_priorities.tail(1).index, inplace = True)

    # Plot this in time
    egress_port_priorities["SimTime"] = egress_port_priorities["SimTime"] / 1000
    ax[i].plot(egress_port_priorities["SimTime"], egress_port_priorities["Sum.u32"])
    if i == 0:
        ax[i].set_ylabel("Priority")
    ax[i].set_xlabel("Time (ns)")
    ax[i].set_title("Priority Serviced by Egress Port %s" % (egress_port))

    i += 1

plt.figure(4)

# Search for the packet buffer usage
buffer_available = df.loc[(df["ComponentName"] == "packet_buffer") & (df["StatisticName"] == "blocks_available")]
buffer_available.drop(buffer_available.tail(1).index, inplace = True)

# Plot the packe buffer user by time
buffer_available["SimTime"] = buffer_available["SimTime"] / 1000
plt.plot(buffer_available["SimTime"], buffer_available["Sum.u32"])
ax = plt.gca()
ax.set_ylabel("Blocks Available")
ax.set_xlabel("Time (ns)")
ax.set_title("Packet buffer block usage")


# Plot for the priority being serviced at the time
fig, ax = plt.subplots(1, port_size, figsize=(15,5), dpi=100, sharex=True, sharey=True)
i = 0
for port_fifo in ports:
    # Search key for egress port
    component = "port_fifos_%s:priority_queues" % (port_fifo)


    for pri in range(8):
        key_value = "%s[%d]" % (component, pri)

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
    component = "port_fifos_%s:priority_queues" % (port_fifo)


    for pri in range(8):
        key_value = "%s[%d]" % (component, pri)

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