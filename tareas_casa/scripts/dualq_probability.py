#!/p/aruba/asic/eda/anaconda3/envs/asicenv/bin/python

import pandas
import argparse
import matplotlib.pyplot as plt

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="QS Model: Memory Accounting stats")

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-s', '--stats', required=True,
                                help="Model CSV statistics.")
required_arguments.add_argument('-p', '--port', required=True,
                                help="Port to check the MA stats.")
args = parser.parse_args()

port = args.port

print("Reading: ", args.stats)
df = pandas.read_csv(args.stats)

# Remove unnecessary columns
try:
    del df["Rank"]
    del df["SumSQ.f32"]
except:
    print("Skipping delete columns")

plt.figure(1)

# QG index to search
port_index = "port_%s" % (port)
# print(port_index)

# Search data for this QG
p_C = df.loc[(df["ComponentName"] == "classic_wred") & (df["StatisticName"] == "p_C") & (df["StatisticSubId"] == port_index)]
p_CL = df.loc[(df["ComponentName"] == "l4s_wred") & (df["StatisticName"] == "p_CL") & (df["StatisticSubId"] == port_index)]
p_L = df.loc[(df["ComponentName"] == "l4s_wred") & (df["StatisticName"] == "p_L") & (df["StatisticSubId"] == port_index)]

# Drop last entry because it is a summary
p_C.drop(p_C.tail(1).index, inplace = True)
p_CL.drop(p_CL.tail(1).index, inplace = True)
p_L.drop(p_L.tail(1).index, inplace = True)

# plot parent utilization, bytes utilization, and utilization
plt.plot(p_C["SimTime"] / 1000, p_C["Sum.f32"])
plt.plot(p_CL["SimTime"] / 1000, p_CL["Sum.f32"])
plt.plot(p_L["SimTime"] / 1000, p_L["Sum.f32"])

# Show graph
ax = plt.gca()
ax.legend(["p_C", "p_CL", "p_L"])
ax.set_ylabel("Probability")
ax.set_ylim([0, 130])
ax.set_xlabel("Time (ns)")
ax.set_title("Coupling in port %s" % (port))

plt.figure(2)

plt.plot(p_C["SimTime"] / 1000, p_C["Sum.f32"])

ax = plt.gca()
ax.legend(["p_C"])
ax.set_ylabel("Probability")
ax.set_ylim([0, 130])
ax.set_xlabel("Time (ns)")
ax.set_title("Classic Probability in port %s" % (port))

plt.figure(3)

plt.plot(p_CL["SimTime"] / 1000, p_CL["Sum.f32"])

ax = plt.gca()
ax.legend(["p_CL"])
ax.set_ylabel("Probability")
ax.set_ylim([0, 130])
ax.set_xlabel("Time (ns)")
ax.set_title("Coupling Probability in port %s" % (port))

plt.figure(4)

plt.plot(p_L["SimTime"] / 1000, p_L["Sum.f32"])

ax = plt.gca()
ax.legend(["p_L"])
ax.set_ylabel("Probability")
ax.set_ylim([0, 130])
ax.set_xlabel("Time (ns)")
ax.set_title("L4S Probability in port %s" % (port))

plt.show()

