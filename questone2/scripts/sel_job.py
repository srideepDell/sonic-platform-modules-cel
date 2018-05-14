import syslog
import subprocess
import os
import sys
from datetime import datetime

def run_command(command):
    try:
	proc = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE)
    	(out, err) = proc.communicate()   
	if proc.returncode != 0:
		sys.exit(proc.returncode)          
    except IOError as e:
	sys.exit(0)   
    
    return out

def add_syslog(message):
    # Add log to syslog
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

def main():
    try:
        # Check datetime
        sync_time()

	# Check event
        sel_list = get_sel()
        sel_temp_list = matching = [s for s in sel_list if "Temperature" in s]
        for sel_temp in sel_temp_list:
            sel_temp_info_list = [x.strip() for x in sel_temp.split('|')] 
            date, time, title, message = sel_temp_info_list[1:5]
            sel_datetime = datetime.strptime(date+time, '%m/%d/%Y%H:%M:%S')
            td_mins = diff_time_min(datetime.now(), sel_datetime)
            if td_mins <= 5:
                add_syslog("WARNING : "+ date + " " + time +" "+ title + " " +  message)
    except Exception as e:
        sys.exit(0)
	
if __name__ == '__main__':
    main()
