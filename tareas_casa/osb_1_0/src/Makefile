CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)
JSONLIB=-I/nis/asic/ws08/porrasd/tools/nlohmann-json/include
TRAFFICGEN_LIB=-I../../traffic_gen_1_0/src
# JSONLIB=-I/home/porrasd/local/nlohmann-json/include
BIN=bin

ELEMENTS=$(BIN)/receiver.o $(BIN)/receiver_buffer.o	\
			$(BIN)/port_accumulator.o $(BIN)/logical_fifo.o \
			$(BIN)/nic_arbitration.o

COMPONENTS= $(BIN)/ethernet_port.o $(BIN)/port_group.o \
			$(BIN)/outer_parser.o $(BIN)/packet_classifier.o \
			$(BIN)/acceptance_checker.o $(BIN)/osb_drop_receiver.o \
			$(BIN)/multi_fifo.o $(BIN)/buffer_port_mux.o \
			$(BIN)/central_arbiter.o $(BIN)/buffer_slice_mux.o \
			$(BIN)/data_packer.o $(BIN)/lookup_pipeline.o \
			$(BIN)/packet_delay_line.o $(BIN)/final_slice.o
			

#  OSB 
all: libOSB_1_0.so install

$(BIN)/:
	mkdir $(BIN)

$(BIN)/%.o: %.cc %.h
	$(CXX) $(CXXFLAGS) $(JSONLIB) $(TRAFFICGEN_LIB) -c -o $@ $<

libOSB_1_0.so: $(BIN)/ $(ELEMENTS) $(COMPONENTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JSONLIB) $(TRAFFICGEN_LIB) -o $@ $(ELEMENTS) $(COMPONENTS)

install:
	sst-register OSB_1_0 OSB_1_0_LIBDIR=$(CURDIR)
	sst-register SST_ELEMENT_SOURCE OSB_1_0=$(CURDIR)
	sst-register SST_ELEMENT_TESTS  OSB_1_0=$(CURDIR)/../../tests/qs_1_0

clean:
	rm -f $(BIN)/*.o libOSB_1_0.so
