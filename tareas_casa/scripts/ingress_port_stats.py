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
required_arguments.add_argument('-p', '--ports', required=True,
                                help="Number of ports in the model.")
args = parser.parse_args()

ports = int(args.ports)

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

# Iterate over the ports
for i in range(0, ports):
    # Create the filter to get the i-th ingress port
    ingress_port_search = "ingress_port_%d" % (i)
    # Search using pandas
    dest_sent_ingress = df.loc[(df["ComponentName"] == ingress_port_search) & (df["StatisticName"] == "destination")]

    print("Port %d:\n" % (i))
    print("Ingress:")
    if len(dest_sent_ingress) != 0:
        for current_port in range(0, ports):
            bucket = "Bin%d:%d-%d.u64" % (current_port, current_port, current_port)
            packets = dest_sent_ingress[bucket].iloc[-1]
            if packets != 0:
                print("|----> Destination Port %d: %d packets" % (current_port, packets))
    # Create the filter to get the i-th egress port
    egress_port_search = "egress_port_%d" % (i)
    # Search using pandas
    dest_sent_egress = df.loc[(df["ComponentName"] == egress_port_search) & (df["StatisticName"] == "source")]

    print("Egress:")
    if len(dest_sent_egress) != 0:
        for current_port in range(0, ports):
            bucket = "Bin%d:%d-%d.u64" % (current_port, current_port, current_port)
            packets = dest_sent_egress[bucket].iloc[-1]
            if packets != 0:
                print("|----> Source Port %d: %d packets" % (current_port, packets))
    print("\n")