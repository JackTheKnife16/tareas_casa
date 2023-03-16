#!/usr/bin/python3

"""
Queueing System Model Test Launcher
"""

# Imports
import sys
import os
import argparse
import logging
import json
import subprocess
from datetime import datetime

####################
# INITIAL SETTINGS #
####################
# Turn off bytecode generation (pyc files)
sys.dont_write_bytecode = True

# ==============================================================================
# Args set up

#######################
# Utility functions   #
#######################
def set_logging_conf(log_mode):
   logging.basicConfig(level=log_mode, format='%(asctime)s: %(levelname)s: %(message)s')

#######################
# COMMAND LINE PARSER #
#######################
# Parser Object
parser = argparse.ArgumentParser(description="Queueing System Model Test Launcher")

# Optional Arguments
parser.add_argument('-d', '--debug', required=False,
                    help="Debug Mode", action='store_true')

# Required Arguments
required_arguments = parser.add_argument_group('required arguments')

required_arguments.add_argument('-t', '--test', required=True,
                                help="Path to the test.")
required_arguments.add_argument('-a', '--args', required=True,
                                help="Path to args file.")

args = parser.parse_args()


################
# LOGGING MODE #
################
if args.debug:
    set_logging_conf(logging.DEBUG)
else:
    set_logging_conf(logging.INFO)

# ==============================================================================

logging.info('''
----------------------------------------------------------------------------------------
 _____                       _               _____           _                 
|  _  |                     (_)             /  ___|         | |                
| | | |_   _  ___ _   _  ___ _ _ __   __ _  \ `--. _   _ ___| |_ ___ _ __ ___  
| | | | | | |/ _ \ | | |/ _ \ | '_ \ / _` |  `--. \ | | / __| __/ _ \ '_ ` _ \ 
\ \/' / |_| |  __/ |_| |  __/ | | | | (_| | /\__/ / |_| \__ \ ||  __/ | | | | |
 \_/\_\\__,_|\___|\__,_|\___|_|_| |_|\__, | \____/ \__, |___/\__\___|_| |_| |_|
                                      __/ |         __/ |                      
                                     |___/         |___/                                                                                                    
-------------------------------------------------------------------------------------------
''')

if __name__ == '__main__':
    logging.debug("Test: %s", args.test)

    cmd = [
        "sst",
        args.test,
        "--model-options=\"-a%s\"" % (args.args)
    ]
    logging.debug("Executing: %s", cmd)

    before  = datetime.now()

    out = subprocess.Popen(cmd, stderr=subprocess.PIPE)
    stdout, stderr = out.communicate()

    after  = datetime.now()

    duration = after - before
    duration_in_m = duration.total_seconds() / 60
    
    # Store simulation log
    # with open("%s.log" % (args.test.split(".")[0]), "w") as log_file:
    #     # logging.debug("STDOUT: %s", stdout.decode("utf-8"))
    #     log_file.write(stdout.decode("utf-8"))

    logging.debug("STDERR:\n\n---------\n%s\n---------\n\n", stderr.decode("utf-8"))


    logging.info("Duration: %f min", duration_in_m)


    logging.info("Test Complete!")