import syslog
import subprocess
import sys
import re
from datetime import datetime


def run_command(command):
    try:
        proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
        (out, err) = proc.communicate()
    except IOError as e:
        sys.exit(0)
    return out


def add_syslog(date, time, pmon_format, title, message, status):
    # Add log to syslog
    message = pmon_format + " : " + date + " " + time + " | " + title + " | " + message + " | " + status
    syslog.syslog(syslog.LOG_WARNING, message)


def get_sel():
    # Get event from BMC
    sel_str = run_command('ipmitool sel list')
    return sel_str.splitlines()


def get_bmc_time():
    # Get BMC time
    bmc_time = run_command('ipmitool sel time get')
    return str(bmc_time)


def diff_time_min(stop, start):
    # Get difference of time in minutes
    td = stop - start
    return int(round(td.total_seconds() / 60))


def sync_time():
    # Sync time between BMC and SONiC
    bmc_time = get_bmc_time().rstrip('\n')
    bmc_datetime = datetime.strptime(bmc_time, '%m/%d/%Y %H:%M:%S')
    td_mins = diff_time_min(datetime.now(), bmc_datetime)
    if td_mins != 0:
        run_command('ipmitool sel time set now')


def get_exist_log():
    # Get existing bmc sel on syslog
    sel_log = run_command('cat /var/log/syslog | grep sel_job.py')
    sel_log_list = sel_log.splitlines() if sel_log else []
    exist_timestamp = []
    for str_data in sel_log_list:
        try:
            date_string = re.search(r'\d{2}/\d{2}/\d{4} \d{2}:\d{2}:\d{2}', str_data)
            sel_datetime = datetime.strptime(date_string.group(), '%m/%d/%Y %H:%M:%S')
            exist_timestamp.append(sel_datetime)
        except:
            pass
    return exist_timestamp


def set_fan_format(status, message):
    # Set fan syslog format
    status = "UNPLUG" if status == "Deasserted" else "PLUG_IN" if status == "Asserted" else "FAILED"
    status = "FAILED" if message != "" else status
    return "%PMON-0-FAN_" + status


def set_temp_format(message):
    status = "HIGH" if "Upper" in message else "LOW" if "Lower" in message else "ERROR"
    return "%PMON-0-TEMP_" + status


def set_psu_format(message, psu_status):
    status = "UNPLUG" if "lost" in message else "FAILED" if "Failure" in message else "ERROR"
    out = True

    if "lost" in message and psu_status == "Asserted":
        status = "UNPLUG"
    elif "lost" in message and psu_status == "Deasserted":
        status = "PLUG_IN"
    elif "Presence" in message and psu_status == "Deasserted":
        status = "UNPLUG"
    elif "Presence" in message and psu_status == "Asserted":
        status = "PLUG_IN"
    elif "Failure" in message and psu_status == "Asserted":
        status = "FAILED"
    else:
        out = False

    return "%PMON-0-PSU_" + status, out


def set_volt_format(message):
    status = "HIGH" if "Upper" in message else "LOW" if "Lower" in message else "ERROR"
    return "%PMON-0-VOL_" + status


def main():
    try:
        # Check datetime
        sync_time()
        # Check existing log
        ex_log_ts = get_exist_log()
        # Check event
        sel_list = get_sel()

        for sel_data in sel_list:
            sel_temp_info_list = [x.strip() for x in sel_data.split('|')]
            date, time, title, message, status = sel_temp_info_list[1:6]
            sel_datetime = datetime.strptime(date + time, '%m/%d/%Y%H:%M:%S')
            if sel_datetime not in ex_log_ts:

                if title.split()[0] == "Fan":
                    pmon_format = set_fan_format(status, message)
                    add_syslog(date, time, pmon_format, title, message, status)
                elif title.split()[0] == "Temperature":
                    pmon_format = set_temp_format(message)
                    add_syslog(date, time, pmon_format, title, message, status)
                elif title.split()[0] == "Power":
                    pmon_format, out = set_psu_format(message, status)
                    if out:
                        add_syslog(date, time, pmon_format, title, message, status)
                elif title.split()[0] == "Voltage":
                    pmon_format = set_volt_format(message)
                    add_syslog(date, time, pmon_format, title, message, status)

                else:
                    add_syslog(date, time, "OTHER_" + title.split()[0], title, message, status)

    except Exception as e:
        sys.exit(0)


if __name__ == '__main__':
    main()
