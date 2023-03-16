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
        This script is a linter that utilizes the clang-format tool to verify 
        and correct .cc and .h files within a specified directory. The script 
        accepts the directory path as input, and includes an option to correct 
        formatting errors if necessary. The script checks the files for errors, 
        and returns a message indicating whether the format is correct or not. 
        This tool is useful for developers looking to maintain consistent 
        formatting in their C++ code.
Editor(s):
        Freddy Zuniga (freddy.zuniga@hpe.com)
"""

import argparse
import os
import subprocess
import time


DEFAULT_EXTENSIONS = ('cc', 'h')
DEFAULT_CLANG_VERSION = 'clang-format-12'

def clang_format_verif(filename):
    """The subprocess module allows us to spawn new processes, in this case, 
    we use it to run the 'clang-format' command. The output of the command is 
    captured and stored in the 'revision' variable.

    :param filename: Is the name of the file that will be verified.

    :return: error code (-1) if there was an error, or a success code (0) if 
    there was no error, along with an error message if one exists.
    """
    revision = subprocess.run([DEFAULT_CLANG_VERSION,
                               '--dry-run',
                               filename],
                              capture_output=True,
                              check=False,
                              text=True)

    if revision.stderr != "":
        return [-1, revision.stderr]
    else:
        return [0, revision.stderr]

def clang_format_correction(filename):
    """Applies the clang-format tool to the specified file, overwriting the 
    file in place.

    :params filename (str): The name of the file to be formatted.
    """
    subprocess.run([DEFAULT_CLANG_VERSION,
                    '-i',
                    filename],
                    check=False)

def iterate_over_files(directory, correction_option):
    """Iterates over all files in a given directory and checks their format using 
    the clang-format tool. If the file format is incorrect and correction_option is 
    True, the file is automatically corrected. If correction_option is False, an 
    error message is returned.

    :param directory: The path to the directory containing the files to be checked.
    :param correction_option: A boolean indicating whether to automatically correct 
    format errors.

    :return: A list containing the status of the format check ('Correct Format' or 
    'Incorrect Format') and an error message (if one exists).
    """
    errors = ""
    is_valid_extension = False
    for filename in os.listdir(directory):
        if any(filename.endswith(ext) for ext in DEFAULT_EXTENSIONS):
            is_valid_extension = True
            break

    if is_valid_extension:
        for filename in os.scandir(directory): 
            if filename.path.endswith(DEFAULT_EXTENSIONS):
                val = clang_format_verif(filename.path)
                if val[0] == -1 and correction_option is False:
                    errors += val[1] + "\n"

                if val[0] == -1 and correction_option is True:
                    clang_format_correction(filename.path)
        if correction_option or errors == "":
            return ['Correct Format', '']
        else:
            ## errors: if we need in the future print errors in
            ## the terminal or save errors in a log, if this is
            ## not necessarily then we can delete this variable
            return ['Incorrect Format', errors]
    else:
        return ['There are no files to check with clang-format in the provided directory.', '']

def create_error_log(errors):
    """
    Create a log file for errors and save it in the 'clang_logs' directory.
    
    :param errors: The error messages to be written to the log file.
    :type errors: str
    """
    # Get the absolute path of the script
    script_path = os.path.abspath(__file__)
    # Get the directory containing the script
    script_dir = os.path.dirname(script_path)

    # Define the directory path for the logs
    dir_path_logs = script_dir + "/clang_logs"

    # Check if the folder already exists
    if not os.path.exists(dir_path_logs):
        # If it doesn't exist, create the folder
        os.makedirs(dir_path_logs)

    # Create a timestamp for the log file name
    timestamp = time.strftime("%Y-%m-%d-%H:%M:%S", time.localtime())
    # Create the log file name with the timestamp
    log_name = dir_path_logs + "/" + timestamp + "-clang_errors.log"

    # Open the log file in write mode and write the errors
    with open(log_name, "w", encoding="utf-8") as file:
        file.write(errors)

    # Create a report message with the log file path
    report = "see error log: " + log_name
    # Print the report message
    print(report)

def main():
    """Main function that handles command-line arguments and calls the 
    `iterate_over_files` function.

    :return: prints the result of the `iterate_over_files` function to the console.
    """
    parser = argparse.ArgumentParser(description='clang-format verification')
    parser.add_argument(
        '-c',
        '--correction_option',
        required=False,
        help='when activate this option, it correct the format of files only if '
             'the format of these is different to the official SST format',
        action='store_true')

    parser.add_argument(
        '-d',
        '--directory',
        type=str,
        required=True,
        help='receives the path of the directory containing the files to be evaluated')

    args = parser.parse_args()
    correction_option = args.correction_option
    directory = args.directory
    result = iterate_over_files(directory, correction_option)
    print(result[0])
    if result[0] == "Incorrect Format":
        create_error_log(result[1])

if __name__ == '__main__':
    main()
