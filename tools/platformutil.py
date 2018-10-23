#!/usr/bin/env python
#
# platformutil/main.py
#
# Command-line utility for interacting with PSU in SONiC
#
# example output
# platformutil psu status
# PSU     Presence        Status      PN                  SN
# PSU 1   PRESENT         OK          CSU550AP-3-300      M623TW004ZAAL
# PSU 2   NOT_PRESENT     N/A         N/A                 N/A
#
# platformutil fan status
# FAN     Status          Speed           Low_thd             High_thd        PN              SN
# FAN 1   OK              10169 RPM       300 RPM             16000 RPM       M6510-FAN-F     1000000000014
# FAN 2   NOT_OK          20000 RPM       300 RPM             16000 RPM       M6510-FAN-F     1000000000014
#
# platformutil sensor status
#Sensor             InputName            State    Value     Low_thd    High_thd
#-----------------  -------------------  -------  --------  ---------  ----------
#syscpld-i2c-0-0d   CPU temp             NOT_OK   41.0 C    0 C        0.0 C
#syscpld-i2c-0-0d   Optical temp         NOT_OK   26.0 C    0 C        0.0 C
#syscpld-i2c-0-0d   Switch temp          NOT_OK   35.0 C    0 C        0.0 C
#
# should implenmet the below classes in the specified plugin
#
# class PsuUtil:
#     int get_num_psus();                 //get the number of power supply units
#     bool get_psu_presence(int index)    //get the power status of the psu, index:1,2
#     bool get_psu_status(int index)      //get the running status of the psu,index:1,2
#     str get_psu_sn(int index)       //get the serial number of the psu, return value example: "M623TW004ZAAL"
#     str get_psu_pn(int index)       //get the product name of the psu, return value example: "CSU550AP-3-300"
#
# class FanUtil:
#     int get_fans_name_list();     //get the names of all the fans(FAN1-1,FAN1-2,FAN2-1,FAN2-2...)
#     int get_fan_speed(int index);   //get the current speed of the fan, the unit is "RPM"
#     int get_fan_low_threshold(int index); //get the low speed threshold of the fan, if the current speed < low speed threshold, the status of the fan is ok.
#     int get_fan_high_threshold(int index); //get the hight speed threshold of the fan, if the current speed > high speed threshold, the status of the fan is not ok
#     str get_fan_pn(int index);//get the product name of the fan
#     str get_fan_sn(int index);//get the serial number of the fan
#
# class SensorUtil:
#     int get_num_sensors();  //get the number of sensors
#     int get_sensor_input_num(int index); //get the number of the input items of the specified sensor
#     str get_sensor_name(int index);// get the device name of the specified sensor.for example "coretemp-isa-0000"
#     str get_sensor_input_name(int sensor_index, int input_index); //get the input item name of the specified input item of the specified sensor index, for example "Physical id 0"
#     str get_sensor_input_type(int sensor_index, int input_index); //get the item type of the specified input item of the specified sensor index, the return value should
#                                                                   //among  "voltage","temperature"...
#     float get_sensor_input_value(int sensor_index, int input_index);//get the current value of the input item, the unit is "V" or "C"...
#     float get_sensor_input_low_threshold(int sensor_index, int input_index); //get the low threshold of the value, the status of this item is not ok if the current
#                                                                                 //value<low_threshold
#     float get_sensor_input_high_threshold(int sensor_index, int input_index); //get the high threshold of the value, the status of this item is not ok if the current
#                                                                                   // value > high_threshold

try:
    import sys
    import os
    import subprocess
    import click
    import imp
    import syslog
    import types
    import traceback
    from tabulate import tabulate
except ImportError as e:
    raise ImportError("%s - required module not found" % str(e))

VERSION = '1.0'

SYSLOG_IDENTIFIER = "platformutil"
PLATFORM_PSU_MODULE_NAME = "psuutil"
PLATFORM_PSU_CLASS_NAME = "PsuUtil"

#gongjian add
PLATFORM_SENSOR_MODULE_NAME = "sensorutil"
PLATFORM_SENSOR_CLASS_NAME = "SensorUtil"

PLATFORM_FAN_MODULE_NAME = "fanutil"
PLATFORM_FAN_CLASS_NAME = "FanUtil"
#end gongjian add

PLATFORM_ROOT_PATH = '/usr/share/sonic/device'
PLATFORM_ROOT_PATH_DOCKER = '/usr/share/sonic/platform'
SONIC_CFGGEN_PATH = '/usr/local/bin/sonic-cfggen'
MINIGRAPH_PATH = '/etc/sonic/minigraph.xml'
HWSKU_KEY = "DEVICE_METADATA['localhost']['hwsku']"
PLATFORM_KEY = "DEVICE_METADATA['localhost']['platform']"

# Global platform-specific psuutil class instance
platform_psuutil = None

#gongjian add
platform_sensorutil = None
Platform_fanutil = None
#end gongjian add

# ========================== Syslog wrappers ==========================


def log_info(msg, also_print_to_console=False):
    syslog.openlog(SYSLOG_IDENTIFIER)
    syslog.syslog(syslog.LOG_INFO, msg)
    syslog.closelog()

    if also_print_to_console:
        click.echo(msg)


def log_warning(msg, also_print_to_console=False):
    syslog.openlog(SYSLOG_IDENTIFIER)
    syslog.syslog(syslog.LOG_WARNING, msg)
    syslog.closelog()

    if also_print_to_console:
        click.echo(msg)


def log_error(msg, also_print_to_console=False):
    syslog.openlog(SYSLOG_IDENTIFIER)
    syslog.syslog(syslog.LOG_ERR, msg)
    syslog.closelog()

    if also_print_to_console:
        click.echo(msg)


# ==================== Methods for initialization ====================

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


# Loads platform specific psuutil module from source
def load_platform_util():
    global platform_psuutil
    #gongjian add
    global platform_sensorutil
    global platform_fanutil

    # Get platform and hwsku
    (platform, hwsku) = get_platform_and_hwsku()

    # Load platform module from source
    platform_path = ''
    if len(platform) != 0:
        platform_path = "/".join([PLATFORM_ROOT_PATH, platform])
    else:
        platform_path = PLATFORM_ROOT_PATH_DOCKER
    hwsku_path = "/".join([platform_path, hwsku])

    try:
        module_file_psu = "/".join([platform_path, "plugins", PLATFORM_PSU_MODULE_NAME + ".py"])
        module_psu = imp.load_source(PLATFORM_PSU_MODULE_NAME, module_file_psu)
    except IOError, e:
        log_error("Failed to load platform module '%s': %s" % (PLATFORM_PSU_MODULE_NAME, str(e)), True)
        return -1

    try:
        platform_psuutil_class = getattr(module_psu, PLATFORM_PSU_CLASS_NAME)
        platform_psuutil = platform_psuutil_class()
    except AttributeError, e:
        log_error("Failed to instantiate '%s' class: %s" % (PLATFORM_PSU_CLASS_NAME, str(e)), True)
        return -2


    #gongjian add
    try:
        module_file_sensor = "/".join([platform_path, "plugins", PLATFORM_SENSOR_MODULE_NAME + ".py"])
        module_sensor = imp.load_source(PLATFORM_SENSOR_MODULE_NAME, module_file_sensor)
    except IOError, e:
        log_error("Failed to load platform module '%s': %s" % (PLATFORM_SENSOR_MODULE_NAME, str(e)), True)
        return -1

    try:
        platform_sensorutil_class = getattr(module_sensor, PLATFORM_SENSOR_CLASS_NAME)
        platform_sensorutil = platform_sensorutil_class()
    except AttributeError, e:
        log_error("Failed to instantiate '%s' class: %s" % (PLATFORM_SENSOR_CLASS_NAME, str(e)), True)
        return -2

    try:
        module_file_fan = "/".join([platform_path, "plugins", PLATFORM_FAN_MODULE_NAME + ".py"])
        module_fan = imp.load_source(PLATFORM_FAN_MODULE_NAME, module_file_fan)
    except IOError, e:
        log_error("Failed to load platform module '%s': %s" % (PLATFORM_FAN_MODULE_NAME, str(e)), True)
        return -1

    try:
        platform_fanutil_class = getattr(module_fan, PLATFORM_FAN_CLASS_NAME)
        platform_fanutil = platform_fanutil_class()
    except AttributeError, e:
        log_error("Failed to instantiate '%s' class: %s" % (PLATFORM_FAN_CLASS_NAME, str(e)), True)
        return -2
    #end gongjian add
    return 0


# ==================== CLI commands and groups ====================


# This is our main entrypoint - the main 'psuutil' command
@click.group()
def cli():
    """platformutil - Command line utility for providing platform status"""

    if os.geteuid() != 0:
        click.echo("Root privileges are required for this operation")
        sys.exit(1)

    # Load platform-specific psuutil, fanutil and sensorutil class
    err = load_platform_util()
    if err != 0:
        sys.exit(2)

#'fan' subcommand
@cli.group()
@click.pass_context
def fan(ctx):
    """fan state"""
    ctx.obj = "fan"

# 'sensor' subcommand
@cli.group()
@click.pass_context
def sensor(ctx):
    """sensor state"""
    ctx.obj = "sensor"

# 'psu' subcommand
@cli.group()
@click.pass_context
def psu(ctx):
    """psu state"""
    ctx.obj = "psu"

# 'version' subcommand
@cli.command()
def version():
    """Display version info"""
    click.echo("platformutil version {0}".format(VERSION))


# 'num' subcommand
@click.command()
@click.pass_context
def num(ctx):
    """Display number of supported sensor/fan/psu device"""
    if ctx.obj == "psu":
        click.echo(str(platform_psuutil.get_num_psus()))
    if ctx.obj == "fan":
        click.echo(str(len(platform_fanutil.get_fans_name_list())))
    if ctx.obj == "sensor":
        click.echo(str(platform_sensorutil.get_num_sensors()))

psu.add_command(num)
sensor.add_command(num)
fan.add_command(num)


# 'status' subcommand
#all API should return "N/A" or float("-intf") if not supported
@click.command()
@click.pass_context
def status(ctx):
    """Display PSU/FAN/SENSOR status"""
    if ctx.obj == "psu":
        supported_psu = range(1, platform_psuutil.get_num_psus() + 1)
        header = ['PSU', 'Presence', 'Status', 'PN', 'SN']
        status_table = []

        for psu in supported_psu:
            presence_str = "N/A"
            status_str = "N/A"
            #product name
            pn = "N/A"
            # serial number
            sn = "N/A"
            psu_name = "PSU {}".format(psu)
            presence = platform_psuutil.get_psu_presence(psu)
            if presence:
                presence_str = 'PRESENT'
                oper_status = platform_psuutil.get_psu_status(psu)
                status_str = 'OK' if oper_status else "NOT_OK"
                sn = platform_psuutil.get_psu_sn(psu)
                pn = platform_psuutil.get_psu_pn(psu)
            else:
                presence_str = 'NOT_PRESENT'
            status_table.append([psu_name,presence_str, status_str,pn,sn])
        if status_table:
            click.echo(tabulate(status_table, header, tablefmt="simple"))

    if ctx.obj == "fan":
        supported_fans = platform_fanutil.get_fans_name_list()
        header = ['FAN','Status','Speed','Low_thd','High_thd','PN','SN']
        status_table = []

        for fan in supported_fans:
            speed = float("-inf")
            low_threshold = float("-inf")
            high_threshold = float("-inf")
            pn = "N/A"
            sn = "N/A"
            status = "N/A"
            speed = platform_fanutil.get_fan_speed(fan)
            low_threshold = platform_fanutil.get_fan_low_threshold(fan)
            high_threshold = platform_fanutil.get_fan_high_threshold(fan)
            if (float(speed) > float(high_threshold)) or ( float(speed) < float(low_threshold)):
                status = "NOT_OK"
            else :
                status = "OK"
            pn = platform_fanutil.get_fan_pn(fan)
            sn = platform_fanutil.get_fan_sn(fan)
            status_table.append([fan,status,(str(speed)+" RPM"),\
                                 (str(low_threshold)+" RPM"),(str(high_threshold)+" RPM"),pn,sn])
        if status_table:
            click.echo(tabulate(status_table, header, tablefmt="simple"))

    if ctx.obj == "sensor":
        supported_sensors = range(1,platform_sensorutil.get_num_sensors()+1)
        header = ['Sensor','InputName', 'State', 'Value', 'Low_thd','High_thd']
        status_table = []
        for sensor in supported_sensors:
            sensor_name = "N/A"
            supported_input_num = 0

            supported_input_num = platform_sensorutil.get_sensor_input_num(sensor)
            sensor_name = platform_sensorutil.get_sensor_name(sensor)
            supported_input = range(1,supported_input_num+1)
            for input in supported_input:
                input_name = "N/A"
                input_type = "temperature"# "temperature" or "voltage"
                value = float("-inf")
                low_thd = float("-inf")
                high_thd = float("-inf")
                suffix = " "

                input_name = platform_sensorutil.get_sensor_input_name(sensor,input)
                input_type = platform_sensorutil.get_sensor_input_type(sensor, input)
                if input_type == "temperature":
                    suffix = ' C'
                elif input_type == "voltage":
                    suffix = ' V'
                elif input_type  == "RPM":
                    suffix = ' RPM'
                elif input_type  == "power":
                    suffix = ' W'
                value = platform_sensorutil.get_sensor_input_value(sensor, input)
                low_thd = platform_sensorutil.get_sensor_input_low_threshold(sensor, input)
                high_thd = platform_sensorutil.get_sensor_input_high_threshold(sensor, input)
                state = "NOT_OK"
                if float(value)>float(low_thd) and float(value)<float(high_thd):
                    state = "OK"

                status_table.append([sensor_name,input_name,state, (str(value)+suffix),(str(low_thd)+suffix),(str(high_thd)+suffix)])
        if status_table:
            click.echo(tabulate(status_table, header, tablefmt="simple"))

psu.add_command(status)
sensor.add_command(status)
fan.add_command(status)


if __name__ == '__main__':
    cli()
