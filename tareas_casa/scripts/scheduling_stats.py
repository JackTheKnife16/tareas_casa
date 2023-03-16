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
required_arguments.add_argument('-p', '--port', required=True,
                                help="Ports to check stats.")
args = parser.parse_args()

port = args.port

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
# Plot for the queue being serviced at the time by scheduler
plt.figure(1)
# Search key for egress port
key_value = "scheduler_%s" % (port)

# Search for priorities being serviced by the egress port
scheduler_queue_service = df.loc[(df["ComponentName"] == key_value) & (df["StatisticName"] == "queue_serviced")]
scheduler_queue_service.drop(scheduler_queue_service.tail(1).index, inplace = True)

# Plot this in time
scheduler_queue_service["SimTime"] = scheduler_queue_service["SimTime"] / 1000
plt.plot(scheduler_queue_service["SimTime"], scheduler_queue_service["Sum.u32"], 'bo')

ax = plt.gca()
ax.set_ylabel("Queue Priority")
ax.set_xlabel("Time (ns)")
ax.set_title("Priority Queue Serviced by Scheduler %s" % (port))

# Plot for the source being serviced at the time by scheduler
plt.figure(2)
# Search key for egress port
key_value = "scheduler_%s" % (port)

# Search for priorities being serviced by the egress port
scheduler_src_service = df.loc[(df["ComponentName"] == key_value) & (df["StatisticName"] == "source_serviced")]
scheduler_src_service.drop(scheduler_src_service.tail(1).index, inplace = True)

# Plot this in time
scheduler_src_service["SimTime"] = scheduler_src_service["SimTime"] / 1000
plt.plot(scheduler_src_service["SimTime"], scheduler_src_service["Sum.u32"], 'bo')

ax = plt.gca()
ax.set_ylabel("Source Port")
ax.set_xlabel("Time (ns)")
ax.set_title("Source Port Serviced by Scheduler %s" % (port))

plt.show()