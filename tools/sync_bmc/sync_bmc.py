#!/usr/bin/env python

#############################################################################
# Fishbone
#
# Platform and model specific service for send data to BMC
#
#############################################################################

import subprocess
import requests
import os
from cputemputil import CpuTempUtil


PLATFORM_ROOT_PATH = '/usr/share/sonic/device'
SONIC_CFGGEN_PATH = '/usr/local/bin/sonic-cfggen'
HWSKU_KEY = 'DEVICE_METADATA.localhost.hwsku'
PLATFORM_KEY = 'DEVICE_METADATA.localhost.platform'
TEMP_URL = 'http://[fe80::1:1%eth0.4088]:8080/api/sys/temp'

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


# Send CPU temperature to BMC.
def send_cpu_temp():
    max_cpu_tmp = CpuTempUtil().get_max_cpu_tmp()
    json_input = {
        "chip":"cpu",
        "option":"input",
        "value": str(max_cpu_tmp)
    }
    print "send ", json_input
    requests.post(TEMP_URL, json=json_input)


# Send Optic moudule temperature to BMC.
def send_optic_temp():
    # port_config_file_path = get_path_to_port_config_file()
    return NotImplemented


def main():
    try:
        send_cpu_temp()
        # send_optic_temp()
    except:
        pass


if __name__== "__main__":
  main()
