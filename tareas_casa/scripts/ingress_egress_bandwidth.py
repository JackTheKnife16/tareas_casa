#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Ingress and Egress port bandwidth script")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--ports', required=True,
                                help="Number of ports in the model.")
args = parser.parse_args()

stats_csv = args.stats
ports = int(args.ports)

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

print("\n\n\n\n\nBandwidth:")
# Iterate over the ports
for i in range(0, ports):
    # Create the filter to get the i-th ingress port
    ingress_port_search = "ingress_port_%d" % (i)
    # Search using pandas
    bytes_sent_ingress = df.loc[(df["ComponentName"] == ingress_port_search) & (df["StatisticName"] == "bytes_sent")]
    # Drop last entry because it is the summary
    bytes_sent_ingress.drop(bytes_sent_ingress.tail(1).index, inplace = True)

    # Some egress ports dont receive traffic, then packet sent and bandwidth is 0
    bandwidth_ingress = 0.0
    sent_pkt_ingress = 0
    if len(bytes_sent_ingress) != 0:
        # Get time when first packet arrived and size of first packet
        first_pkt_t = bytes_sent_ingress["SimTime"].iloc[0] / 1000

        # Get time when last packet arrived and all bytes sent
        last_pkt_t = bytes_sent_ingress["SimTime"].iloc[-1] / 1000
        last_pkt_b = bytes_sent_ingress["Sum.u32"].iloc[-1] 

        # Compute bandwidth and packets sent
        bandwidth_ingress = ((last_pkt_b * 8)/(last_pkt_t-first_pkt_t))
        
        # Get the packets sent by ingress port
        sent_pkts_results = df.loc[(df["ComponentName"] == ingress_port_search) & (df["StatisticName"] == "sent_packets")] 
        sent_pkt_ingress = sent_pkts_results["Sum.u32"].iloc[0]
    
    # Same with egress port
    egress_port_search = "egress_port_%d" % (i)
    bytes_sent_egress = df.loc[(df["ComponentName"] == egress_port_search) & (df["StatisticName"] == "bytes_sent")]
    bytes_sent_egress.drop(bytes_sent_egress.tail(1).index, inplace = True)
    
    # Some egress ports dont receive traffic, then packet sent and bandwidth is 0
    bandwidht_egress = 0.0
    sent_pkt_egress = 0
    if len(bytes_sent_egress) != 0:
        first_pkt_t = bytes_sent_egress["SimTime"].iloc[0] / 1000

        last_pkt_t = bytes_sent_egress["SimTime"].iloc[-1] / 1000
        last_pkt_b = bytes_sent_egress["Sum.u32"].iloc[-1]

        bandwidht_egress = ((last_pkt_b * 8)/(last_pkt_t-first_pkt_t))
        # Get the packets sent by ingress port
        sent_pkts_results = df.loc[(df["ComponentName"] == egress_port_search) & (df["StatisticName"] == "sent_packets")] 
        sent_pkt_egress = sent_pkts_results["Sum.u32"].iloc[0]
    
    # Print results
    print("|----> Ingress port %d: %d packets @ %f Gb/s\n|----> Egress port %i:  %d packets @ %f Gb/s\n\n" %(
        i,
        sent_pkt_ingress, 
        bandwidth_ingress,
        i,
        sent_pkt_egress, 
        bandwidht_egress,
        ))