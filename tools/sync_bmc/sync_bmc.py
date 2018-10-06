#!/usr/bin/env python

#############################################################################
# Fishbone                                                                  #
#                                                                           #
# Platform and model specific service for send data to BMC                  #
#                                                                           #
# Known issue : BMC only accept sensor data in integer format               #
#                                                                           #
#############################################################################

import subprocess
import requests
import os
import imp


PLATFORM_ROOT_PATH = '/usr/share/sonic/device'
SONIC_CFGGEN_PATH = '/usr/local/bin/sonic-cfggen'
HWSKU_KEY = 'DEVICE_METADATA.localhost.hwsku'
PLATFORM_KEY = 'DEVICE_METADATA.localhost.platform'
TEMP_URL = 'http://[fe80::1:1%eth0.4088]:8080/api/sys/temp'

PLATFORM_SPECIFIC_SFP_MODULE_NAME = "sfputil"
PLATFORM_SPECIFIC_SFP_CLASS_NAME = "SfpUtil"

PLATFORM_SPECIFIC_OPTICTEMP_MODULE_NAME = "optictemputil"
PLATFORM_SPECIFIC_OPTICTEMP_CLASS_NAME = "OpticTempUtil"

PLATFORM_SPECIFIC_CPUTEMP_MODULE_NAME = "cputemputil"
PLATFORM_SPECIFIC_CPUTEMP_CLASS_NAME = "CpuTempUtil"

platform_sfputil = None
platform_optictemputil = None
platform_cputemputil = None


# Returns platform and HW SKU
def get_platform_and_hwsku():
    try:
        proc = subprocess.Popen([SONIC_CFGGEN_PATH, '-H', '-v', PLATFORM_KEY],
                                stdout=subprocess.PIPE,
                                shell=False,
                                stderr=subprocess.STDOUT)
        stdout = proc.communicate()[0]
        proc.wait()
        platform = stdout.rstrip('\n')

        proc = subprocess.Popen([SONIC_CFGGEN_PATH, '-d', '-v', HWSKU_KEY],
                                stdout=subprocess.PIPE,
                                shell=False,
                                stderr=subprocess.STDOUT)
        stdout = proc.communicate()[0]
        proc.wait()
        hwsku = stdout.rstrip('\n')
    except OSError, e:
        raise OSError("Cannot detect platform")

    return (platform, hwsku)


# Returns path to port config file
def get_path_to_port_config_file():
    # Get platform and hwsku
    (platform, hwsku) = get_platform_and_hwsku()

    # Load platform module from source
    platform_path = "/".join([PLATFORM_ROOT_PATH, platform])
    hwsku_path = "/".join([platform_path, hwsku])

    # First check for the presence of the new 'port_config.ini' file
    port_config_file_path = "/".join([hwsku_path, "port_config.ini"])
    if not os.path.isfile(port_config_file_path):
        # port_config.ini doesn't exist. Try loading the legacy 'portmap.ini' file
        port_config_file_path = "/".join([hwsku_path, "portmap.ini"])

    return port_config_file_path


def load_platform_util(module_name, class_name):

    # Get platform and hwsku
    (platform, hwsku) = get_platform_and_hwsku()

    # Load platform module from source
    platform_path = "/".join([PLATFORM_ROOT_PATH, platform])
    hwsku_path = "/".join([platform_path, hwsku])

    try:
        module_file = "/".join([platform_path, "plugins", module_name + ".py"])
        module = imp.load_source(module_name, module_file)
    except IOError, e:
        print("Failed to load platform module '%s': %s" % (module_name, str(e)), True)
        return -1

    try:
        platform_util_class = getattr(module, class_name)
        platform_util = platform_util_class()
    except AttributeError, e:
        print("Failed to instantiate '%s' class: %s" % (class_name, str(e)), True)
        return -2

    return platform_util


# Send CPU temperature to BMC.
def send_cpu_temp():
    max_cpu_tmp = platform_cputemputil.get_max_cpu_tmp()
    json_input = {
        "chip":"cpu",
        "option":"input",
        "value": str(int(max_cpu_tmp))
    }
    print "send ", json_input
    requests.post(TEMP_URL, json=json_input)



# Send Optic moudule temperature to BMC.
def send_optic_temp():
    port_config_file_path = get_path_to_port_config_file()  
    platform_sfputil.read_porttab_mappings(port_config_file_path)
    port_list = platform_sfputil.port_to_i2cbus_mapping
    qsfp_port_list = platform_sfputil.qsfp_ports

    temp_list = []
    port_type = "SFP"
    for port_num, bus_num in port_list.items():
        if port_num in qsfp_port_list:
            port_type = "QSFP"

        temp = platform_optictemputil.get_optic_temp(bus_num, port_type)
        temp_list.append(float(temp))

    max_optic_temp = max(temp_list)
    json_input = {
        "chip":"optical",
        "option":"input",
        "value": str(int(max_optic_temp))
    }
    print "send ", json_input
    requests.post(TEMP_URL, json=json_input)


def main():
    global platform_sfputil
    global platform_cputemputil
    global platform_optictemputil

    try:
        platform_sfputil = load_platform_util(PLATFORM_SPECIFIC_SFP_MODULE_NAME, PLATFORM_SPECIFIC_SFP_CLASS_NAME)
        platform_cputemputil = load_platform_util(PLATFORM_SPECIFIC_CPUTEMP_MODULE_NAME, PLATFORM_SPECIFIC_CPUTEMP_CLASS_NAME)
        platform_optictemputil = load_platform_util(PLATFORM_SPECIFIC_OPTICTEMP_MODULE_NAME, PLATFORM_SPECIFIC_OPTICTEMP_CLASS_NAME)

        send_cpu_temp()
        send_optic_temp()
    except Exception, e:
         print e
         pass


if __name__== "__main__":
  main()
