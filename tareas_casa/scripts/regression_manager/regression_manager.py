#!/usr/bin/env python3

###############################################################################
#
#   Copyright (c) 1998-2022 Hewlett-Packard Enterprise Development LP
#                        All Rights Reserved.
#   The contents of this file are proprietary and confidential to the
#   Hewlett Packard Enterprise Development LP.  No part of this file
#   may be photocopied, reproduced or translated to another program
#   language without the prior written consent of Hewlett Packard
#   Enterprise Development LP.
#
#                       RESTRICTED RIGHTS LEGEND
#
#   Use, duplication or disclosure by the Government is subject to
#   restrictions as set forth in subdivision (b) (3) (ii) of the Rights
#   in Technical Data and Computer Software clause at 52.227-7013.
#
#             Hewlett Packard Enterprise Development LP
#                       3000 Hanover Street
#                       Palo Alto, CA 94303
#
###############################################################################

"""
Description:
        Regression Manager for QS_1_0
Editor(s):
        Freddy Zuniga (freddy.zuniga@hpe.com)
"""


import json
import threading
import time
import logging
import os

home_dir = os.path.expanduser("~")
json_file = home_dir + "/hpn_rosewood/model/sst/scripts/regression_manager/tests.json"

class Test:
    """ Class representing an individual test"""
    def __init__(self, test_name, times, rosewood_repo, args_file, directory):
        self.test_name = test_name
        self.times = times
        self.rosewood_repo = rosewood_repo
        self.args_file = args_file
        self.directory = directory

    # Method to run the test
    def run_test(self):
        """ Method to run the test """
        logging.info("Running test %s %d times", self.test_name, self.times)
        for i in range(self.times):
            #dir_test = "test/qs_1_0" + self.path_test
            logging.info("Running test %s, iteration %d", self.test_name, i+1)
            tests_path = home_dir + "/" + self.rosewood_repo + "/"
            if self.directory != "":
                repo_path = tests_path + self.directory + "/" + self.test_name
                options = "-a " + tests_path + self.directory + "/" + self.args_file + " -v INFO"
            else:
                repo_path = tests_path + self.test_name
                options = "-a " + tests_path + self.args_file + " -v INFO"

            command = "sst " + repo_path + " --model-options='" + options + "' &> temp.log"
            print(command)
            os.system(command)

# Class managing the execution of the tests
class RegressionManager:
    """ Class managing the execution of the tests """
    def __init__(self, rosewood_repo, test_list, max_threads):
        self.rosewood_repo = rosewood_repo
        self.test_list = test_list
        self.max_threads = max_threads

    def run(self):
        """ Method to run the tests """
        logging.info("Running regression tests from %s", self.rosewood_repo)
        test_threads = []
        for test in self.test_list:
            while len(test_threads) >= self.max_threads:
                # Wait until some thread finishes and remove it from the list
                for test_thread in test_threads:
                    if not test_thread.is_alive():
                        test_threads.remove(test_thread)
                        break
                time.sleep(1)
            test.rosewood_repo = self.rosewood_repo
            test_thread = threading.Thread(target=test.run_test)
            test_threads.append(test_thread)
            test_thread.start()
        # Wait for all threads to finish
        for test_thread in test_threads:
            test_thread.join()

def main():
    """ Main function """
    date='%d-%b-%y %H:%M:%S'
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s', datefmt=date)
    with open(json_file, "r", encoding="utf-8") as file:
        tests_json = json.load(file)
    rosewood_repo = tests_json["rosewood_repo"]
    test_list_json = tests_json["test_list"]
    max_threads = tests_json["max_threads"]
    test_list = []
    for test_json in test_list_json:
        test = Test(test_json["test_name"], test_json["times"], 
                    rosewood_repo, test_json["args_file"], test_json["directory"])
        test_list.append(test)
    regression_manager = RegressionManager(rosewood_repo, test_list, max_threads)
    regression_manager.run()

if __name__ == "__main__":
    main()
