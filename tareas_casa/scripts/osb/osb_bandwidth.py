#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="OSB Model: Component bandwidth")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-c', '--component_list', nargs='+', required=True,
                                help="List of component to compute the bandwidth.")

parser.add_argument('-i', '--ipg_size', required=False,
                    help="Inter-Packet size. [Default: 20B]")

args = parser.parse_args()

# stats name
stats_csv = args.stats

# IPG size
ipg_size = 20
if args.ipg_size:
    ipg_size = int(args.ipg_size)


print("Reading: ", stats_csv)
df = pandas.read_csv(stats_csv)

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

for component_name in args.component_list:
    # Search using pandas
    bytes_sent_component = df.loc[(df["ComponentName"] == component_name) & (df["StatisticName"] == "bytes_sent")]
    # Drop last entry because it is the summary
    bytes_sent_component.drop(bytes_sent_component.tail(1).index, inplace = True)

    if len(bytes_sent_component) != 0:
        # Get time when first packet arrived and size of first packet
        first_pkt_t = bytes_sent_component["SimTime"].iloc[0] / 1000
        first_pkt_b = bytes_sent_component["Sum.u32"].iloc[0]

        # Get time when last packet arrived and all bytes sent
        last_pkt_t = bytes_sent_component["SimTime"].iloc[-1] / 1000
        last_pkt_b = bytes_sent_component["Sum.u32"].iloc[-1]
        
        # Get the packets sent by ingress port
        sent_pkts_results = df.loc[(df["ComponentName"] == component_name) & (df["StatisticName"] == "sent_packets")]
        if len(sent_pkts_results):
            sent_pkts = sent_pkts_results["Sum.u32"].iloc[0]
            # Compute bandwidth and packets sent
            bandwidth_ingress = ((((last_pkt_b-first_pkt_b) + sent_pkts * ipg_size) * 8) / (last_pkt_t-first_pkt_t))
            
            # Print results
            print("\n|----> %s: %d packets @ %f Gb/s\n" %(
                component_name,
                sent_pkts, 
                bandwidth_ingress
                ))
        else:
            sent_pkts = 0
            print("\n|---> %s: To compute the bandwidth the statistic sent_packets must be enabled.\n" % (component_name))
    else:
        print("\n|---> %s: There are not statistics for this component\n" % (component_name))
