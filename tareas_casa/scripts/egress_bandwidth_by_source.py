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
required_arguments.add_argument('-p', '--egress_port', required=True,
                                help="Egress port number to check.")
required_arguments.add_argument('-i', '--source_port_list', nargs='+', required=True,
                                help="List of source ports to check in the egress port.")
args = parser.parse_args()

stats_csv = args.stats
port = int(args.egress_port)

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

figure_id = 1

print(args.source_port_list)
for source in args.source_port_list:
    # Create the filter to get the given egress port |
    egress_port_search = "egress_port_%d" % (port)
    source_port_search = "port_%s" % (source)
    # Search using pandas
    bytes_sent_egress = df.loc[(df["ComponentName"] == egress_port_search) & (df["StatisticName"] == "bytes_sent_by_port") & (df["StatisticSubId"] == source_port_search)]
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
        plt.figure(figure_id)
        plt.plot(bytes_sent_egress["SimTime"] / 1000, egress_bw)
        ax = plt.gca()
        ax.set_ylabel("Bandwidth (Gb/s)")
        ax.set_xlabel("Time (ns)")
        ax.ticklabel_format(useOffset=False)
        ax.set_title("Bandwidth in Time Egress Port %d by Ingress Port %s" % (port, source))
        figure_id += 1

plt.show()