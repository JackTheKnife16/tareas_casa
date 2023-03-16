import sst
import argparse
import json
import os
import logging

# Logging format function
def set_logging_conf(log_mode):
   logging.basicConfig(level=log_mode, format='%(asctime)s: %(levelname)s: %(message)s')

# Parser object
parser = argparse.ArgumentParser(description="Memory accounting basic SST tests")

# Optional aguments
parser.add_argument('-d', '--debug', required=False,
                    help="Debug Mode", action='store_true')

# Required arguments
required_arguments = parser.add_argument_group('required arguments')
required_arguments.add_argument('-a', '--args_json', required=True,
                                help="Arguments JSON file.")

args = parser.parse_args()

# Logging mode
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

# Loading ports config file
if os.path.isfile(args_json["ports_config"]):
    try:
        with open(args_json["ports_config"]) as jsonfile:
            ports_config = json.load(jsonfile)
    except Exeption as e:
        logging.error("Can't parse %s", args_json["ports_config"])
        exit(1)
else:
    logging.error("File %s does not exist", args_json["ports_config"])
    exit(1)


# Define SST core options
sst.setProgramOption("timebase", "1ps")

# Number of ports
num_ports = len(ports_config)

# Number of nodes
num_nodes = 1

# Number of queues per ports
num_queues_per_port = 8

# Enable statistics level 7
sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputCSV",
                        {
                            "filepath" : args_json["stats_csv"],
                            "separator" : ","
                        })

#-----------------------------------------------------------------------------------------------------------------------------
# Miscellaneous Components
#-----------------------------------------------------------------------------------------------------------------------------

comp_finisher = sst.Component("finisher", "TRAFFIC_GEN_1_0.Finisher")
comp_finisher.addParams({
    "num_ports" : num_ports,
    "num_nodes" : 1,
    "verbose" : 0,
})

comp_drop_receiver = sst.Component("drop_receiver", "QS_2_0.DropReceiver")
comp_drop_receiver.addParams({
    "verbose" : 0,
})

#-----------------------------------------------------------------------------------------------------------------------------
# Ingress DataPath Components
#-----------------------------------------------------------------------------------------------------------------------------

# Packet Generator
comp_packet_generator = sst.Component("traffic_generator", "TRAFFIC_GEN_1_0.TrafficGenerator")
comp_packet_generator.addParams({
    "num_ports" : num_ports,
    "num_nodes": num_nodes,
    "verbose" : 1,
})

subcomp_traffic_profile = comp_packet_generator.setSubComponent("traffic_profile", "TRAFFIC_GEN_1_0.OneToOneFixed", 0)
subcomp_traffic_profile.addParams({
    "frontplane_config": args_json["ports_config"],
    "num_packets": 50,
    "packet_size": 512,
    "num_nodes": num_nodes,
    "num_ports": num_ports,
    "seed" : 1447,
    "verbose" : 1,
})

# Ingress Forwarding
comp_ingress_forwarding_engine = sst.Component("ingress_forwarding_engine", "QS_2_0.IngressForwardingEngine")
comp_ingress_forwarding_engine.addParams({
    "verbose" : 1,
    "num_ports" : num_ports,
})

# Ingress ports
comp_ingress_ports = []
i = 0
for port in ports_config:
    comp_ingress_port = sst.Component("ingress_port_%d" % (i), "QS_2_0.IngressPort")
    params = {
        "port" : i,
        "port_bw": port["bw"] + "b/s",
        "verbose" : 1,
    }

    comp_ingress_port.addParams(params)
    comp_ingress_ports.append(comp_ingress_port)
    
    link_packet_gen_ingress_port = sst.Link("link_packet_gen_ingress_port_%d" % (i))
    link_packet_gen_ingress_port.connect(
        (comp_packet_generator, "ingress_port_0_%d" % (i), "10ps"),
        (comp_ingress_port, "traffic_generator", "10ps"),
    )

    link_ingress_port_ingress_forwarding_engine = sst.Link("link_ingress_port_ingress_forwarding_engine_%d" % (i))
    link_ingress_port_ingress_forwarding_engine.connect(
        (comp_ingress_port, "acceptance_checker", "10ps"),
        (comp_ingress_forwarding_engine, "ingress_port_%d" % (i), "10ps")
    )
    i += 1

#-----------------------------------------------------------------------------------------------------------------------------
# Acceptance Check and WRED Components
#-----------------------------------------------------------------------------------------------------------------------------

# Acceptance check
comp_acceptance_checker = sst.Component("acceptance_checker", "QS_2_0.AcceptanceChecker")
comp_acceptance_checker.addParams({
    "ma_config_file" : args_json["ma_config"],
    "num_ports" : num_ports,
    "verbose" : 1,
})

# WRED
comp_wred = sst.Component("wred", "QS_2_0.WRED")
comp_wred.addParams({
    "ma_config_file" : args_json["ma_config"],
    "port_config_file": args_json["ports_config"],
    "verbose" : 1,
})

#-----------------------------------------------------------------------------------------------------------------------------
# Memory Accounting Components
#-----------------------------------------------------------------------------------------------------------------------------

# Global Pool
comp_global_pool_util = sst.Component("global_pool_util", "QS_2_0.GlobalPoolUtil")
comp_global_pool_util.addParams({
    "ma_config_file" : args_json["ma_config"],
    "num_ports" : num_ports,
    "buffer_size" : 65536,
    "verbose" : 0,
})

# Bytes Tracker
comp_bytes_tracker = sst.Component("bytes_tracker", "QS_2_0.BytesTracker")
comp_bytes_tracker.addParams({
    "ma_config_file": args_json["ma_config"],
    "num_ports" : num_ports,
    "verbose" : 0,
})

# Memory Group Lookup Tables
comp_lookup_table_mg = sst.Component("lookup_table_mg", "QS_2_0.LookUpController")
comp_lookup_table_mg.addParams({
    "verbose" : 0,
    "num_ports" : num_ports,
})

MG_lookup_tables = 4
for i in range(0, MG_lookup_tables):
    subcomp_lookup_table = comp_lookup_table_mg.setSubComponent(
        "look_up_table", 
        "QS_2_0.LookUpTable", 
        i,
    )
    subcomp_lookup_table.addParams({
        "table_file": "%s%d.json" % (args_json["mg_lut_root"], i),
        "verbose" : 0,
    })

# Memory Group Utilization Reporter
comp_mg_util_reporter = sst.Component("mg_util_reporter", "QS_2_0.UtilizationReporter")
comp_mg_util_reporter.addParams({
    "verbose" : 0,
})

# Memory Group bytes updater
comp_mg_bytes_updater = sst.Component("mg_bytes_updater", "QS_2_0.BytesUpdater")
comp_mg_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "type" : 1,
    "num_ports" : num_ports,
    "verbose" : 0,
})

# Memory Group connectors and elements
comp_mg_elements = {}
i = 0
for memory_group in ma_config["Egress_MG"]["Mapping"]:
    bytes_limit = ma_config["Egress_MG"]["Byte_limits"][i]
    
    logging.debug("MG %d --> bytes_limit=%d bias=%d", memory_group["MG"], bytes_limit["Byte_limit"], memory_group["Biasing_value"])
    
    # Memory Group element [MG index]
    comp_mg_element = sst.Component("mg_%d" % (memory_group["MG"]), "QS_2_0.AccountingElement")
    comp_mg_element.addParams({
        "type" : 1,
        "index" : memory_group["MG"],
        "max_bytes" : bytes_limit["Byte_limit"],
        "bias" : memory_group["Biasing_value"],
        "parent_translation_config": args_json["parent_translation"],
        "verbose" : 0,
    })
    comp_mg_elements[memory_group["MG"]] = comp_mg_element

    # Memory Group Element Connector to bytes tracker [MG index]
    subcomp_mg_connector_bytes = comp_mg_bytes_updater.setSubComponent(
        "accounting_elements", 
        "QS_2_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_bytes.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : 0,
    })

    # Memory Group Element Connector to Lookup Table[MG index]
    subcomp_mg_connector_lookup = comp_lookup_table_mg.setSubComponent(
        "mg_elements", 
        "QS_2_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_lookup.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : 0,
    })

    # Memory Group Element Connector to Utilization reporter[MG index]
    subcomp_mg_connector_util_reporter = comp_mg_util_reporter.setSubComponent(
        "accounting_elements", 
        "QS_2_0.ElementConnector", 
        memory_group["MG"]
    )
    subcomp_mg_connector_util_reporter.addParams({
        "index": str(memory_group["MG"]),
        "type": "MG",
        "verbose" : 0,
    })

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

    i += 1

# Queue Group and Priority Queus Lookup Tables
comp_lookup_table_qg_pq = sst.Component("lookup_table_qg_pq", "QS_2_0.LookUpController")
comp_lookup_table_qg_pq.addParams({
    "num_ports" : num_ports,
    "verbose" : 0,
})

QG_PQ_lookup_tables = 32
for i in range(0, QG_PQ_lookup_tables):
    subcomp_lookup_table = comp_lookup_table_qg_pq.setSubComponent(
        "look_up_table", 
        "QS_2_0.LookUpTable", 
        i,
    )
    subcomp_lookup_table.addParams({
        "table_file": "%s%d.json" % (args_json["qg_pq_lut_root"], i),
        "num_ports" : num_ports,
        "verbose" : 0,
    })

# Queue Group

# Queue Group Utilization Reporter
comp_qg_util_reporter = sst.Component("qg_util_reporter", "QS_2_0.UtilizationReporter")
comp_qg_util_reporter.addParams({
    "verbose" : 0,
})

# Queue Group bytes updater
comp_qg_bytes_updater = sst.Component("qg_bytes_updater", "QS_2_0.BytesUpdater")
comp_qg_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "num_ports" : num_ports,
    "type" : 2,
    "verbose" : 0,
})

# Queue Group connectors and elements
comp_qg_elements = []
for port in ports_config:
    i = port["port"]
    # Byte Limit
    found = False
    for bytes_limit_config in ma_config["Egress_QG"]["Byte_limits"]:
        if bytes_limit_config["QG"] == port["bw"]:
            bytes_limit = bytes_limit_config
            found = True
            break 
    if not found:
        logging.fatal("QG: Can't find Bytes Limit for port %d. Please check port config file.", i)
    
    # Mapping
    found = False
    for mapping_config in ma_config["Egress_QG"]["Mapping"]:
        if mapping_config["QG"] == port["bw"]:
            mapping = mapping_config
            found = True
            break
    if not found:
        logging.fatal("QG: Can't find Mapping for port %d. Please check port config file.", i)
    
    logging.debug("QG %d --> bytes_limit=%d bias=%d", i, bytes_limit["Byte_limit"], mapping["Biasing_value"])

    # QG element [QG index]
    comp_qg_element = sst.Component("qg_%d" % (i), "QS_2_0.AccountingElement")
    comp_qg_element.addParams({
        "type" : 2,
        "index" : i,
        "max_bytes" : bytes_limit["Byte_limit"],
        "bias" : mapping["Biasing_value"],
        "parent_translation_config": args_json["parent_translation"],
        "verbose" : 0,
    })
    comp_qg_elements.append(comp_qg_element)

    # QG Element Connector to bytes tracker [QG index]
    subcomp_qg_connector_bytes = comp_qg_bytes_updater.setSubComponent(
        "accounting_elements", 
        "QS_2_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_bytes.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : 0,
    })

    # QG Element Connector to Lookup Table [QG index]
    subcomp_qg_connector_lookup = comp_lookup_table_qg_pq.setSubComponent(
        "qg_elements", 
        "QS_2_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_lookup.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : 0,
    })

    # QG Element Connector to Utilization reporter[QG index]
    subcomp_qg_connector_util_reporter = comp_qg_util_reporter.setSubComponent(
        "accounting_elements", 
        "QS_2_0.ElementConnector", 
        i
    )
    subcomp_qg_connector_util_reporter.addParams({
        "index": str(i),
        "type": "QG",
        "verbose" : 0,
    })

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

# Physical Queue

# Physical Queue Bytes Updater
comp_pq_bytes_updater = sst.Component("pq_bytes_updater", "QS_2_0.BytesUpdater")
comp_pq_bytes_updater.addParams({
    "ma_config_file" : args_json["ma_config"],
    "num_ports" : num_ports,
    "type" : 3,
    "verbose" : 0,
})

# Physical Queue Utilization Reporter
comp_pq_util_reporter = sst.Component("pq_util_reporter", "QS_2_0.UtilizationReporter")
comp_pq_util_reporter.addParams({
    "verbose" : 0,
})

# Physical Queue connectors and elements
comp_pq_elements = {}
pq_index = 0
for port in ports_config:
    i = port["port"]

    # 8 priorities
    for j in range(0, 8):
        if port["bw"] in ma_config["Egress_PQ"]["Byte_limits"][j]:
            bytes_limit = ma_config["Egress_PQ"]["Byte_limits"][j][port["bw"]]
        else:
            logging.fatal("PQ: Can't find Bytes Limit for port %d. Please check port config file.", i)

        mapping = ma_config["Egress_PQ"]["Mapping"][j]

        logging.debug("PQ %d,%d --> bytes_limit=%d bias=%d", i, j, bytes_limit, mapping["Biasing_value"])
        
        # PQ element [PQ index]
        comp_pq_element = sst.Component("pq_%d_%d" % (i, j), "QS_2_0.AccountingElement")
        comp_pq_element.addParams({
            "type" : 3,
            "index" : i,
            "priority": j,
            "max_bytes" : bytes_limit,
            "bias" : mapping["Biasing_value"],
            "parent_translation_config": args_json["parent_translation"],
            "verbose" : 0,
        })
        comp_pq_elements[(i, j)] = comp_pq_element

        # PQ Element Connector [PQ index]
        subcomp_pq_connector = comp_pq_bytes_updater.setSubComponent(
            "accounting_elements", 
            "QS_2_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : 0,
        })

        # PQ Element Connector to Lookup Table [PQ index]
        subcomp_pq_connector_lookup = comp_lookup_table_qg_pq.setSubComponent(
            "pq_elements", 
            "QS_2_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector_lookup.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : 0,
        })

        # QG Element Connector to Utilization reporter[QG index]
        subcomp_pq_connector_util_reporter = comp_pq_util_reporter.setSubComponent(
            "accounting_elements", 
            "QS_2_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_connector_util_reporter.addParams({
            "index": "%d_%d" % (i, j),
            "type": "QG",
            "verbose" : 0,
        })

        # PQ Element Connector to parent [PQ index]
        subcomp_pq_parent = comp_mg_elements[mapping["MG_parent"]].setSubComponent(
            "children", 
            "QS_2_0.ElementConnector", 
            pq_index
        )
        subcomp_pq_parent.addParams({
            "index": "%d_%d" % (i, j),
            "type": "PQ",
            "verbose" : 0,
        })
        
        pq_index += 1

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


#-----------------------------------------------------------------------------------------------------------------------------
# Unicast TM Components
#-----------------------------------------------------------------------------------------------------------------------------

comp_queue_service_tracker = sst.Component("queue_service_tracker", "QS_2_0.QueueServiceTracker")
comp_queue_service_tracker.addParams({
    "verbose" : 1,
    "num_ports" : num_ports,
    "num_queues_per_port" : num_queues_per_port,
})

comp_port_bank = sst.Component("port_bank", "QS_2_0.PortBank")
comp_port_bank.addParams({
    "verbose" : 1,
    "num_ports" : num_ports,
})

# Port schedulers

subcomp_port_scheduler_list = []
i = 0
for port in ports_config:
   subcomp_port_scheduler = comp_port_bank.setSubComponent("port_scheduler", "QS_2_0.PortScheduler", i)
   subcomp_port_scheduler.addParams({
      "port": i,
      "verbose" : 1,
   })
   subcomp_port_scheduler_list.append(subcomp_port_scheduler)

#   subcomp_port_scheduler.enableStatistics(
#      ["outcoming_voq_packets"],
#      {
#         "type":"sst.HistogramStatistic",
#         "minvalue" : "0", 
#         "binwidth" : "1", 
#         "numbins"  : "16", 
#         "IncludeOutOfBounds" : "1", 
#         "rate":"1 event",
#      }  
#   ) 

   i += 1

#-----------------------------------------------------------------------------------------------------------------------------
# Memory System Components
#-----------------------------------------------------------------------------------------------------------------------------

comp_memory_system_queue_handler = sst.Component("memory_system_queue_handler", "QS_2_0.MemorySystemQueueHandler")
comp_memory_system_queue_handler.addParams({
    "verbose" : 1,
})

#subcomp_memory_system_queue_list = []
#i = 0
#for port in ports_config:
#      for j in range(8):
#         subcomp_memory_system_queue = comp_memory_system_queue_handler.setSubComponent("memory_system_queue", "QS_2_0.MemorySystemQueue", i * 8 + j)
#         subcomp_memory_system_queue.addParams({
#            "verbose" : 1,
#         })
#         subcomp_memory_system_queue_list.append(subcomp_memory_system_queue)
#  
#      i += 1


comp_pkt_buffer = sst.Component("packet_buffer", "QS_2_0.PacketBuffer")
comp_pkt_buffer.addParams({
	"initial_blocks" : 65536,
    "verbose" : 1,
})      

comp_memory_system_puller = sst.Component("memory_system_puller", "QS_2_0.MemorySystemPuller")
comp_memory_system_puller.addParams({
    "verbose" : 1,
    "num_ports" : num_ports,
})

subcomp_memory_system_puller_port_list = []
i = 0
for port in ports_config:
      subcomp_memory_system_puller_port = comp_memory_system_puller.setSubComponent("memory_system_puller_port", "QS_2_0.MemorySystemPullerPort", i)
      subcomp_memory_system_puller_port.addParams({
         "port": i,
         "verbose" : 1,
      })
      subcomp_memory_system_puller_port_list.append(subcomp_memory_system_puller_port)
      
      i += 1


#-----------------------------------------------------------------------------------------------------------------------------
# Egress Datapath Components
#-----------------------------------------------------------------------------------------------------------------------------      

comp_egress_forwarding_engine = sst.Component("egress_forwarding_engine", "QS_2_0.EgressForwardingEngine")
comp_egress_forwarding_engine.addParams({
    "verbose" : 1,
    "num_ports" : num_ports,
})

comp_egress_ports = []
i = 0
for port in ports_config:
   comp_egress_port = sst.Component("egress_port_%d" % (i), "QS_2_0.EgressPort")
   params = {
       "port" : i,
       "port_bw": port["bw"] + "b/s",
       "verbose" : 1,
   }

   comp_egress_port.addParams(params)
   comp_egress_ports.append(comp_egress_port)

   # Egress Forwarding Engine --> Egress port [i] 
   link_egress_forwarding_engine_egress_port = sst.Link("link_egress_forwarding_engine_egress_port_%d" % (i))
   link_egress_forwarding_engine_egress_port.connect(
       (comp_egress_forwarding_engine, "egress_port_%d" % (i), "5ps"),
       (comp_egress_port, "scheduler", "5ps")
   )

   # Egress port [i] --> Finisher
   link_egress_port_finisher = sst.Link("link_egress_port_finisher_%d" % (i))
   link_egress_port_finisher.connect(
       (comp_egress_port, "output", "5ps"),
       (comp_finisher, "egress_port_0_%d" % (i), "5ps"),
   )

   # Egress port [i] --> comp_memory_system_puller 
   link_egress_forwarding_engine_egress_port = sst.Link("link_egress_port_%d_puller_loop_port_%d" % (i, i))
   link_egress_forwarding_engine_egress_port.connect(
       (comp_memory_system_puller, "loop_port_%d" % (i), "5ps"),
       (comp_egress_port, "byte_loop_port", "5ps")
   )    

   i += 1

#-----------------------------------------------------------------------------------------------------------------------------
# Connections
#-----------------------------------------------------------------------------------------------------------------------------

link_ingress_forwarding_engine_acceptance_checker = sst.Link("link_ingress_port_ingress_forwarding_engine")
link_ingress_forwarding_engine_acceptance_checker.connect(
   (comp_ingress_forwarding_engine, "acceptance_check_port", "10ps"),
   (comp_acceptance_checker, "ingress_forwarding_port", "10ps")
)

link_packet_generator_finisher = sst.Link("link_packet_generator_finisher")
link_packet_generator_finisher.connect(
    (comp_packet_generator, "finisher", "5ps"),
    (comp_finisher, "traffic_generator", "5ps")
)

# Acceptance Checker --> WRED
link_acceptance_checker_wred = sst.Link("link_acceptance_checker_wred")
link_acceptance_checker_wred.connect(
    (comp_acceptance_checker, "wred_port", "10ps"),
    (comp_wred, "acceptance_checker", "10ps")
)

# WRED --> Queue Service Tracker
link_acceptance_checker_wred = sst.Link("link_wred_queue_service_tracker")
link_acceptance_checker_wred.connect(
    (comp_wred, "queue_service_tracker_port", "10ps"),
    (comp_queue_service_tracker, "enqueue_port", "10ps")
)

# WRED --> Memory System Queue Handler
link_acceptance_checker_wred = sst.Link("link_wred_memory_system_queue_handler")
link_acceptance_checker_wred.connect(
    (comp_wred, "packet_buffer", "10ps"),
    (comp_memory_system_queue_handler, "acceptance_check_port", "10ps")
)

# WRED --> bytes_tracker
link_wred_bytes_tracker = sst.Link("link_wred_bytes_tracker")
link_wred_bytes_tracker.connect(
    (comp_wred, "bytes_tracker", "5ps"),
    (comp_bytes_tracker, "wred", "5ps")
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

# Queue Service Tracker --> Port Bank (dequeue port)
link_queue_service_tracker_port_bank_dequeue = sst.Link("link_queue_service_tracker_port_bank_dequeue")
link_queue_service_tracker_port_bank_dequeue.connect(
    (comp_queue_service_tracker, "dequeue_port", "10ps"),
    (comp_port_bank, "dequeue_port", "10ps")
)

# Queue Service Tracker --> Port Bank (adjust port)
link_queue_service_tracker_port_bank_adjust_update_port = sst.Link("link_queue_service_tracker_port_bank_adjust_update_port")
link_queue_service_tracker_port_bank_adjust_update_port.connect(
    (comp_queue_service_tracker, "adjust_update_port", "10ps"),
    (comp_port_bank, "adjust_update_port", "10ps")
)

# Queue Service Tracker --> Port Bank (queue status port)
link_queue_service_tracker_port_bank_queue_status_port = sst.Link("link_queue_service_tracker_port_bank_queue_status_port")
link_queue_service_tracker_port_bank_queue_status_port.connect(
    (comp_queue_service_tracker, "queue_status_port", "10ps"),
    (comp_port_bank, "queue_status_port", "10ps")
)

# Memory System Queue Handler --> Memory System Puller (read packet request)
link_memory_system_queue_handler_memory_system_puller_read_packet_request = sst.Link("link_memory_system_queue_handler_memory_system_puller_read_packet_request")
link_memory_system_queue_handler_memory_system_puller_read_packet_request.connect(
    (comp_memory_system_queue_handler, "packet_read_request_port", "10ps"),
    (comp_memory_system_puller, "packet_read_request_port", "10ps"),
)

# Memory System Queue Handler --> Memory System Puller (read packet reply)
link_memory_system_queue_handler_memory_system_puller_read_packet_reply = sst.Link("link_acceptance_checker_memory_system_puller_read_packet_reply")
link_memory_system_queue_handler_memory_system_puller_read_packet_reply.connect(
    (comp_memory_system_queue_handler, "packet_read_reply_port", "10ps"),
    (comp_memory_system_puller, "packet_read_reply_port", "10ps"),
)

# Port Bank --> Memory System Puller (TX Request Port)
link_port_bank_memory_system_puller_tx_request_port = sst.Link("link_queue_service_tracker_queue_memory_system_puller_tx_request_port")
link_port_bank_memory_system_puller_tx_request_port.connect(
    (comp_port_bank, "tx_request_port", "10ps"),
    (comp_memory_system_puller, "tx_request_port", "10ps")
)

# Port Bank --> Memory System Puller (TX Reply Port)
link_port_bank_memory_system_puller_tx_reply_port = sst.Link("link_port_bank_memory_system_puller_tx_reply_port")
link_port_bank_memory_system_puller_tx_reply_port.connect(
    (comp_port_bank, "tx_reply_port", "10ps"),
    (comp_memory_system_puller, "tx_reply_port", "10ps")
)

# Memory System Puller  --> Egress Forwardig Engine (TX Reply Port)
link_memory_system_puller_egress_forwarding_engine_egress_port = sst.Link("link_memory_system_puller_egress_forwarding_engine_egress_port")
link_memory_system_puller_egress_forwarding_engine_egress_port.connect(
    (comp_memory_system_puller, "egress_packet_port", "10ps"),
    (comp_egress_forwarding_engine, "ms_puller_port", "10ps")
)

# Memory System Queue Handler --> Packet Buffer
link_memory_system_queue_handler_packet_buffer = sst.Link("link_memory_system_queue_handler_packet_buffer")
link_memory_system_queue_handler_packet_buffer.connect(
    (comp_memory_system_queue_handler, "packet_buffer", "10ps"),
    (comp_pkt_buffer, "acceptance_checker", "10ps")
)

# Memory System Puller  --> Bytes Tracker
link_ms_puller_bytes_tracker = sst.Link("link_ms_puller_bytes_tracker")
link_ms_puller_bytes_tracker.connect(
    (comp_memory_system_puller, "bytes_tracker_port", "10ps"),
    (comp_bytes_tracker, "ms_puller", "10ps")
)

# Egress Forwarding Engine --> Packet Buffer
link_egress_forwarding_engine_packet_buffer = sst.Link("link_egress_forwarding_engine_packet_buffer")
link_egress_forwarding_engine_packet_buffer.connect(
    (comp_egress_forwarding_engine, "packet_buffer_port", "10ps"),
    (comp_pkt_buffer, "egress_ports", "10ps")
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

#-----------------------------------------------------------------------------------------------------------------------------
# Statistics
#-----------------------------------------------------------------------------------------------------------------------------

# Ingress ports
for ingress_port in comp_ingress_ports:
    ingress_port.enableStatistics(
        ["destination"],
        {
            "type":"sst.HistogramStatistic",
            "minvalue" : "0", 
            "binwidth" : "1", 
            "numbins"  : "52", 
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
for egress_port in comp_egress_ports:
    egress_port.enableStatistics(
        ["source"],
        {
            "type":"sst.HistogramStatistic",
            "minvalue" : "0", 
            "binwidth" : "1", 
            "numbins"  : "52", 
            "IncludeOutOfBounds" : "1", 
        }
    )

    egress_port.enableStatistics(
        ["bytes_sent"],
        {
            "type":"sst.AccumulatorStatistic",
            "rate": "1 event",
        }
    )

    egress_port.enableStatistics(
        ["sent_packets"],
        {
            "type":"sst.AccumulatorStatistic",
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

egress_ports_receiving_traffic = [20]

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
