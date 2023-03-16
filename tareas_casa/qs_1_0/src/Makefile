CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)
JSONLIB=-I/nis/asic/ws08/porrasd/tools/nlohmann-json/include
TRAFFICGEN_LIB=-I../../traffic_gen_1_0/src
# JSONLIB=-I/home/porrasd/local/nlohmann-json/include
BIN=bin

COMPONENTS=$(BIN)/packet_buffer.o $(BIN)/egress_update.o \
			$(BIN)/ingress_port.o $(BIN)/acceptance_checker.o \
			$(BIN)/host_ingress_port.o $(BIN)/host_egress_port.o \
			$(BIN)/wred.o $(BIN)/drop_receiver.o \
			$(BIN)/priority_queue.o $(BIN)/port_fifo.o \
			$(BIN)/scheduler.o $(BIN)/egress_port.o \
			$(BIN)/scheduler_dwrr.o \
			$(BIN)/bytes_tracker.o $(BIN)/element_connector.o \
			$(BIN)/bytes_updater.o $(BIN)/global_pool_util.o \
			$(BIN)/accounting_element.o $(BIN)/lookup_table.o \
			$(BIN)/utilization_reporter.o $(BIN)/event_forwarder.o 

DUALQ_COMP=$(BIN)/dualq_acceptance_checker.o $(BIN)/dualq_ecn_classifier.o \
			$(BIN)/dualq_wred.o $(BIN)/dualq_l4s_wred.o \
			$(BIN)/dualq_strict_priority_scheduler.o $(BIN)/dualq_lookup_controller.o \
			$(BIN)/dualq_global_pool_util.o $(BIN)/dualq_bytes_tracker.o \
			$(BIN)/dualq_packet_classifier.o $(BIN)/dualq_priority_queue.o \
			$(BIN)/dualq_dwrr_scheduler.o


#  _____                       _               _____           _                 
# |  _  |                     (_)             /  ___|         | |                
# | | | |_   _  ___ _   _  ___ _ _ __   __ _  \ `--. _   _ ___| |_ ___ _ __ ___  
# | | | | | | |/ _ \ | | |/ _ \ | '_ \ / _` |  `--. \ | | / __| __/ _ \ '_ ` _ \ 
# \ \/' / |_| |  __/ |_| |  __/ | | | | (_| | /\__/ / |_| \__ \ ||  __/ | | | | |
#  \_/\_\\__,_|\___|\__,_|\___|_|_| |_|\__, | \____/ \__, |___/\__\___|_| |_| |_|
#                                       __/ |         __/ |                      
#                                      |___/         |___/                       
all: libQS_1_0.so install

$(BIN)/:
	mkdir $(BIN)

$(BIN)/%.o: %.cc %.h
	$(CXX) $(CXXFLAGS) $(JSONLIB) $(TRAFFICGEN_LIB) -c -o $@ $<

libQS_1_0.so: $(BIN)/ $(COMPONENTS) $(DUALQ_COMP)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JSONLIB) $(TRAFFICGEN_LIB) -o $@ $(COMPONENTS) $(DUALQ_COMP)

install:
	sst-register QS_1_0 QS_1_0_LIBDIR=$(CURDIR)
	sst-register SST_ELEMENT_SOURCE QS_1_0=$(CURDIR)
	sst-register SST_ELEMENT_TESTS  QS_1_0=$(CURDIR)/../../tests/qs_1_0

clean:
	rm -f $(BIN)/*.o libQS_1_0.so
