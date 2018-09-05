#ifndef CPLD_DRIVER_HEAD
#define CPLD_DRIVER_HEAD

#include "pcie_fpga.h"

extern struct platform_device midstone100th2_dev;
extern struct platform_driver midstone100th2_drv;

extern struct mutex fpga_i2c_master_locks[I2C_MASTER_CH_TOTAL];
extern uint16_t fpga_i2c_lasted_access_port[I2C_MASTER_CH_TOTAL];

static struct i2c_board_info sff_eeprom_info = {
    I2C_BOARD_INFO("sff8436", 0x50),
};

static struct kobject *cpld_b = NULL;
static struct kobject *cpld1 = NULL;
static struct kobject *cpld2 = NULL;
static struct kobject *cpld3 = NULL;
static struct kobject *cpld4 = NULL;
static struct kobject *fpga = NULL;
extern struct device *sff_dev;


#endif // END IF CPLD_DRIVER_HEAD