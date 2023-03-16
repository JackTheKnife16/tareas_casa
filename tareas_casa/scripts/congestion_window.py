#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="TCP Model: Congestion Window")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-n', '--host', required=True,
                                help="Name of the host.")
args = parser.parse_args()

print("Reading: ", args.stats) 
df = pandas.read_csv(args.stats)

host_name = args.host + ":applications"

# Remove unnecessary columns
try:
    del df["StatisticSubId"]
    del df["Rank"]
    del df["SumSQ.u32"]
except:
    print("Skipping delete columns")

plt.figure(1)

# Search for dropped packet during time
dropped_packets = df.loc[(df["ComponentName"] == host_name) & (df["StatisticName"] == "cWnd")]
dropped_packets.drop(dropped_packets.tail(1).index, inplace = True)

# Show results
plt.plot(dropped_packets["SimTime"] / 1000, dropped_packets["Sum.u32"])
ax = plt.gca()
ax.set_ylabel("Congestion Window (B)")
ax.set_xlabel("Time (ns)")
ax.set_title("%s's Congestion Window" % (args.host))

plt.show()