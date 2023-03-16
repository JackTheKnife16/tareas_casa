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
    except Exception as e:
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
    except Exception as e:
        logging.error("Can't parse %s", args_json["ma_config"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["ma_config"])
    exit(1)

# Loadig nerwork table file
if os.path.isfile(args_json["network_table"]):
    try:
        with open(args_json["network_table"]) as jsonfile:
            network_table = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", args_json["network_table"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["network_table"])
    exit(1)

# Loadig L4S config file
if os.path.isfile(args_json["l4s_config"]):
    try:
        with open(args_json["l4s_config"]) as jsonfile:
            l4s_config = json.load(jsonfile)
    except Exception as e:
        logging.error("Can't parse %s", args_json["l4s_config"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["l4s_config"])
    exit(1)

# Loading ports config file
if os.path.isfile(args_json["ports_config"]):
    try:
        with open(args_json["ports_config"]) as jsonfile:
            ports_config = json.load(jsonfile)
    except Exception as e:
        logging.error("Can't parse %s", args_json["ports_config"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["ports_config"])
    exit(1)

traffic_config_file = "tests/qs_1_0/dualq_coupled_aqm/traffic_config_dualq.json"
# Loading ports config file
if os.path.isfile(traffic_config_file):
    try:
        with open(traffic_config_file) as jsonfile:
            traffic_config = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", traffic_config_file)
        exit(1)
else:
    logging.error("File %s does not exist", traffic_config_file)
    exit(1)

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

def set_up_host(ip_address, mac_address, network_table_path, priority, port_number, speed, app_name, app_params, verbose=0):
    # TCP model
    host = sst.Component("host_%d" % (port_number), "networkTraffic.NetworkHost")
    host.addParams({
        "ip_address": ip_address,
        "mac_address": mac_address,
        "network_table": network_table_path,
        "priority": priority,
        "verbose" : verbose,
    })
    sch = host.setSubComponent("event_scheduler", "networkTraffic.EventScheduler", 0)
    sch.addParams({
        "verbose" : verbose,
    })

    # Host Port -----------
    phys_port = sst.Component("phys_port_%d" % (port_number), "networkTraffic.NetworkPort")
    phys_port.addParams({
        "number": port_number,
        "verbose" : verbose,
    })
    # -----------------------

    # Host Layers --------------------

    # Physical
    phys = host.setSubComponent("physical", "networkTraffic.PhysicalLayer", 0)
    phys.addParams({
        "host_bandwidth": speed,
        "verbose" : verbose,
    })

    # Data Link
    dlink = host.setSubComponent("data_link", "networkTraffic.EthernetDataLinkLayer", 0)
    dlink.addParams({
        "dot1q": 0,
        "verbose" : verbose,
    })

    # Network
    net = host.setSubComponent("network", "networkTraffic.IpNetworkLayer", 0)
    net.addParams({
        "ipv6": 0,
        "verbose" : verbose,
    })

    # Transport
    transp = host.setSubComponent("transport", "networkTraffic.TCPTransportLayer", 0)
    transp.addParams({
        "verbose" : verbose,
    })

    # Applications
    application = host.setSubComponent("applications", app_name, 0)
    application.addParams(app_params)

    # Connecting host

    # Port <---> Physical Layer
    link_port_physical = sst.Link("link_port_physical_%d" % (port_number))
    link_port_physical.connect(
        (phys, "network_port", "10ps"),
        (phys_port, "physical_connection", "10ps"),
    )

    return phys_port, application

host_ports = {}

# "port"
# "priority"
# "bw"
# "dest"
# "is_host"
for host_config in traffic_config:
    if host_config["is_host"]:
        src_net_entry = network_table[host_config["port"]]
        dest_net_entry = network_table[host_config["dest"]]

        print('''
Host %d
port: %d
priority: %d
bw: %sb/s
src mac: %s
src ip: %s
receiver: %d
dest port: %d
dest ip: %s
        ''' %(host_config["port"], host_config["port"], host_config["priority"],
        host_config["bw"], src_net_entry["mac_address"], src_net_entry["ip_address"],
        host_config["receiver"], host_config["dest"], dest_net_entry["ip_address"]))

        if not host_config["receiver"]:
            congestion_control = 2
            use_ecn = 1
            if host_config["port"] == 2:
                congestion_control = 0
                use_ecn = 1

            app_name = "networkTraffic.BasicTransmitter"
            app_params = {
                "peer_ip_addr": dest_net_entry["ip_address"],
                "peer_port": 8080,
                "num_packets": 1000,
                "packet_size": 1024,
                "use_ecn": use_ecn,
                "congestion_algorithm": congestion_control,
                "data_rate": host_config["bw"] + "b/s",
                "verbose" : verbosity_option,
            }
        else:
            app_name = "networkTraffic.BasicReceiver"
            app_params = {
                "use_ecn": 1,
                "congestion_algorithm": 2,
                "source_port": 8080,
                "verbose" : verbosity_option,
            }


        phys_port, app = set_up_host(
            src_net_entry["ip_address"],
            src_net_entry["mac_address"],
            args_json["network_table"], 
            host_config["priority"], 
            host_config["port"], 
            host_config["bw"] + "b/s", 
            app_name, 
            app_params,
        )
        host_ports[host_config["port"]] = phys_port

        if not host_config["receiver"]:
            app.enableStatistics(
                ["cWnd"],
                {
                    "type":"sst.AccumulatorStatistic",
                    "rate": "1 event",
                    "resetOnRead" : True,
                }
            )
# ---------------------------

# Packet Generator ---------------------------------------------------------------------------------
comp_packet_generator = sst.Component("traffic_generator", "TRAFFIC_GEN_1_0.TrafficGenerator")
comp_packet_generator.addParams({
    "verbose" : verbosity_option,
})

subcomp_traffic_profile = comp_packet_generator.setSubComponent("traffic_profile", "TRAFFIC_GEN_1_0.ConstantTraffic", 0)
subcomp_traffic_profile.addParams({
    "frontplane_config": args_json["ports_config"],
    "traffic_config": traffic_config_file,
    "num_ports" : num_ports,
    "seed" : 1447,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# Finisher ------------------------------------------------------------------------------------------
comp_finisher = sst.Component("finisher", "TRAFFIC_GEN_1_0.Finisher")
comp_finisher.addParams({
    "num_ports" : num_ports,
    "num_nodes" : 1,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# Acceptance Check ----------------------------------------------------------------------------------
comp_acceptance_checker = sst.Component("acceptance_checker", "QS_1_0.DualQAcceptanceChecker")
comp_acceptance_checker.addParams({
    "ma_config_file" : args_json["ma_config"],
    "num_ports" : num_ports,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------

# List with ports and receivers ---------------------------------------------------------------------
comp_ingress_ports = []
# iterate the number of ports 0-47 --> 5 Gb/s 48-51 --> 25 Gb/s
i = 0
for port in ports_config:
    # Ingress port [i] -------------------------------------------------------------------------
    if i not in host_ports:
        comp_ingress_port = sst.Component("ingress_port_%d" % (i), "QS_1_0.IngressPort")
        params = {
            "port" : i,
            "node" : 0,
            "port_bw": port["bw"] + "b/s",
            "offered_load" : 98.00,
            "verbose" : verbosity_option,
        }
        comp_ingress_port.addParams(params)
    else:
        comp_ingress_port = sst.Component("ingress_port_%d" % (i), "QS_1_0.HostIngressPort")
        params = {
            "port" : i,
            "node" : 0,
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

# ECN Classifier ---------------------------------------------------------------------------------------------
comp_ecn_classifier = sst.Component("ecn_classifier", "QS_1_0.ECNClassifier")
comp_ecn_classifier.addParams({
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Classic WRED -----------------------------------------------------------------------------------------------
comp_classic_wred = sst.Component("classic_wred", "QS_1_0.DualQClassicWRED")
comp_classic_wred.addParams({
    "k_proportionality": 2,
    "ma_config_file" : args_json["ma_config"],
    "port_config_file": args_json["ports_config"],

    "enable_ecn" : 1,
    "ecn_config_file" : args_json["ecn_config"],
    "num_ports": num_ports,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# L4S WRED -----------------------------------------------------------------------------------------------
comp_l4s_wred = sst.Component("l4s_wred", "QS_1_0.L4SWRED")
comp_l4s_wred.addParams({
    "ma_config_file" : args_json["l4s_config"],
    "port_config_file": args_json["ports_config"],
    "num_ports": num_ports,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Packet Buffer Event Forwarder -----------------------------------------------------------------------------------------------
comp_packet_buffer_event_forwarder = sst.Component("packet_buffer_event_forwarder", "QS_1_0.EventForwarder")
comp_packet_buffer_event_forwarder.addParams({
    "num_inputs": 2,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# Drop Receiver Event Forwarder -----------------------------------------------------------------------------------------------
comp_drop_receiver_event_forwarder = sst.Component("drop_receiver_event_forwarder", "QS_1_0.EventForwarder")
comp_drop_receiver_event_forwarder.addParams({
    "num_inputs": 2,
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
    "num_ports" : num_ports,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------

# MEMORY ACCOUNTING ------------------------------------------------------------------------------------------
# Global Pool Util ----------------------------------------------------------------------------------------
comp_global_pool_util = sst.Component("global_pool_util", "QS_1_0.GlobalPoolUtil")
comp_global_pool_util.addParams({
    "ma_config_file" : args_json["ma_config"],
    "buffer_size" : 65536,
    "num_ports" : num_ports,
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------
# Bytes Tracker --------------------------------------------------------------------------------------
comp_bytes_tracker = sst.Component("bytes_tracker", "QS_1_0.DualQBytesTracker")
comp_bytes_tracker.addParams({
    "ma_config_file": args_json["ma_config"],
    "num_ports" : num_ports,
    "verbose" : verbosity_option,
})
# ----------------------------------------------------------------------------------------------------ma_config

# List of fifos, scheduler and egress ports ----------------------------------------------------------
comp_ports_fifos = []
subcomp_priority_queues = []
comp_l4s_ports_fifos = []
subcomp_l4s_queues = []
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
    comp_scheduler = sst.Component("scheduler_%d" % (i), "QS_1_0.DualQDWRRScheduler")
    comp_scheduler.addParams({
        "num_queues": 9,
        "highest_priority": 8,
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
        # Priority Queue[i][j] <--> Scheduler[i]
        link_pri_queue_front_size_scheduler = sst.Link("link_pri_queue_front_size_scheduler_%d_%d" % (i,j))
        link_pri_queue_front_size_scheduler.connect(
           (subcomp_pri_queue, "scheduler_sizes", "5ps"),
           (comp_scheduler, "front_size_priority_queue_%d" % (j), "5ps"),
        )
    subcomp_priority_queues.append(subcomp_queues)
    # -----------------------------------------------------------------------------------------------

    # L4S Port FIFO ----------------------------------------------------------------------------------
    comp_l4s_port_fifos = sst.Component("l4s_port_fifos_%d" % (i), "QS_1_0.PortFIFOs")
    comp_l4s_port_fifos.addParams({
        "port" : i,
        "num_queues": 1,
        "verbose" : verbosity_option,
    })
    comp_l4s_ports_fifos.append(comp_l4s_port_fifos)
    # L queue
    subcomp_l_queue = comp_l4s_port_fifos.setSubComponent("priority_queues", "QS_1_0.PriorityQueue", 0)
    subcomp_l_queue.addParams({
        "port": i,
        "verbose" : verbosity_option,
    })
    subcomp_l4s_queues.append(subcomp_l_queue)
    # Connections -----------------------------------------------------------------------------------
    # L Queue[i] <--> Scheduler[i]
    link_l_queue_scheduler = sst.Link("link_l_queue_scheduler_%d_%d" % (i, j))
    link_l_queue_scheduler.connect(
        (subcomp_l_queue, "scheduler", "5ps"),
        (comp_scheduler, "l_priority_queue", "5ps"),
    )
    # L Queue[i] <--> Scheduler[i]
    link_l_pri_queue_front_size_scheduler = sst.Link("link_l_pri_queue_front_size_scheduler_%d_%d" % (i, j))
    link_l_pri_queue_front_size_scheduler.connect(
        (subcomp_l_queue, "scheduler_sizes", "5ps"),
        (comp_scheduler, "l_front_size_priority_queue", "5ps"),
    )
    # --------------------------------------------------------------------------------------------------

    # Egress port [i] -----------------------------------------------------------------------------------
    if i not in host_ports:
        comp_egress_port = sst.Component("egress_port_%d" % (i), "QS_1_0.EgressPort")
        params = {
            "num_ports" : num_ports,
            "port" : i,
            "num_nodes" : 1,
            "node" : 0,
            "port_bw": port["bw"] + "b/s",
            "verbose" : verbosity_option,
        }
        comp_egress_port.addParams(params)
    else:
        comp_egress_port = sst.Component("egress_port_%d" % (i), "QS_1_0.HostEgressPort")
        params = {
            "num_ports" : num_ports,
            "port" : i,
            "num_nodes" : 1,
            "node" : 0,
            "port_bw": port["bw"] + "b/s",
            "verbose" : verbosity_option,
        }
        comp_egress_port.addParams(params)
    comp_egress_ports.append(comp_egress_port)
    # ----------------------------------------------------------------------------------------------------

    # Connections -----------------------------------------------------------------------------------------
    # Classic WRED port fifo [i] <--> Port Fifo [i]
    link_classic_wred_port_fifos = sst.Link("link_classic_wred_port_fifos_%d" % (i))
    link_classic_wred_port_fifos.connect(
        (comp_classic_wred, "port_fifo_%d" % (i), "10ps"),
        (comp_port_fifos, "wred", "10ps"),
    )
    # L4S WRED port fifo [i] <--> Port Fifo [i]
    link_l4s_wred_port_fifos = sst.Link("link_l4s_wred_port_fifos_%d" % (i))
    link_l4s_wred_port_fifos.connect(
        (comp_l4s_wred, "port_fifo_%d" % (i), "10ps"),
        (comp_l4s_port_fifos, "wred", "10ps"),
    )
    # Classic Port FIFO [i] <--> Scheduler [i]
    link_port_fifos_scheduler = sst.Link("link_port_fifos_scheduler_%d" % (i))
    link_port_fifos_scheduler.connect(
        (comp_port_fifos, "packet_selector", "5ps"),
        (comp_scheduler, "port_fifo", "5ps"),
    )
    # L4S Port FIFO [i] <--> Scheduler [i]
    link_l_port_fifos_scheduler = sst.Link("link_l_port_fifos_scheduler_%d" % (i))
    link_l_port_fifos_scheduler.connect(
        (comp_l4s_port_fifos, "packet_selector", "5ps"),
        (comp_scheduler, "l_port_fifo", "5ps"),
    )
    # Scheduler [i] --> Egress Port [i]
    link_scheduler_egress_port = sst.Link("link_scheduler_egress_port_%d" % (i))
    link_scheduler_egress_port.connect(
        (comp_scheduler, "egress_port", "5ps"),
        (comp_egress_port, "scheduler", "5ps"),
    )
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
    # Scheduler [i] --> Bytes tracker [i]
    link_scheduler_bytes_tracker = sst.Link("link_scheduler_bytes_tracker_%d" % (i))
    link_scheduler_bytes_tracker.connect(
        (comp_scheduler, "bytes_tracker", "5ps"),
        (comp_bytes_tracker, "port_scheduler_%d" % (i), "5ps"),
    )
    # Scheduler [i] --> Bytes tracker [i]
    link_l4s_scheduler_bytes_tracker = sst.Link("link_l4s_scheduler_bytes_tracker_%d" % (i))
    link_l4s_scheduler_bytes_tracker.connect(
        (comp_scheduler, "l_bytes_tracker", "5ps"),
        (comp_bytes_tracker, "l_port_scheduler_%d" % (i), "5ps"),
    )
    # -----------------------------------------------------------------------------------------------------

    i += 1

# Setting accounting elements -----------------------------------------------------------------------------
# Memory Group --------------------------------------------------------------------------------------------

# MG LUTs -------------------------------------------------------------------------------------------------
comp_lookup_table_mg = sst.Component("lookup_table_mg", "QS_1_0.LookUpController")
comp_lookup_table_mg.addParams({
    "num_ports": num_ports,
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
    "num_ports" : num_ports,
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
comp_lookup_table_qg_pq = sst.Component("lookup_table_qg_pq", "QS_1_0.DualQLookUpController")
comp_lookup_table_qg_pq.addParams({
    "num_ports": num_ports,
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
    "num_ports" : num_ports,
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
    "num_ports" : num_ports,
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

# L4S Queues --------------------------------------------------------------------------------------------

# LQ Utilization Reporter ---------------------------------------------------------------------------------
comp_lq_util_reporter = sst.Component("lq_util_reporter", "QS_1_0.UtilizationReporter")
comp_lq_util_reporter.addParams({
    "verbose" : verbosity_option,
})
# ---------------------------------------------------------------------------------------------------------

# LQ bytes updater-----------------------------------------------------------------------------------------
comp_lq_bytes_updater = sst.Component("lq_bytes_updater", "QS_1_0.BytesUpdater")
comp_lq_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "type" : 2,
    "num_ports" : num_ports,
    "verbose" : verbosity_option,
})
# Setting connectors and QG elements
comp_lq_elements = []
for port in ports_config:
    # Start index as the port
    i = port["port"]
    # Getting Byte Limit
    found = False
    for bytes_limit_config in l4s_config["Egress_LQ"]["Byte_limits"]:
        if bytes_limit_config["LQ"] == port["bw"]:
            bytes_limit = bytes_limit_config
            found = True
            break 
    if not found:
        logging.fatal("LQ Can't find Bytes Limit for port %d. Please check port config file.", i)
    
    # Getting mapping
    found = False
    for mapping_config in l4s_config["Egress_LQ"]["Mapping"]:
        if mapping_config["LQ"] == port["bw"]:
            mapping = mapping_config
            found = True
            break
    if not found:
        logging.fatal("LQ: Can't find Mapping for port %d. Please check port config file.", i)
    
    logging.debug("LQ %d --> bytes_limit=%d bias=%d", i, bytes_limit["Byte_limit"], mapping["Biasing_value"])

    # LQ element [LQ index] -----------------------------------------------------------------------
    comp_lq_element = sst.Component("lq_%d" % (i), "QS_1_0.AccountingElement")
    comp_lq_element.addParams({
        "type" : 4,
        "index" : i,
        "max_bytes" : bytes_limit["Byte_limit"],
        "bias" : mapping["Biasing_value"],
        "parent_translation_config": args_json["parent_translation"],
        "verbose" : verbosity_option,
    })
    comp_lq_elements.append(comp_lq_element)

    # PQ Element Connector to parent [PQ index] ----------------------------------------------------------------
    subcomp_lq_parent = comp_qg_elements[i].setSubComponent(
        "children", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_lq_parent.addParams({
        "index": str(i),
        "type": "LQ",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # LQ Element Connector to bytes tracker [LQ index] ------------------------------------------------------------------------
    subcomp_lq_connector_bytes = comp_lq_bytes_updater.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_lq_connector_bytes.addParams({
        "index": str(i),
        "type": "LQ",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------
    # LQ Element Connector to Lookup Table [LQ index] ----------------------------------------------------------------
    subcomp_lq_connector_lookup = comp_lookup_table_qg_pq.setSubComponent(
        "lq_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_lq_connector_lookup.addParams({
        "index": str(i),
        "type": "LQ",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------
    # LQ Element Connector to Utilization reporter[LQ index] ----------------------------------------------------------------
    subcomp_lq_connector_util_reporter = comp_lq_util_reporter.setSubComponent(
        "accounting_elements", 
        "QS_1_0.ElementConnector", 
        i
    )
    subcomp_lq_connector_util_reporter.addParams({
        "index": str(i),
        "type": "LQ",
        "verbose" : verbosity_option,
    })
    # -----------------------------------------------------------------------------------------------

    # Connections -----------------------------------------------------------------------------------
    # Global Pool Util [LQ index]  <--> LQ [LQ index]
    link_global_pool_lq = sst.Link("link_global_pool_lq_%d" % (i))
    link_global_pool_lq.connect(
        (comp_lq_element, "parent_utilization", "5ps"),
        (subcomp_lq_parent, "accounting_element", "5ps"),
    )

    # LQ Element Connector Bytes [LQ index]  <--> LQ [LQ index]
    link_connector_bytes_lq = sst.Link("link_connector_bytes_lq_%d" % (i))
    link_connector_bytes_lq.connect(
        (subcomp_lq_connector_bytes, "accounting_element", "5ps"),
        (comp_lq_element, "bytes_tracker", "5ps"),
    )

    # LQ Element Connector LookUp Table [LQ index]  <--> LQ [LQ index]
    link_connector_lookup_lq = sst.Link("link_connector_lookup_lq_%d" % (i))
    link_connector_lookup_lq.connect(
        (subcomp_lq_connector_lookup, "accounting_element", "5ps"),
        (comp_lq_element, "lookup_table", "5ps"),
    )

    # LQ Element Connector Utilization Updater [LQ index]  <--> LQ [LQ index]
    link_connector_lookup_lq = sst.Link("link_connector_util_reporter_lq_%d" % (i))
    link_connector_lookup_lq.connect(
        (subcomp_lq_connector_util_reporter, "accounting_element", "5ps"),
        (comp_lq_element, "child_utilization", "5ps"),
    )
    # -----------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------------------



# Connections ---------------------------------------------------------------------------------------------

# Packet Generator --> Finisher
link_packet_generator_finisher = sst.Link("link_packet_generator_finisher")
link_packet_generator_finisher.connect(
    (comp_packet_generator, "finisher", "5ps"),
    (comp_finisher, "traffic_generator", "5ps")
)

# Acceptance Checker --> ECN Classifier
link_acceptance_checker_ecn_classifier = sst.Link("link_acceptance_checker_ecn_classifier")
link_acceptance_checker_ecn_classifier.connect(
    (comp_acceptance_checker, "wred", "5ps"),
    (comp_ecn_classifier, "in", "5ps")
)

# ECN Classifer --> L4S WRED (Coupling)
link_ecn_classifer_l4s_wred = sst.Link("link_ecn_classifer_l4s_wred")
link_ecn_classifer_l4s_wred.connect(
    (comp_ecn_classifier, "l_check", "5ps"),
    (comp_l4s_wred, "acceptance_checker", "5ps")
)

# ECN Classifer --> Classic WRED
link_ecn_classifier_l4s_wred = sst.Link("link_ecn_classifier_l4s_wred")
link_ecn_classifier_l4s_wred.connect(
    (comp_ecn_classifier, "c_check", "5ps"),
    (comp_classic_wred, "acceptance_checker", "5ps")
)

# Classic WRED --> L4S WRED
link_wred_coupling = sst.Link("link_wred_coupling")
link_wred_coupling.connect(
    (comp_classic_wred, "coupling", "5ps"),
    (comp_l4s_wred, "coupling", "5ps")
)

# Classic WRED --> Bytes Tracker
link_classic_wred_bytes_tracker = sst.Link("link_classic_wred_bytes_tracker")
link_classic_wred_bytes_tracker.connect(
    (comp_classic_wred, "bytes_tracker", "5ps"),
    (comp_bytes_tracker, "wred", "5ps")
)

# L4S WRED --> Bytes Tracker
link_l4s_wred_bytes_tracker = sst.Link("link_l4s_wred_bytes_tracker")
link_l4s_wred_bytes_tracker.connect(
    (comp_l4s_wred, "bytes_tracker", "5ps"),
    (comp_bytes_tracker, "l4s_wred", "5ps")
)

# Classic WRED --> Packet Buffer Event Forwarder
link_classic_wred_packet_buffer_event_forwarder = sst.Link("link_classic_wred_packet_buffer_event_forwarder")
link_classic_wred_packet_buffer_event_forwarder.connect(
    (comp_classic_wred, "packet_buffer", "5ps"),
    (comp_packet_buffer_event_forwarder, "input_0", "5ps")
)

# L4S WRED --> Packet Buffer Event Forwarder
link_l4s_wred_packet_buffer_event_forwarder = sst.Link("link_l4s_wred_packet_buffer_event_forwarder")
link_l4s_wred_packet_buffer_event_forwarder.connect(
    (comp_l4s_wred, "packet_buffer", "5ps"),
    (comp_packet_buffer_event_forwarder, "input_1", "5ps")
)

# Packet Buffer Event Forwarder --> Packet Buffer
link_event_forwarder_packet_buffer = sst.Link("link_event_forwarder_packet_buffer")
link_event_forwarder_packet_buffer.connect(
    (comp_packet_buffer_event_forwarder, "output_0", "5ps"),
    (comp_pkt_buffer, "acceptance_checker", "5ps")
)

# Classic WRED --> Drop Receiver Event Forwarder
link_classic_wred_drop_receiver_event_forwarder = sst.Link("link_classic_wred_drop_receiver_event_forwarder")
link_classic_wred_drop_receiver_event_forwarder.connect(
    (comp_classic_wred, "drop_receiver", "5ps"),
    (comp_drop_receiver_event_forwarder, "input_0", "5ps")
)

# L4S WRED --> Drop Receiver Event Forwarder
link_l4s_wred_drop_receiver_event_forwarder = sst.Link("link_l4s_wred_drop_receiver_event_forwarder")
link_l4s_wred_drop_receiver_event_forwarder.connect(
    (comp_l4s_wred, "drop_receiver", "5ps"),
    (comp_drop_receiver_event_forwarder, "input_1", "5ps")
)

# Drop Receiver Event Forwarder --> Packet Buffer
link_event_forwarder_drop_receiver = sst.Link("link_event_forwarder_drop_receiver")
link_event_forwarder_drop_receiver.connect(
    (comp_drop_receiver_event_forwarder, "output_0", "5ps"),
    (comp_drop_receiver, "wred", "5ps")
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

# Bytes Tracker --> PQ Bytes Updater
link_bytes_tracker_pq_bytes_updater = sst.Link("link_bytes_tracker_pq_bytes_updater")
link_bytes_tracker_pq_bytes_updater.connect(
    (comp_bytes_tracker, "pq_bytes", "5ps"),
    (comp_pq_bytes_updater, "bytes_tracker", "5ps"),
)

# Bytes Tracker --> LQ Bytes Updater
link_bytes_tracker_lq_bytes_updater = sst.Link("link_bytes_tracker_lq_bytes_updater")
link_bytes_tracker_lq_bytes_updater.connect(
    (comp_bytes_tracker, "lq_bytes", "5ps"),
    (comp_lq_bytes_updater, "bytes_tracker", "5ps"),
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

# Utilization Reporter LQ --> Acceptance Checker 
link_lq_reporter_acceptance_checker = sst.Link("link_lq_reporter_acceptance_checker")
link_lq_reporter_acceptance_checker.connect(
    (comp_lq_util_reporter, "acceptance_checker", "5ps"),
    (comp_acceptance_checker, "lq_utilization", "5ps"),
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

# WRED
comp_classic_wred.enableStatistics(
    ["ce_packets"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
    }
)

comp_classic_wred.enableStatistics(
    ["ce_packets_by_dest"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "%d" % (num_ports), 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_classic_wred.enableStatistics(
    ["ce_packets_by_src"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "%d" % (num_ports), 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_l4s_wred.enableStatistics(
    ["ce_packets"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
    }
)

comp_l4s_wred.enableStatistics(
    ["ce_packets_by_dest"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "%d" % (num_ports), 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_l4s_wred.enableStatistics(
    ["ce_packets_by_src"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "%d" % (num_ports), 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_classic_wred.enableStatistics(
    ["p_C"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
        "resetOnRead" : True,
    }
)

comp_l4s_wred.enableStatistics(
    ["p_CL"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
        "resetOnRead" : True,
    }
)

comp_l4s_wred.enableStatistics(
    ["p_L"],
    {
        "type":"sst.AccumulatorStatistic",
        "rate": "1 event",
        "resetOnRead" : True,
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
egress_ports_receiving_traffic = [0, 1, 2]
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

    comp_egress_ports[egress_port].enableStatistics(
        ["bytes_sent_by_port"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
        }
    )

    comp_egress_ports[egress_port].enableStatistics(
        ["bytes_sent_by_priority"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
        }
    )

    comp_schedulers[egress_port].enableStatistics(
        ["queue_serviced"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_schedulers[egress_port].enableStatistics(
        ["source_serviced"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )
    
    for j in range(8):
        subcomp_priority_queues[egress_port][j].enableStatistics(
            ["num_packets"],
            {
                "type":"sst.AccumulatorStatistic",
                "rate": "1 event",
            }
        )

        subcomp_priority_queues[egress_port][j].enableStatistics(
            ["num_bytes"],
            {
                "type":"sst.AccumulatorStatistic",
                "rate": "1 event",
            }
        )
    
    subcomp_l4s_queues[egress_port].enableStatistics(
        ["num_packets"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
        }
    )

    subcomp_l4s_queues[egress_port].enableStatistics(
        ["num_bytes"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
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
        "numbins"  : "%d" % (num_ports), 
        "IncludeOutOfBounds" : "1", 
    }
)

comp_drop_receiver.enableStatistics(
    ["source"],
    {
        "type":"sst.HistogramStatistic",
        "minvalue" : "0", 
        "binwidth" : "1", 
        "numbins"  : "%d" % (num_ports), 
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

# L4S Queues
for lq in egress_ports_receiving_traffic:
    comp_lq_elements[lq].enableStatistics(
        ["bytes_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_lq_elements[lq].enableStatistics(
        ["parent_util"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

    comp_lq_elements[lq].enableStatistics(
        ["utilization"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
            "resetOnRead" : True,
        }
    )

