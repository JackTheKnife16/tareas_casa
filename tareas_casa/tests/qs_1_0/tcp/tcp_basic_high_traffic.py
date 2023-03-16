import sst
import argparse
import json
import os
import logging

#######################
# Utility functions   #
#######################
def set_logging_conf(log_mode):
   logging.basicConfig(level=log_mode, format='%(asctime)s: %(levelname)s: %(message)s')

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="Memory accounting basic SST tests")

# Optional Arguments
parser.add_argument('-d', '--debug', required=False,
                    help="Debug Mode", action='store_true')

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-a', '--args_json', required=True,
                                help="Arguments JSON file.")

parser.add_argument('-v', '--verbosity', type=str,
                    default="INFO",
                    help='verbosity option, it can be INFO, MODERATE or DEBUG, value by default is INFO.')

args = parser.parse_args()

INFO = 0
MODERATE = 1
DEBUG = 2

verbosity_option = INFO

if args.verbosity == 'DEBUG':
    verbosity_option = DEBUG
elif args.verbosity == 'MODERATE':
    verbosity_option = MODERATE
else:
    pass

################
# LOGGING MODE #
################
if args.debug:
    set_logging_conf(logging.DEBUG)
else:
    set_logging_conf(logging.INFO)

# Loading json args file
if os.path.isfile(args.args_json):
    try:
        with open(args.args_json) as jsonfile:
            args_json = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", args.args_json)
        exit(1)
else:
    logging.error("File %s does not exist", args.args_json)
    exit(1)

# Loadig ma config file
if os.path.isfile(args_json["ma_config"]):
    try:
        with open(args_json["ma_config"]) as jsonfile:
            ma_config = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", args_json["ma_config"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["ma_config"])
    exit(1)

port_config_file = args_json["ports_config"]
# Loading ports config file
if os.path.isfile(port_config_file):
    try:
        with open(port_config_file) as jsonfile:
            ports_config = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", port_config_file)
        exit(1)
else:
    logging.error("File %s does not exist", port_config_file)
    exit(1)
traffic_config_file = "tests/qs_1_0/tcp/traffic_config/traffic_config_high_traffic.json"


# Define SST core options
sst.setProgramOption("timebase", "1ps")

# Number of Ports
num_ports = len(ports_config)

# Enable statistics level 7
sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
                        {
                            "filepath" : args_json["stats_csv"],
                            "separator" : ","
                        })

# TCP model
host0 = sst.Component("host0", "networkTraffic.NetworkHost")
host0.addParams({
    "ip_address": "10.0.0.2",
    "mac_address": "BE:EF:FF:FF:FF:FF",
    "network_table": "tests/qs_1_0/tcp/network_table.json",
    "priority": 1,
    "verbose" : verbosity_option,
})
sch0 = host0.setSubComponent("event_scheduler", "networkTraffic.EventScheduler", 0)
sch0.addParams({
    "verbose" : verbosity_option,
})

# Host 0 Port -----------
port0 = sst.Component("port0", "networkTraffic.NetworkPort")
port0.addParams({
    "number": 0,
    "verbose" : verbosity_option,
})
# -----------------------

# Host 0 Layers --------------------

# Physical
phys0 = host0.setSubComponent("physical", "networkTraffic.PhysicalLayer", 0)
phys0.addParams({
    "host_bandwidth": "2.5Gb/s",
    "verbose" : verbosity_option,
})

# Data Link
dlink0 = host0.setSubComponent("data_link", "networkTraffic.EthernetDataLinkLayer", 0)
dlink0.addParams({
    "dot1q": 0,
    "verbose" : verbosity_option,
})

# Network
net0 = host0.setSubComponent("network", "networkTraffic.IpNetworkLayer", 0)
net0.addParams({
    "ipv6": 0,
    "verbose" : verbosity_option,
})

# Transport
transp0 = host0.setSubComponent("transport", "networkTraffic.TCPTransportLayer", 0)
transp0.addParams({
    "verbose" : verbosity_option,
})

# Applications
transmitter = host0.setSubComponent("applications", "networkTraffic.BasicTransmitter", 0)
transmitter.addParams({
    "peer_ip_addr": "10.0.0.3",
    "peer_port": 8080,
    "num_packets": 1000,
    "packet_size": 1024,
    "data_rate": "2.5Gb/s",
    "verbose" : verbosity_option,
})

# Connecting host 0

# Port <---> Physical Layer
link_port_physical_0 = sst.Link("link_port_physical_0")
link_port_physical_0.connect(
    (phys0, "network_port", "10ps"),
    (port0, "physical_connection", "10ps"),
)

# ---------------------------

# Host 1 (Client) -------------------------
host1 = sst.Component("host1", "networkTraffic.NetworkHost")
host1.addParams({
    "ip_address": "10.0.0.3",
    "mac_address": "BE:EF:FF:FF:FF:FE",
    "network_table": "tests/qs_1_0/tcp/network_table.json",
    "priority": 1,
    "verbose" : verbosity_option,
})
sch1 = host1.setSubComponent("event_scheduler", "networkTraffic.EventScheduler", 0)
sch1.addParams({
    "verbose" : verbosity_option,
})
# --------------------

# Host 1 Port -----------
port1 = sst.Component("port1", "networkTraffic.NetworkPort")
port1.addParams({
    "number": 1,
    "verbose" : verbosity_option,
})
# -----------------------

# Host 1 Layers --------------------

# Physical
phys1 = host1.setSubComponent("physical", "networkTraffic.PhysicalLayer", 0)
phys1.addParams({
    "host_bandwidth": "2.5Gb/s",
    "verbose" : verbosity_option,
})

# Data Link
dlink1 = host1.setSubComponent("data_link", "networkTraffic.EthernetDataLinkLayer", 0)
dlink1.addParams({
    "dot1q": 0,
    "verbose" : verbosity_option,
})

# Network
net1 = host1.setSubComponent("network", "networkTraffic.IpNetworkLayer", 0)
net1.addParams({
    "ipv6": 0,
    "verbose" : verbosity_option,
})

# Transport
transp1 = host1.setSubComponent("transport", "networkTraffic.TCPTransportLayer", 0)
transp1.addParams({
    "verbose" : verbosity_option,
})

# Applications
receiver = host1.setSubComponent("applications", "networkTraffic.BasicReceiver", 0)
receiver.addParams({
    "source_port": 8080,
    "verbose" : verbosity_option,
})

# Connecting host 1

# Port <---> Physical Layer
link_port_physical_1 = sst.Link("link_port_physical_1")
link_port_physical_1.connect(
    (phys1, "network_port", "10ps"),
    (port1, "physical_connection", "10ps"),
)
host_ports = {
    0: port0,
    1: port1
}
# ---------------------------


# Packet Generator ---------------------------------------------------------------------------------
comp_packet_generator = sst.Component("traffic_generator", "TRAFFIC_GEN_1_0.TrafficGenerator")
comp_packet_generator.addParams({
    "verbose" : verbosity_option,
})

subcomp_traffic_profile = comp_packet_generator.setSubComponent("traffic_profile", "TRAFFIC_GEN_1_0.ConstantTraffic", 0)
subcomp_traffic_profile.addParams({
    "frontplane_config": port_config_file,
    "traffic_config": traffic_config_file,
    "num_ports" : num_ports,
    "seed" : 1447,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# Finisher ------------------------------------------------------------------------------------------
comp_finisher = sst.Component("finisher", "TRAFFIC_GEN_1_0.Finisher")
comp_finisher.addParams({
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# Acceptance Check ----------------------------------------------------------------------------------
comp_acceptance_checker = sst.Component("acceptance_checker", "QS_1_0.AcceptanceChecker")
comp_acceptance_checker.addParams({
    "ma_config_file" : args_json["ma_config"],
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# List with ports and receivers ---------------------------------------------------------------------
comp_ingress_ports = []
# iterate the number of ports 0-47 --> 2.5 Gb/s 48-51 --> 10 Gb/s
i = 0
for port in ports_config:
    # Ingress port [i] -------------------------------------------------------------------------
    if i not in host_ports:
        # verbose = 0
        # if i == 48:
        #     verbose = 1
        comp_ingress_port = sst.Component("ingress_port_%d" % (i), "QS_1_0.IngressPort")
        params = {
            "port" : i,
            "port_bw": port["bw"] + "b/s",
            "offered_load" : 100,
            "start_delay": "900000000ps",
            "verbose" : verbosity_option,
        }
        comp_ingress_port.addParams(params)
    else:
        comp_ingress_port = sst.Component("ingress_port_%d" % (i), "QS_1_0.HostIngressPort")
        params = {
            "port" : i,
            "verbose" : verbosity_option,
        }
        comp_ingress_port.addParams(params)

    comp_ingress_ports.append(comp_ingress_port)
    # ------------------------------------------------------------------------------------------------
    
    # Connections ------------------------------------------------------------------------------------
    if i not in host_ports:
        # Packet Generator [i] <--> Ingress Port [i] 
        link_packet_gen_ingress_port = sst.Link("link_packet_gen_ingress_port_%d" % (i))
        link_packet_gen_ingress_port.connect(
            (comp_packet_generator, "ingress_port_0_%d" % (i), "10ps"),
            (comp_ingress_port, "traffic_generator", "10ps"),
        )
    else:
        # Host Port --> Ingress Port
        link_phys_ingress_ingress_port = sst.Link("link_phys_ingress_ingress_port_%d" % (i))
        link_phys_ingress_ingress_port.connect(
            (host_ports[i], "ingress_port", "10ps"),
            (comp_ingress_port, "external_port", "10ps"),
        )
    # Ingress Port [i] --> Acceptance Checker [i]
    link_ingress_port_acceptance_checker = sst.Link("link_ingress_port_acceptance_checker_%d" % (i))
    link_ingress_port_acceptance_checker.connect(
        (comp_ingress_port, "acceptance_checker", "10ps"),
        (comp_acceptance_checker, "ingress_port_%d" % (i), "10ps")
    )
    # ------------------------------------------------------------------------------------------------
    i += 1


# WRED -----------------------------------------------------------------------------------------------
comp_wred = sst.Component("wred", "QS_1_0.WRED")
comp_wred.addParams({
    "ma_config_file" : args_json["ma_config"],
    "port_config_file": port_config_file,

    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Packet Buffer --------------------------------------------------------------------------------------
comp_pkt_buffer = sst.Component("packet_buffer", "QS_1_0.PacketBuffer")
comp_pkt_buffer.addParams({
	"initial_blocks" : 65536,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Drop Receiver --------------------------------------------------------------------------------------
comp_drop_receiver = sst.Component("drop_receiver", "QS_1_0.DropReceiver")
comp_drop_receiver.addParams({
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Egress update -------------------------------------------------------------------------------------
comp_egress_update = sst.Component("egress_update", "QS_1_0.EgressUpdate")
comp_egress_update.addParams({
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# MEMORY ACCOUNTING ------------------------------------------------------------------------------------------
# Global Pool Util ----------------------------------------------------------------------------------------
comp_global_pool_util = sst.Component("global_pool_util", "QS_1_0.GlobalPoolUtil")
comp_global_pool_util.addParams({
    "ma_config_file" : args_json["ma_config"],
    "buffer_size" : 65536,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------
# Bytes Tracker --------------------------------------------------------------------------------------
comp_bytes_tracker = sst.Component("bytes_tracker", "QS_1_0.BytesTracker")
comp_bytes_tracker.addParams({
    "ma_config_file": args_json["ma_config"],
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------ma_config

# List of fifos, scheduler and egress ports ----------------------------------------------------------
comp_ports_fifos = []
subcomp_priority_queues = []
comp_schedulers = []
comp_egress_ports = []
# comp_receivers = []
# iterate the number of ports 0-47 --> 5 Gb/s 48-51 --> 25 Gb/s
i = 0
for port in ports_config:
    # Port FIFOs [i] ----------------------------------------------------------------------------------
    comp_port_fifos = sst.Component("port_fifos_%d" % (i), "QS_1_0.PortFIFOs")
    comp_port_fifos.addParams({
        "port" : i,
        "verbose" : verbosity_option,
    })
    comp_ports_fifos.append(comp_port_fifos)
    # --------------------------------------------------------------------------------------------------

    # Scheduler [i] ------------------------------------------------------------------------------------
    comp_scheduler = sst.Component("scheduler_%d" % (i), "QS_1_0.Scheduler")
    comp_scheduler.addParams({
        "verbose" : verbosity_option,
    })
    comp_schedulers.append(comp_scheduler)
    # --------------------------------------------------------------------------------------------------
    
    # Priority queues ----------------------------------------------------------------------------------
    subcomp_queues = {}
    for j in range(8):
        # Priority Queue [i][j] ------------------------------------------------------------------------
        subcomp_pri_queue = comp_port_fifos.setSubComponent("priority_queues", "QS_1_0.PriorityQueue", j)
        subcomp_pri_queue.addParams({
            "port": i,
            "priority": j,
            "verbose" : verbosity_option,
        })
        subcomp_queues[j] = subcomp_pri_queue
        # -----------------------------------------------------------------------------------------------

        # Connections -----------------------------------------------------------------------------------
        # Priority Queue[i][j] <--> Scheduler[i]
        link_pri_queue_scheduler = sst.Link("link_pri_queue_scheduler_%d_%d" % (i, j))
        link_pri_queue_scheduler.connect(
            (subcomp_pri_queue, "scheduler", "5ps"),
            (comp_scheduler, "priority_queue_%d" % (j), "5ps"),
        )
    subcomp_priority_queues.append(subcomp_queues)
    # -----------------------------------------------------------------------------------------------

    # Egress port [i] -----------------------------------------------------------------------------------
    if i not in host_ports:
        comp_egress_port = sst.Component("egress_port_%d" % (i), "QS_1_0.EgressPort")
        params = {
            "port" : i,
            "node" : 0,
            "port_bw": port["bw"] + "b/s",
            "verbose" : verbosity_option,
        }
        comp_egress_port.addParams(params)
    else:
        comp_egress_port = sst.Component("egress_port_%d" % (i), "QS_1_0.HostEgressPort")
        params = {
            "port" : i,
            "node" : 0,
            "port_bw": port["bw"] + "b/s",
            "verbose" : verbosity_option,
        }
        comp_egress_port.addParams(params)


    comp_egress_ports.append(comp_egress_port)
    # ----------------------------------------------------------------------------------------------------

    # # Receiver [i] ---------------------------------------------------------------------------------------
    # comp_receiver = sst.Component("receiver_%d" % (i), "QS_1_0.DummyReceiver")
    # comp_receiver.addParams({
    #     "receiving" : 0,
    #     "verbose" : verbosity_option,
    # })
    # comp_receivers.append(comp_receiver)
    # # -----------------------------------------------------------------------------------------------------

    # Connections -----------------------------------------------------------------------------------------
    # WRED port fifo [i] <--> Port Fifo [i]
    link_wred_port_fifos = sst.Link("link_wred_port_fifos_%d" % (i))
    link_wred_port_fifos.connect(
        (comp_wred, "port_fifo_%d" % (i), "10ps"),
        (comp_port_fifos, "wred", "10ps"),
    )
    # Port FIFO [i] <--> Scheduler [i]
    link_port_fifos_scheduler = sst.Link("link_port_fifos_scheduler_%d" % (i))
    link_port_fifos_scheduler.connect(
        (comp_port_fifos, "packet_selector", "5ps"),
        (comp_scheduler, "port_fifo", "5ps"),
    )
    # Scheduler [i] --> Egress Port [i]
    link_scheduler_egress_port = sst.Link("link_scheduler_egress_port_%d" % (i))
    link_scheduler_egress_port.connect(
        (comp_scheduler, "egress_port", "5ps"),
        (comp_egress_port, "scheduler", "5ps"),
    )
    # # Egress port [i] --> Receiver [i]
    # link_egress_port_receiver = sst.Link("link_egress_port_receiver_%d" % (i))
    # link_egress_port_receiver.connect(
    #     (comp_egress_port, "output", "10ps"),
    #     (comp_receiver, "sender_component", "10ps"),
    # )
    # Egress port [i] --> Finisher [i]
    if i not in host_ports:
        link_egress_port_finisher = sst.Link("link_egress_port_finisher_%d" % (i))
        link_egress_port_finisher.connect(
            (comp_egress_port, "output", "5ps"),
            (comp_finisher, "egress_port_0_%d" % (i), "5ps"),
        )
    else:
        link_egress_port_finisher = sst.Link("link_egress_port_finisher_%d" % (i))
        link_egress_port_finisher.connect(
            (comp_egress_port, "finisher", "5ps"),
            (comp_finisher, "egress_port_0_%d" % (i), "5ps"),
        )
        # Host Port --> Ingress Port
        link_phys_egress_egress_port = sst.Link("link_phys_egress_egress_port_%d" % (i))
        link_phys_egress_egress_port.connect(
            (host_ports[i], "egress_port", "10ps"),
            (comp_egress_port, "phys_port", "10ps"),
        )
    # Egress port [i] --> Egress update [i]
    link_egress_port_egress_update = sst.Link("link_egress_port_egress_update_%d" % (i))
    link_egress_port_egress_update.connect(
        (comp_egress_port, "packet_buffer", "5ps"),
        (comp_egress_update, "egress_port_%d" % (i), "5ps"),
    )
    # Egress port [i] --> Bytes tracker [i]
    link_scheduler_bytes_tracker = sst.Link("link_scheduler_bytes_tracker_%d" % (i))
    link_scheduler_bytes_tracker.connect(
        (comp_scheduler, "bytes_tracker", "5ps"),
        (comp_bytes_tracker, "port_scheduler_%d" % (i), "5ps"),
    )
    # -----------------------------------------------------------------------------------------------------

    i += 1

# Setting accounting elements -----------------------------------------------------------------------------
# Memory Group --------------------------------------------------------------------------------------------

# MG LUTs -------------------------------------------------------------------------------------------------
comp_lookup_table_mg = sst.Component("lookup_table_mg", "QS_1_0.LookUpController")
comp_lookup_table_mg.addParams({
    "verbose" : verbosity_option,
})

MG_lookup_tables = 4
for i in range(0, MG_lookup_tables):
    subcomp_lookup_table = comp_lookup_table_mg.setSubComponent(
        "look_up_table", 
        "QS_1_0.LookUpTable", 
        i,
    )
    subcomp_lookup_table.addParams({
        "table_file": "%s%d.json" % (args_json["mg_lut_root"], i),
        "verbose" : verbosity_option,
    })
# ---------------------------------------------------------------------------------------------------------

# MG Utilization Reporter ---------------------------------------------------------------------------------
comp_mg_util_reporter = sst.Component("mg_util_reporter", "QS_1_0.UtilizationReporter")
comp_mg_util_reporter.addParams({
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------

# MG bytes updater-----------------------------------------------------------------------------------------
comp_mg_bytes_updater = sst.Component("mg_bytes_updater", "QS_1_0.BytesUpdater")
comp_mg_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "type" : 1,
    "verbose" : verbosity_option,
})
# Setting connectors and MG elements
comp_mg_elements = {}
i = 0
for memory_group in ma_config["Egress_MG"]["Mapping"]:
    bytes_limit = ma_config["Egress_MG"]["Byte_limits"][i]
    
    logging.debug("MG %d --> bytes_limit=%d bias=%d", memory_group["MG"], bytes_limit["Byte_limit"], memory_group["Biasing_value"])
    
    # MG element [MG index] -----------------------------------------------------------------------
    comp_mg_element = sst.Component("mg_%d" % (memory_group["MG"]), "QS_1_0.AccountingElement")
    comp_mg_element.addParams({
        "type" : 1,
        "index" : memory_group["MG"],
        "max_bytes" : bytes_limit["Byte_limit"],
        "bias" : memory_group["Biasing_value"],
        "parent_translation_config": args_json["parent_translation"],
        "verbose" : verbosity_option,
    })
    comp_mg_elements[memory_group["MG"]] = comp_mg_element
    # -----------------------------------------------------------------------------------------------

    # MG Element Connector to bytes tracker [MG index] ----------------------------------------------------------------
    subcomp_mg_connector_bytes = comp_mg_bytes_updater.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_bytes.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # MG Element Connector to Lookup Table[MG index] ----------------------------------------------------------------
    subcomp_mg_connector_lookup = comp_lookup_table_mg.setSubComponent(
        "mg_elements", 
        "QS_1_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_lookup.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # MG Element Connector to Utilization reporter[MG index] ----------------------------------------------------------------
    subcomp_mg_connector_util_reporter = comp_mg_util_reporter.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_util_reporter.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # Connections -----------------------------------------------------------------------------------
    # Global Pool Util [MG index]  <--> MG [MG index]
    link_global_pool_mg = sst.Link("link_global_pool_mg_%d" % (memory_group["MG"]))
    link_global_pool_mg.connect(
        (comp_mg_element, "parent_utilization", "5ps"),
        (comp_global_pool_util, "memory_group_%d" % (memory_group["MG"]), "5ps"),
    )

    # MG Element Connector Bytes [MG index]  <--> MG [MG index]
    link_connector_bytes_mg = sst.Link("link_connector_bytes_mg_%d" % (memory_group["MG"]))
    link_connector_bytes_mg.connect(
        (subcomp_mg_connector_bytes, "accounting_element", "5ps"),
        (comp_mg_element, "bytes_tracker", "5ps"),
    )

    # MG Element Connector LookUp Table [MG index]  <--> MG [MG index]
    link_connector_lookup_mg = sst.Link("link_connector_lookup_mg_%d" % (memory_group["MG"]))
    link_connector_lookup_mg.connect(
        (subcomp_mg_connector_lookup, "accounting_element", "5ps"),
        (comp_mg_element, "lookup_table", "5ps"),
    )

    # MG Element Connector Utilization Updater [MG index]  <--> MG [MG index]
    link_connector_lookup_mg = sst.Link("link_connector_util_reporter_mg_%d" % (memory_group["MG"]))
    link_connector_lookup_mg.connect(
        (subcomp_mg_connector_util_reporter, "accounting_element", "5ps"),
        (comp_mg_element, "child_utilization", "5ps"),
    )
    # -----------------------------------------------------------------------------------------------
    i += 1
# ---------------------------------------------------------------------------------------------------------

# QG and PQ LUTs -------------------------------------------------------------------------------------------------
comp_lookup_table_qg_pq = sst.Component("lookup_table_qg_pq", "QS_1_0.LookUpController")
comp_lookup_table_qg_pq.addParams({
    "verbose" : verbosity_option,
})

QG_PQ_lookup_tables = 32
for i in range(0, QG_PQ_lookup_tables):
    subcomp_lookup_table = comp_lookup_table_qg_pq.setSubComponent(
        "look_up_table", 
        "QS_1_0.LookUpTable", 
        i,
    )
    subcomp_lookup_table.addParams({
        "table_file": "%s%d.json" % (args_json["qg_pq_lut_root"], i),
        "verbose" : verbosity_option,
    })
# ---------------------------------------------------------------------------------------------------------

# Queue Group --------------------------------------------------------------------------------------------

# QG Utilization Reporter ---------------------------------------------------------------------------------
comp_qg_util_reporter = sst.Component("qg_util_reporter", "QS_1_0.UtilizationReporter")
comp_qg_util_reporter.addParams({
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------

# QG bytes updater-----------------------------------------------------------------------------------------
comp_qg_bytes_updater = sst.Component("qg_bytes_updater", "QS_1_0.BytesUpdater")
comp_qg_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "type" : 2,
    "verbose" : verbosity_option,
})
# Setting connectors and QG elements
comp_qg_elements = []
for port in ports_config:
    # Start index as the port
    i = port["port"]
    # Getting Byte Limit
    found = False
    for bytes_limit_config in ma_config["Egress_QG"]["Byte_limits"]:
        if bytes_limit_config["QG"] == port["bw"]:
            bytes_limit = bytes_limit_config
            found = True
            break 
    if not found:
        logging.fatal("QG: Can't find Bytes Limit for port %d. Please check port config file.", i)
    
    # Getting mapping
    found = False
    for mapping_config in ma_config["Egress_QG"]["Mapping"]:
        if mapping_config["QG"] == port["bw"]:
            mapping = mapping_config
            found = True
            break
    if not found:
        logging.fatal("QG: Can't find Mapping for port %d. Please check port config file.", i)
    
    logging.debug("QG %d --> bytes_limit=%d bias=%d", i, bytes_limit["Byte_limit"], mapping["Biasing_value"])

    # QG element [QG index] -----------------------------------------------------------------------
    comp_qg_element = sst.Component("qg_%d" % (i), "QS_1_0.AccountingElement")
    comp_qg_element.addParams({
        "type" : 2,
        "index" : i,
        "max_bytes" : bytes_limit["Byte_limit"],
        "bias" : mapping["Biasing_value"],
        "parent_translation_config": args_json["parent_translation"],
        "verbose" : verbosity_option,
    })
    comp_qg_elements.append(comp_qg_element)
    # QG Element Connector to bytes tracker [QG index] ------------------------------------------------------------------------
    subcomp_qg_connector_bytes = comp_qg_bytes_updater.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_bytes.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------
    # QG Element Connector to Lookup Table [QG index] ----------------------------------------------------------------
    subcomp_qg_connector_lookup = comp_lookup_table_qg_pq.setSubComponent(
        "qg_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_lookup.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------
    # QG Element Connector to Utilization reporter[QG index] ----------------------------------------------------------------
    subcomp_qg_connector_util_reporter = comp_qg_util_reporter.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_util_reporter.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # Connections -----------------------------------------------------------------------------------
    # Global Pool Util [QG index]  <--> QG [QG index]
    link_global_pool_qg = sst.Link("link_global_pool_qg_%d" % (i))
    link_global_pool_qg.connect(
        (comp_qg_element, "parent_utilization", "5ps"),
        (comp_global_pool_util, "queue_group_%d" % (i), "5ps"),
    )

    # QG Element Connector Bytes [QG index]  <--> QG [QG index]
    link_connector_bytes_qg = sst.Link("link_connector_bytes_qg_%d" % (i))
    link_connector_bytes_qg.connect(
        (subcomp_qg_connector_bytes, "accounting_element", "5ps"),
        (comp_qg_element, "bytes_tracker", "5ps"),
    )

    # QG Element Connector LookUp Table [QG index]  <--> QG [QG index]
    link_connector_lookup_qg = sst.Link("link_connector_lookup_qg_%d" % (i))
    link_connector_lookup_qg.connect(
        (subcomp_qg_connector_lookup, "accounting_element", "5ps"),
        (comp_qg_element, "lookup_table", "5ps"),
    )

    # QG Element Connector Utilization Updater [QG index]  <--> QG [QG index]
    link_connector_lookup_qg = sst.Link("link_connector_util_reporter_qg_%d" % (i))
    link_connector_lookup_qg.connect(
        (subcomp_qg_connector_util_reporter, "accounting_element", "5ps"),
        (comp_qg_element, "child_utilization", "5ps"),
    )
    # -----------------------------------------------------------------------------------------------

# Physical Queue --------------------------------------------------------------------------------------------

# PQ bytes updater-----------------------------------------------------------------------------------------
comp_pq_bytes_updater = sst.Component("pq_bytes_updater", "QS_1_0.BytesUpdater")
comp_pq_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "type" : 3,
    "verbose" : verbosity_option,
})

# PQ Utilization Reporter ---------------------------------------------------------------------------------
comp_pq_util_reporter = sst.Component("pq_util_reporter", "QS_1_0.UtilizationReporter")
comp_pq_util_reporter.addParams({
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------

# Setting connectors and ...
comp_pq_elements = {}
pq_index = 0
for port in ports_config:
    # Start index as the port
    i = port["port"]

    # 8 priorities
    for j in range(0, 8):
        if port["bw"] in ma_config["Egress_PQ"]["Byte_limits"][j]:
            bytes_limit = ma_config["Egress_PQ"]["Byte_limits"][j][port["bw"]]
        else:
            logging.fatal("PQ: Can't find Bytes Limit for port %d. Please check port config file.", i)

        mapping = ma_config["Egress_PQ"]["Mapping"][j]

        logging.debug("PQ %d,%d --> bytes_limit=%d bias=%d", i, j, bytes_limit, mapping["Biasing_value"])
        
        # PQ element [PQ index] -----------------------------------------------------------------------
        comp_pq_element = sst.Component("pq_%d_%d" % (i, j), "QS_1_0.AccountingElement")
        comp_pq_element.addParams({
            "type" : 3,
            "index" : i,
            "priority": j,
            "max_bytes" : bytes_limit,
            "bias" : mapping["Biasing_value"],
            "parent_translation_config": args_json["parent_translation"],
            "verbose" : verbosity_option,
        })
        comp_pq_elements[(i, j)] = comp_pq_element
        # PQ Element Connector [PQ index] ------------------------------------------------------------------------
        subcomp_pq_connector = comp_pq_bytes_updater.setSubComponent(
            "accounting_elements", 
            "QS_1_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : verbosity_option,
        })
        # PQ Element Connector to Lookup Table [PQ index] ----------------------------------------------------------------
        subcomp_pq_connector_lookup = comp_lookup_table_qg_pq.setSubComponent(
            "pq_elements", 
            "QS_1_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector_lookup.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : verbosity_option,
        })
        # -----------------------------------------------------------------------------------------------
        # QG Element Connector to Utilization reporter[QG index] ----------------------------------------------------------------
        subcomp_pq_connector_util_reporter = comp_pq_util_reporter.setSubComponent(
            "accounting_elements", 
            "QS_1_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector_util_reporter.addParams({
            "index": "%d_%d" % (i, j),
            "type": "QG",
            "verbose" : verbosity_option,
        })
        # -----------------------------------------------------------------------------------------------

        # PQ Element Connector to parent [PQ index] ----------------------------------------------------------------
        subcomp_pq_parent = comp_mg_elements[mapping["MG_parent"]].setSubComponent(
            "children", 
            "QS_1_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_parent.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : verbosity_option,
        })
        # -----------------------------------------------------------------------------------------------
        
        pq_index += 1
        # -----------------------------------------------------------------------------------------------

        # Connections -----------------------------------------------------------------------------------
        # Global Pool Util [PQ index]  <--> PQ [PQ index]
        link_mg_pq = sst.Link("link_mg_pq_%d_%d" % (i, j))
        link_mg_pq.connect(
            (subcomp_pq_parent, "accounting_element", "5ps"),
            (comp_pq_element, "parent_utilization", "5ps"),
        )

        # PQ Element Connector Bytes [PQ index]  <--> PQ [PQ index]
        link_connector_bytes_pq = sst.Link("link_connector_bytes_pq_%d_%d" % (i, j))
        link_connector_bytes_pq.connect(
            (subcomp_pq_connector, "accounting_element", "5ps"),
            (comp_pq_element, "bytes_tracker", "5ps"),
        )

        # PQ Element Connector LookUp Table [PQ index]  <--> PQ [PQ index]
        link_connector_lookup_pq = sst.Link("link_connector_lookup_pq_%d_%d" % (i, j))
        link_connector_lookup_pq.connect(
            (subcomp_pq_connector_lookup, "accounting_element", "5ps"),
            (comp_pq_element, "lookup_table", "5ps"),
        )

        # PQ Element Connector Utilization Updater [PQ index]  <--> PQ [PQ index]
        link_connector_lookup_pq = sst.Link("link_connector_util_reporter_pq_%d_%d" % (i, j))
        link_connector_lookup_pq.connect(
            (subcomp_pq_connector_util_reporter, "accounting_element", "5ps"),
            (comp_pq_element, "child_utilization", "5ps"),
        )
        # -----------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------------------



# Connections ---------------------------------------------------------------------------------------------

# Packet Generator --> WRED
link_packet_generator_finisher = sst.Link("link_packet_generator_finisher")
link_packet_generator_finisher.connect(
    (comp_packet_generator, "finisher", "5ps"),
    (comp_finisher, "traffic_generator", "5ps")
)

# Acceptance Checker --> WRED
link_acceptance_checker_wred = sst.Link("link_acceptance_checker_wred")
link_acceptance_checker_wred.connect(
    (comp_acceptance_checker, "wred", "10ps"),
    (comp_wred, "acceptance_checker", "10ps")
)

# WRED --> Packet Buffer
link_wred_packet_buffer = sst.Link("link_wred_packet_buffer")
link_wred_packet_buffer.connect(
    (comp_wred, "packet_buffer", "10ps"),
    (comp_pkt_buffer, "acceptance_checker", "10ps")
)

# WRED --> Drop Receiver
link_wred_drop_receiver = sst.Link("link_wred_drop_receiver")
link_wred_drop_receiver.connect(
    (comp_wred, "drop_receiver", "10ps"),
    (comp_drop_receiver, "wred", "10ps")
)

# Drop Receiver --> Finisher
link_drop_receiver_finisher = sst.Link("link_drop_receiver_finisher")
link_drop_receiver_finisher.connect(
    (comp_drop_receiver, "finisher", "5ps"),
    (comp_finisher, "drop_receiver", "5ps")
)

# Egress Update ---> Packet Buffer
link_egress_pkt_buffer = sst.Link("link_egress_pkt_buffer")
link_egress_pkt_buffer.connect( 
    (comp_pkt_buffer, "egress_ports", "10ps"), 
    (comp_egress_update, "packet_buffer", "10ps"),
)

# WRED --> bytes_tracker
link_wred_bytes_tracker = sst.Link("link_wred_bytes_tracker")
link_wred_bytes_tracker.connect(
    (comp_wred, "bytes_tracker", "5ps"),
    (comp_bytes_tracker, "wred", "5ps")
)


# Bytes Tracker --> MG Bytes Updater
link_bytes_tracker_mg_bytes_updater = sst.Link("link_bytes_tracker_mg_bytes_updater")
link_bytes_tracker_mg_bytes_updater.connect(
    (comp_bytes_tracker, "mg_bytes", "5ps"),
    (comp_mg_bytes_updater, "bytes_tracker", "5ps"),
)

# Bytes Tracker --> QG Bytes Updater
link_bytes_tracker_qg_bytes_updater = sst.Link("link_bytes_tracker_qg_bytes_updater")
link_bytes_tracker_qg_bytes_updater.connect(
    (comp_bytes_tracker, "qg_bytes", "5ps"),
    (comp_qg_bytes_updater, "bytes_tracker", "5ps"),
)

# Bytes Tracker --> QG Bytes Updater
link_bytes_tracker_pq_bytes_updater = sst.Link("link_bytes_tracker_pq_bytes_updater")
link_bytes_tracker_pq_bytes_updater.connect(
    (comp_bytes_tracker, "pq_bytes", "5ps"),
    (comp_pq_bytes_updater, "bytes_tracker", "5ps"),
)

# Packet buffer --> Global Pool Util
link_pacekt_buffer_global_pool_util = sst.Link("link_pacekt_buffer_global_pool_util")
link_pacekt_buffer_global_pool_util.connect(
    (comp_pkt_buffer, "buffer_manager", "10ps"),
    (comp_global_pool_util, "packet_buffer", "10ps"),
)

# Utilization Reporter MG --> Acceptance Checker 
link_mg_reporter_acceptance_checker = sst.Link("link_mg_reporter_acceptance_checker")
link_mg_reporter_acceptance_checker.connect(
    (comp_mg_util_reporter, "acceptance_checker", "5ps"),
    (comp_acceptance_checker, "mg_utilization", "5ps"),
)

# Utilization Reporter QG --> Acceptance Checker 
link_qg_reporter_acceptance_checker = sst.Link("link_qg_reporter_acceptance_checker")
link_qg_reporter_acceptance_checker.connect(
    (comp_qg_util_reporter, "acceptance_checker", "5ps"),
    (comp_acceptance_checker, "qg_utilization", "5ps"),
)

# Utilization Reporter PQ --> Acceptance Checker 
link_pq_reporter_acceptance_checker = sst.Link("link_pq_reporter_acceptance_checker")
link_pq_reporter_acceptance_checker.connect(
    (comp_pq_util_reporter, "acceptance_checker", "5ps"),
    (comp_acceptance_checker, "pq_utilization", "5ps"),
)
# ------------------------------------------------------------------------------------------------------------

# ------------------------------------------------------------------------------------------------------------
# Statistics

# Packet buffer
comp_pkt_buffer.enableStatistics(
    ["blocks_available"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
    }
)

# Ingress ports
for ingress_port in comp_ingress_ports:
    ingress_port.enableStatistics(
        ["priorities"],
        {
            "type":"sst.HistogramStatistic",
            "minvalue" : "0", 
            "binwidth" : "1", 
            "numbins"  : "8", 
            "IncludeOutOfBounds" : "1", 
        }
    )

    ingress_port.enableStatistics(
        ["sent_packets"],
        {
            "type":"sst.AccumulatorStatistic",
        }
    )

    ingress_port.enableStatistics(
        ["bytes_sent"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event"
        }
    )

# Egress ports
egress_ports_receiving_traffic = [0, 1]
for egress_port in egress_ports_receiving_traffic:
    comp_egress_ports[egress_port].enableStatistics(
        ["priorities"],
        {
            "type":"sst.HistogramStatistic",
            "minvalue" : "0", 
            "binwidth" : "1", 
            "numbins"  : "8", 
            "IncludeOutOfBounds" : "1", 
        }
    )

    comp_egress_ports[egress_port].enableStatistics(
        ["bytes_sent"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
        }
    )

    comp_egress_ports[egress_port].enableStatistics(
        ["sent_packets"],
        {
            "type":"sst.AccumulatorStatistic",
        }
    )

    comp_egress_ports[egress_port].enableStatistics(
        ["current_priority"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

# Drop Receiver
comp_drop_receiver.enableStatistics(
    ["dropped_packets"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate":"1 event",
    }
)

comp_drop_receiver.enableStatistics(
    ["destination"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "52", 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_drop_receiver.enableStatistics(
    ["source"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "52", 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_drop_receiver.enableStatistics(
    ["priority"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "8", 
        "IncludeOutOfBounds" : "1", 
        "rate":"1 event",
    }
)

# Memory Groups
for mg in comp_mg_elements:
    comp_mg_elements[mg].enableStatistics(
        ["bytes_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_mg_elements[mg].enableStatistics(
        ["parent_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_mg_elements[mg].enableStatistics(
        ["utilization"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

# Queue Groups
for qg in egress_ports_receiving_traffic:
    comp_qg_elements[qg].enableStatistics(
        ["bytes_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_qg_elements[qg].enableStatistics(
        ["parent_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_qg_elements[qg].enableStatistics(
        ["utilization"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

# Physical Queue
for port in egress_ports_receiving_traffic:
    for priority in range(0,8):
        comp_pq_elements[(port, priority)].enableStatistics(
            ["bytes_util"],
            {
                "type":"sst.AccumulatorStatistic",
                "rate": "1 event",
                "resetOnRead" : True,
            }
        )

        comp_pq_elements[(port, priority)].enableStatistics(
            ["parent_util"],
            {
                "type":"sst.AccumulatorStatistic",
                "rate": "1 event",
                "resetOnRead" : True,
            }
        )

        comp_pq_elements[(port, priority)].enableStatistics(
            ["utilization"],
            {
                "type":"sst.AccumulatorStatistic",
                "rate": "1 event",
                "resetOnRead" : True,
            }
        )

transmitter.enableStatistics(
    ["cWnd"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
        "resetOnRead" : True,
    }
)
