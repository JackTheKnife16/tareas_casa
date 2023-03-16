# Import SST library in Python
import sst
import argparse
import json
import os
import logging

# Set the default timebase
sst.setProgramOption("timebase", "1ps")

# Enable statistic until level 7
sst.setStatisticLoadLevel(7)
# Set the stats output as CSV, filepath, and separator for CSV
sst.setStatisticOutput("sst.statOutputCSV",
                        {
                            "filepath" : "osb_1_0_bringup.csv",
                            "separator" : ","
                        })

frontplane_config_file = "osb_1_0/config/frontplane_config/4x800G.json"
if os.path.isfile(frontplane_config_file):
    try:
        with open(frontplane_config_file) as jsonfile:
            frontplane_config = json.load(jsonfile)
    except Exception as e:
        logging.error("Can't parse %s", frontplane_config_file)
        exit(1)
else:
    logging.error("File %s does not exist", ethernet_config)
    exit(1)

# Read the ethernet config files
ports_config = {}
for pg_config in frontplane_config:
    ethernet_config = pg_config["ethernet_config"]
    if os.path.isfile(ethernet_config):
        try:
            with open(ethernet_config) as jsonfile:
                ports_config[pg_config["port_group"]] = json.load(jsonfile)
        except Exception as e:
            logging.error("Can't parse %s", ethernet_config)
            exit(1)
    else:
        logging.error("File %s does not exist", ethernet_config)
        exit(1)

# Model attributes ---------------------------------------------------------------------------------
num_pgs = 4
num_ports_per_pg = len(ports_config[0])
num_ports = num_ports_per_pg * num_pgs
num_priorities = 4
initial_word_width = 128
final_word_width = 256
ipg_size = 20
clock_frequency = "1500 MHz"
initial_chunk_size = 2
non_initial_chunk_size = 1
fds_size = 80
num_buffer_slices = 2
pgs_per_buffer_slice = 2
ports_per_buffer_slice = num_ports_per_pg * pgs_per_buffer_slice
packet_window_size = 512 // initial_word_width # Words
fifo_pop_size = 256 // initial_word_width # Words
# ---------------------------------------------------------------------------------------------------

# Traffic Generator ---------------------------------------------------------------------------------

# Traffic Gen Component -----------------------------------------------------------------------------
comp_traffic_generator = sst.Component("traffic_generator", "TRAFFIC_GEN_1_0.TrafficGenerator")
comp_traffic_generator.addParams({
    "num_ports" : num_ports,
    "num_nodes" : 1,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------

# Traffic Profile -----------------------------------------------------------------------------------
subcomp_traffic_profile = comp_traffic_generator.setSubComponent("traffic_profile", "TRAFFIC_GEN_1_0.OSBOneToOneFixed", 0)
subcomp_traffic_profile.addParams({
    # FIXME: create the profile properly
    "ethernet_config": frontplane_config[0]["ethernet_config"],
    "num_packets": 1,
    "packet_size": 512,
    "num_ports" : num_ports,
    "seed" : 1447,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------

# Finisher ------------------------------------------------------------------------------------------
comp_finisher = sst.Component("finisher", "TRAFFIC_GEN_1_0.Finisher")
comp_finisher.addParams({
    "num_ports" : num_buffer_slices,
    "num_nodes" : 1,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------

# ---------------------------------------------------------------------------------------------------

# Drop Receiver -------------------------------------------------------------------------------------
comp_drop_receiver = sst.Component("drop_receiver", "OSB_1_0.DropReceiver")
comp_drop_receiver.addParams({
    "num_port_ingress_slices" : num_pgs,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------

# Port Ingress Slices --------------------------------------------------------------------------------
port_groups = []
outer_parsers = []
packet_classifiers = []
acceptance_checkers = []
ethernet_ports = {}

for pg_index in range(num_pgs):
    # Port Group ----------------------------------------------------------------------------------------
    comp_port_group = sst.Component("pg_%d" % (pg_index), "OSB_1_0.PortGroup")
    comp_port_group.addParams({
        "frequency" : clock_frequency,
        "pg_index" : pg_index,
        "num_ports" : num_ports_per_pg,
        "ethernet_config": frontplane_config[pg_index]["ethernet_config"],
        "bus_width": initial_word_width,
        "verbose" : 0,
    })
    port_groups.append(comp_port_group)
    # ---------------------------------------------------------------------------------------------------

    # Ethernet Ports ------------------------------------------------------------------------------------
    ethernet_ports[pg_index] = []
    for port in ports_config[pg_index]:
        if (port["enable"]):
            # Compute port number
            pid = port["port"]
            port_number = pid + pg_index * num_ports_per_pg

            comp_ethernet_port = sst.Component("ethernet_port_%d" % (port_number), "OSB_1_0.EthernetPort")
            comp_ethernet_port.addParams({
                "port": port_number,
                "pg_index": pg_index,
                "port_speed": port["bw"] + "b/s",
                "bus_width": initial_word_width,
                "initial_chunk": initial_chunk_size,
                "non_initial_chunk": non_initial_chunk_size,
                "ipg_size": ipg_size,
                "verbose" : 0,
            })

            link_eth_port_traffic_gen = sst.Link("link_eth_port_%d_traffic_gen" % (port_number))
            link_eth_port_traffic_gen.connect(
                (comp_traffic_generator, "ingress_port_0_%d" % (port_number), "5ps"),
                (comp_ethernet_port, "traffic_gen", "5ps"),
            )
            link_eth_port_port_group = sst.Link("link_eth_port_%d_port_group_%d" % (port_number, pg_index))
            link_eth_port_port_group.connect(
                (comp_ethernet_port, "output", "10ps"),
                (comp_port_group, "port_%d" % (pid), "10ps"),
            )

            ethernet_ports[pg_index].append(comp_ethernet_port)
    # ---------------------------------------------------------------------------------------------------

    # Outer Parser --------------------------------------------------------------------------------------
    comp_outer_parser = sst.Component("outer_parser_%d" % (port_number), "OSB_1_0.OuterParser")
    comp_outer_parser.addParams({
        "frequency" : clock_frequency,
        "pg_index" : pg_index,
        "verbose" : 0,
    })
    outer_parsers.append(comp_outer_parser)
    # PG --> Outer Parser
    link_port_group_outer_parser_pkt = sst.Link("link_port_group_outer_parser_%d_pkt" % (pg_index))
    link_port_group_outer_parser_pkt.connect(
        (comp_port_group, "packet_bus", "10ps"),
        (comp_outer_parser, "in_packet_bus", "10ps"),
    )

    link_port_group_outer_parser_fds = sst.Link("link_port_group_outer_parser_%d_fds" % (pg_index))
    link_port_group_outer_parser_fds.connect(
        (comp_port_group, "fds_bus", "10ps"),
        (comp_outer_parser, "in_fds_bus", "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------

    # Packet Classifier ---------------------------------------------------------------------------------
    comp_packet_classifier = sst.Component("packet_classifier_%d" % (port_number), "OSB_1_0.PacketClassifier")
    comp_packet_classifier.addParams({
        "frequency" : clock_frequency,
        "pg_index" : pg_index,
        "verbose" : 0,
    })
    packet_classifiers.append(comp_packet_classifier)
    # Outer Parser --> Packet Classifier
    link_outer_parser_packet_classifier_pkt = sst.Link("link_outer_parser_packet_classifier_%d_pkt" % (pg_index))
    link_outer_parser_packet_classifier_pkt.connect(
        (comp_outer_parser, "out_packet_bus", "10ps"),
        (comp_packet_classifier, "in_packet_bus", "10ps"),
    )

    link_outer_parser_packet_classifier_fds = sst.Link("link_outer_parser_packet_classifier_%d_fds" % (pg_index))
    link_outer_parser_packet_classifier_fds.connect(
        (comp_outer_parser, "out_fds_bus", "10ps"),
        (comp_packet_classifier, "in_fds_bus", "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------

    # Acceptance Checker --------------------------------------------------------------------------------
    comp_acceptance_checker = sst.Component("acceptance_checker_%d" % (port_number), "OSB_1_0.AcceptanceChecker")
    comp_acceptance_checker.addParams({
        "frequency" : clock_frequency,
        "pg_index" : pg_index,
        "bus_width": final_word_width,
        "mtu_config_file": "osb_1_0/config/mtu_config/mtu_basic.json",
        "packet_data_limit": 1,
        "fds_limit": 1,
        "verbose" : 0,
    })
    acceptance_checkers.append(comp_acceptance_checker)
    # Packet Classifier --> Acceptance Checker
    link_packet_classifier_acceptance_checker_pkt = sst.Link("link_packet_classifier_acceptance_checker_%d_pkt" % (pg_index))
    link_packet_classifier_acceptance_checker_pkt.connect(
        (comp_packet_classifier, "out_packet_bus", "10ps"),
        (comp_acceptance_checker, "in_packet_bus", "10ps"),
    )

    link_packet_classifier_acceptance_checker_fds = sst.Link("link_packet_classifier_acceptance_checker_%d_fds" % (pg_index))
    link_packet_classifier_acceptance_checker_fds.connect(
        (comp_packet_classifier, "out_fds_bus", "10ps"),
        (comp_acceptance_checker, "in_fds_bus", "10ps"),
    )
    # Acceptance Checker --> Drop Receiver
    link_acceptance_checker_drop_receiver = sst.Link("link_acceptance_checker_drop_receiver_%d" % (pg_index))
    link_acceptance_checker_drop_receiver.connect(
        (comp_acceptance_checker, "drop_receiver", "10ps"),
        (comp_drop_receiver, "acceptance_checker_%d" % (pg_index), "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------------

# Central Arbitration Logic -------------------------------------------------------------------------
# Central Arbiter -----------------------------------------------------------------------------------
comp_central_arbiter = sst.Component("central_arbiter", "OSB_1_0.CentralArbiter")
comp_central_arbiter.addParams({
    "frequency": clock_frequency,
    "buffer_slices": num_buffer_slices,
    "ports_per_buffer": ports_per_buffer_slice,
    "priorities": num_priorities,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------

# Buffer Slice Mux -----------------------------------------------------------------------------------
comp_buffer_slice_mux = sst.Component("buffer_slice_mux", "OSB_1_0.BufferSliceMux")
comp_buffer_slice_mux.addParams({
    "buffer_slices": num_buffer_slices,
    "verbose" : 0,
})
# ---------------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------------

# Buffer Slices -------------------------------------------------------------------------------------
packet_data_multififos = []
fds_multififos = []
buffer_port_muxes = []
for buffer_slice_idx in range(num_buffer_slices):
    # Packet Data Multi-FIFO ----------------------------------------------------------------------------
    comp_packet_data_multi_fifo = sst.Component("packet_data_multi_fifo_%d" % (buffer_slice_idx), "OSB_1_0.MultiFIFO")
    comp_packet_data_multi_fifo.addParams({
        "ports": ports_per_buffer_slice,
        "priorities" : num_priorities,
        "port_ingress_slices": pgs_per_buffer_slice,
        "buffer_slice_index": buffer_slice_idx,
        "packet_window_size" : packet_window_size,
        "pop_size": fifo_pop_size,
        "width" : initial_word_width,
        "depth_config" : "osb_1_0/config/fifo_config/packet_fifo_config.json",
        "frontplane_config": frontplane_config_file,
        "req_enable": 1,
        "overflow": 0,
        "verbose" : 0,
    })
    packet_data_multififos.append(comp_packet_data_multi_fifo)
    # Acceptance Checker --> Packet Multi-FIFO
    for i in range(pgs_per_buffer_slice):
        pg_index = buffer_slice_idx * pgs_per_buffer_slice + i
        link_acceptance_checker_packet_multififo_input = sst.Link("link_acceptance_checker_%d_packet_multififo_input_%d" % (pg_index, buffer_slice_idx))
        link_acceptance_checker_packet_multififo_input.connect(
            (acceptance_checkers[pg_index], "out_packet_bus", "10ps"),
            (comp_packet_data_multi_fifo, "input_%d" % (i), "10ps"),
        )

        link_acceptance_checker_packet_multififo_update = sst.Link("link_acceptance_checker_%d_packet_multififo_update_%d" % (pg_index, buffer_slice_idx))
        link_acceptance_checker_packet_multififo_update.connect(
            (acceptance_checkers[pg_index], "packet_fifos", "10ps"),
            (comp_packet_data_multi_fifo, "fifo_update_%d" % (i), "10ps"),
        )
    # Packet Multi-FIFO --> Central Arbiter
    link_multi_fifo_central_arbiter_ic_req = sst.Link("link_multi_fifo_%d_central_arbiter_ic_req" % (buffer_slice_idx))
    link_multi_fifo_central_arbiter_ic_req.connect(
        (comp_packet_data_multi_fifo, "ic_req", "10ps"),
        (comp_central_arbiter, "ic_req_%d" % (buffer_slice_idx), "10ps"),
    )
    link_multi_fifo_central_arbiter_nic_req = sst.Link("link_multi_fifo_%d_central_arbiter_nic_req" % (buffer_slice_idx))
    link_multi_fifo_central_arbiter_nic_req.connect(
        (comp_packet_data_multi_fifo, "nic_req", "10ps"),
        (comp_central_arbiter, "nic_req_%d" % (buffer_slice_idx), "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------

    # FDS Multi-FIFO --------------------------------------------------------------------------------
    comp_fds_multi_fifo = sst.Component("fds_multi_fifo_%d" % (buffer_slice_idx), "OSB_1_0.MultiFIFO")
    comp_fds_multi_fifo.addParams({
        "ports": ports_per_buffer_slice,
        "priorities" : num_priorities,
        "port_ingress_slices": pgs_per_buffer_slice,
        "buffer_slice_index": buffer_slice_idx,
        "width" : fds_size,
        "depth_config" : "osb_1_0/config/fifo_config/fds_config.json",
        "frontplane_config": frontplane_config_file,
        "req_enable": 0,
        "overflow": 0,
        "verbose" : 0,
    })
    fds_multififos.append(comp_fds_multi_fifo)
    # Acceptance Checker --> FDS Multi-FIFO
    for i in range(pgs_per_buffer_slice):
        pg_index = buffer_slice_idx * pgs_per_buffer_slice + i
        link_acceptance_checker_fds_multififo_input = sst.Link("link_acceptance_checker_%d_fds_multififo_input_%d" % (pg_index, buffer_slice_idx))
        link_acceptance_checker_fds_multififo_input.connect(
            (acceptance_checkers[pg_index], "out_fds_bus", "10ps"),
            (comp_fds_multi_fifo, "input_%d" % (i), "10ps"),
        )

        link_acceptance_checker_fds_multififo_update = sst.Link("link_acceptance_checker_%d_fds_multififo_update_%d" % (pg_index, buffer_slice_idx))
        link_acceptance_checker_fds_multififo_update.connect(
            (acceptance_checkers[pg_index], "fds_fifos", "10ps"),
            (comp_fds_multi_fifo, "fifo_update_%d" % (i), "10ps"),
        )
    # ---------------------------------------------------------------------------------------------------

    # Buffer Port Mux -----------------------------------------------------------------------------------
    comp_buffer_port_mux = sst.Component("buffer_port_mux_%d" % (buffer_slice_idx), "OSB_1_0.BufferPortMux")
    comp_buffer_port_mux.addParams({
        "buffer_slice_index": buffer_slice_idx,
        "fifo_width" : initial_word_width,
        "pop_size" : 2,
        "verbose" : 0,
    })
    buffer_port_muxes.append(comp_buffer_port_mux)
    # Multi-FIFOs <--> Buffer Port Mux
    link_p_multi_fifo_buffer_port_mux_output = sst.Link("link_p_multi_fifo_buffer_port_mux_%d_output" % (buffer_slice_idx))
    link_p_multi_fifo_buffer_port_mux_output.connect(
        (comp_packet_data_multi_fifo, "output", "10ps"),
        (comp_buffer_port_mux, "pkt_in", "10ps"),
    )
    link_p_multi_fifo_buffer_port_mux_req = sst.Link("link_p_multi_fifo_buffer_port_mux_%d_req" % (buffer_slice_idx))
    link_p_multi_fifo_buffer_port_mux_req.connect(
        (comp_packet_data_multi_fifo, "fifo_req", "10ps"),
        (comp_buffer_port_mux, "pkt_req", "10ps"),
    )
    link_f_multi_fifo_buffer_port_mux_output = sst.Link("link_f_multi_fifo_buffer_port_mux_%d_output" % (buffer_slice_idx))
    link_f_multi_fifo_buffer_port_mux_output.connect(
        (comp_fds_multi_fifo, "output", "10ps"),
        (comp_buffer_port_mux, "fds_in", "10ps"),
    )
    link_f_multi_fifo_buffer_port_mux_req = sst.Link("link_f_multi_fifo_buffer_port_mux_%d_req" % (buffer_slice_idx))
    link_f_multi_fifo_buffer_port_mux_req.connect(
        (comp_fds_multi_fifo, "fifo_req", "10ps"),
        (comp_buffer_port_mux, "fds_req", "10ps"),
    )
    # Buffer Port Mux <-- Central Arbiter
    link_buffer_port_mux_central_arbiter_ic_grant = sst.Link("link_buffer_port_mux_%d_central_arbiter_ic_grant" % (buffer_slice_idx))
    link_buffer_port_mux_central_arbiter_ic_grant.connect(
        (comp_buffer_port_mux, "ic_gnt", "10ps"),
        (comp_central_arbiter, "ic_gnt_%d" % (buffer_slice_idx), "10ps"),
    )
    link_buffer_port_mux_central_arbiter_nic_grant = sst.Link("link_buffer_port_mux_%d_central_arbiter_nic_grant" % (buffer_slice_idx))
    link_buffer_port_mux_central_arbiter_nic_grant.connect(
        (comp_buffer_port_mux, "nic_gnt", "10ps"),
        (comp_central_arbiter, "nic_gnt_%d" % (buffer_slice_idx), "10ps"),
    )
    # Buffer Port Mux --> Buffer Slice Mux
    link_buffer_port_mux_buffer_slice_mux = sst.Link("link_buffer_port_mux_%d_buffer_slice_mux" % (buffer_slice_idx))
    link_buffer_port_mux_buffer_slice_mux.connect(
        (comp_buffer_port_mux, "fds_out", "10ps"),
        (comp_buffer_slice_mux, "fds_in_%d" % (buffer_slice_idx), "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------

# ---------------------------------------------------------------------------------------------------

# FE Lookup Pipeline -------------------------------------------------------------------------
data_packers = []
for buffer_slice_idx in range(num_buffer_slices):
    # Data Packer -----------------------------------------------------------------------------------
    comp_data_packer = sst.Component("data_packer_%d" % (buffer_slice_idx), "OSB_1_0.DataPacker")
    comp_data_packer.addParams({
        "frequency" : clock_frequency,
        "buffer_slice_index" : buffer_slice_idx,
        "port_ingress_slices": pgs_per_buffer_slice,
        "input_bus_width": final_word_width,
        "output_bus_width": final_word_width,
        "fds_size": fds_size,
        "initial_chunk_size": initial_chunk_size,
        "non_initial_chunk": non_initial_chunk_size,
        "frontplane_config": frontplane_config_file,
        "verbose" : 1,
    })
    data_packers.append(comp_data_packer)
    # Buffer Port Mux --> data Packer
    link_buffer_port_mux_data_packer = sst.Link("link_link_buffer_port_mux_data_packer_%d" % (buffer_slice_idx))
    link_buffer_port_mux_data_packer.connect(
        (buffer_port_muxes[buffer_slice_idx], "pkt_out", "10ps"),
        (comp_data_packer, "in_packet_bus", "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------------

# Lookup Pipeline
comp_lookup_pipeline = sst.Component("lookup_pipeline", "OSB_1_0.LookupPipeline")
comp_lookup_pipeline.addParams({
    "frequency" : clock_frequency,
    "num_cycles" : 3,
    "verbose" : 0,
})

link_lookup_pipeline_buffer_slice_mux = sst.Link("link_lookup_pipeline_buffer_slice_mux")
link_lookup_pipeline_buffer_slice_mux.connect(
    (comp_lookup_pipeline, "in_fds_bus", "10ps"),
    (comp_buffer_slice_mux, "fds_out", "10ps"),
)

# Receiver
for buffer_slice_idx in range(num_buffer_slices):
    # Receiver -------------------------------------------------------------------------------------------
    comp_receiver = sst.Component("receiver_%d" % (buffer_slice_idx), "OSB_1_0.Receiver")
    comp_receiver.addParams({
        "verbose" : 0,
    })
    # Data Packer --> Receiver
    link_data_packer_receiver = sst.Link("link_data_packer_receiver_%d" % (buffer_slice_idx))
    link_data_packer_receiver.connect(
        (data_packers[buffer_slice_idx], "out_packet_bus", "10ps"),
        (comp_receiver, "input", "10ps"),
    )
    # Receiver --> Finisher
    link_receiver_finisher = sst.Link("link_receiver_%d_finisher" % (buffer_slice_idx))
    link_receiver_finisher.connect(
        (comp_receiver, "output", "10ps"),
        (comp_finisher, "egress_port_0_%d" % (buffer_slice_idx), "10ps"),
    )
    # ---------------------------------------------------------------------------------------------------

# Link connections ----------------------------------------------------------------------------------

link_traffic_generator_finisher = sst.Link("link_traffic_generator_finisher")
link_traffic_generator_finisher.connect(
    (comp_traffic_generator, "finisher", "10ps"),
    (comp_finisher, "traffic_generator", "10ps")
)

link_drop_receiver_finisher = sst.Link("link_drop_receiver_finisher")
link_drop_receiver_finisher.connect(
    (comp_drop_receiver, "finisher", "10ps"),
    (comp_finisher, "drop_receiver", "10ps"),
)


# ---------------------------------------------------------------------------------------------------

# # Statistics ----------------------------------------------------------------------------------------
# for port in ethernet_ports:
#     port.enableStatistics(
#         ["bytes_sent"],
#         {
#             "type":"sst.AccumulatorStatistic",
#             "rate": "1 event",
#         }
#     )

#     port.enableStatistics(
#         ["sent_packets"],
#         {
#             "type":"sst.AccumulatorStatistic",
#         }
#     )

# comp_port_group.enableStatistics(
#     ["bytes_sent"],
#     {
#         "type":"sst.AccumulatorStatistic",
#         "rate": "1 event",
#     }
# )

# comp_port_group.enableStatistics(
#     ["sent_packets"],
#     {
#         "type":"sst.AccumulatorStatistic",
#     }
# )

# comp_data_packer.enableStatistics(
#     ["bytes_sent"],
#     {
#         "type":"sst.AccumulatorStatistic",
#         "rate": "1 event",
#     }
# )

# comp_data_packer.enableStatistics(
#     ["sent_packets"],
#     {
#         "type":"sst.AccumulatorStatistic",
#     }
# )
# # ---------------------------------------------------------------------------------------------------