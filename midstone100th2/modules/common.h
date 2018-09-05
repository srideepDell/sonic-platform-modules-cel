#ifndef COMMON_HEAD
#define COMMON_HEAD

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <uapi/linux/stat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define MOD_VERSION "0.1.0"
#define DRIVER_NAME "midstone100th2"

#ifdef DEBUG_KERN
#define info(fmt,args...)  printk(KERN_INFO "line %3d : "fmt,__LINE__,##args)
#define check(REG)         printk(KERN_INFO "line %3d : %-8s = %2.2X",__LINE__,#REG,ioread8(REG));
#else
#define info(fmt,args...)
#define check(REG)
#endif


/*================================
*  Defined 
* =================================
*/

/*   Dummy    */
/* FPGA FRONT PANEL PORT MGMT */
#define SFF_PORT_CTRL_BASE             0x4000
#define SFF_PORT_STATUS_BASE           0x4004
#define SFF_PORT_INT_STATUS_BASE       0x4008
#define SFF_PORT_INT_MASK_BASE         0x400c

#define PORT_XCVR_REGISTER_SIZE        0x1000

/* I2C_MASTER BASE ADDR */
#define I2C_MASTER_FREQ_1              0x0100
#define I2C_MASTER_CTRL_1              0x0104
#define I2C_MASTER_STATUS_1            0x0108
#define I2C_MASTER_DATA_1              0x010c
#define I2C_MASTER_PORT_ID_1           0x0110

#define I2C_MASTER_CH_1                1
#define I2C_MASTER_CH_2                2
#define I2C_MASTER_CH_3                3
#define I2C_MASTER_CH_4                4
#define I2C_MASTER_CH_5                5
#define I2C_MASTER_CH_6                6
#define I2C_MASTER_CH_7                7
#define I2C_MASTER_CH_8                8
#define I2C_MASTER_CH_9                9
#define I2C_MASTER_CH_10               10
#define I2C_MASTER_CH_11               11
#define I2C_MASTER_CH_TOTAL            I2C_MASTER_CH_11

#define VIRTUAL_I2C_BUS_OFFSET         10
#define VIRTUAL_I2C_QSFP_PORT          64
#define VIRTUAL_I2C_SFP_PORT           2
#define VIRTUAL_I2C_POWER_CHIP_PORT    1
#define VIRTUAL_I2C_CPLD_PORT          1
#define VIRTUAL_I2C_CPLD_B_PORT        1
#define VIRTUAL_I2C_PSU                1
#define VIRTUAL_I2C_FAN_TRAY           4
#define VIRTUAL_I2C_POWER_MON          1
#define VIRTUAL_I2C_LM75               1
#define VIRTUAL_I2C_EEPROM             1
#define SFF_PORT_TOTAL VIRTUAL_I2C_QSFP_PORT + VIRTUAL_I2C_SFP_PORT
#define VIRTUAL_I2C_PORT_LENGTH VIRTUAL_I2C_SFP_PORT + \
                                VIRTUAL_I2C_QSFP_PORT + \
                                VIRTUAL_I2C_POWER_CHIP_PORT + \
                                VIRTUAL_I2C_CPLD_PORT + \
                                VIRTUAL_I2C_CPLD_B_PORT + \
                                VIRTUAL_I2C_PSU + \
                                VIRTUAL_I2C_FAN_TRAY + \
                                VIRTUAL_I2C_POWER_MON + \
                                VIRTUAL_I2C_LM75 + \
                                VIRTUAL_I2C_EEPROM

#define VIRTUAL_I2C_CPLD_INDEX         66
#define VIRTUAL_I2C_CPLD_B_INDEX       67
#define CPLD_B_SLAVE_ADDR              0x0d
#define CPLD1_SLAVE_ADDR               0x30
#define CPLD2_SLAVE_ADDR               0x31
#define CPLD3_SLAVE_ADDR               0x32
#define CPLD4_SLAVE_ADDR               0x33

enum
{
    I2C_SR_BIT_RXAK = 0,
    I2C_SR_BIT_MIF,
    I2C_SR_BIT_SRW,
    I2C_SR_BIT_BCSTM,
    I2C_SR_BIT_MAL,
    I2C_SR_BIT_MBB,
    I2C_SR_BIT_MAAS,
    I2C_SR_BIT_MCF
};

enum
{
    I2C_CR_BIT_BCST = 0,
    I2C_CR_BIT_RSTA = 2,
    I2C_CR_BIT_TXAK,
    I2C_CR_BIT_MTX,
    I2C_CR_BIT_MSTA,
    I2C_CR_BIT_MIEN,
    I2C_CR_BIT_MEN,
};

/*****************************
 *  Structure
 *****************************/
struct sff_device_data
{
    int portid;
};

struct i2c_switch
{
    unsigned char master_bus;  // I2C bus number
    unsigned char switch_addr; // PCA9548 device address, 0xFF if directly connect to a bus.
    unsigned char channel;     // PCA9548 channel number. If the switch_addr is 0xFF, this value is ignored.
    char calling_name[20];     // Calling name.
};

#define EVT_BOARD

static struct i2c_switch fpga_i2c_bus_dev[] = {
#ifdef EVT_BOARD
    /* BUS2 QSFP Exported as virtual bus */
    /* PCA9548_1*/
    {I2C_MASTER_CH_2, 0x72, 0, "QSFP1"}, //10-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 1, "QSFP2"}, //11-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 3, "QSFP3"}, //12-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 2, "QSFP4"}, //13-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 4, "QSFP5"}, //14-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 5, "QSFP6"}, //15-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 7, "QSFP7"}, //16-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 6, "QSFP8"}, //17-0050 PASS
    /* PCA9548_2*/
    {I2C_MASTER_CH_2, 0x73, 0, "QSFP9"}, //18-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 1, "QSFP10"}, //19-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 3, "QSFP11"}, //20-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 2, "QSFP12"}, //21-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 4, "QSFP13"}, //22-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 5, "QSFP14"}, //23-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 7, "QSFP15"}, //24-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 6, "QSFP16"}, //25-0050 PASS
    /* PCA9548_3*/
    {I2C_MASTER_CH_2, 0x74, 0, "QSFP17"}, //26-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 1, "QSFP18"}, //27-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 3, "QSFP19"}, //28-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 2, "QSFP20"}, //29-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 4, "QSFP21"}, //30-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 5, "QSFP22"}, //31-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 7, "QSFP23"}, //32-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 6, "QSFP24"}, //33-0050 PASS
    /* PCA9548_4*/
    {I2C_MASTER_CH_2, 0x75, 0, "QSFP25"}, //34-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 1, "QSFP26"}, //35-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 3, "QSFP27"}, //36-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 2, "QSFP28"}, //37-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 4, "QSFP29"}, //38-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 5, "QSFP30"}, //39-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 7, "QSFP31"}, //40-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 6, "QSFP32"}, //41-0050 PASS
    /* BUS3 QSFP Exported as virtual bus */
    /* PCA9548_5*/
    {I2C_MASTER_CH_3, 0x72, 0, "QSFP33"}, //42-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 1, "QSFP34"}, //43-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 3, "QSFP35"}, //44-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 2, "QSFP36"}, //45-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 4, "QSFP37"}, //46-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 5, "QSFP38"}, //47-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 7, "QSFP39"}, //48-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 6, "QSFP40"}, //49-0050 PASS
    /* PCA9548_6*/
    {I2C_MASTER_CH_3, 0x73, 0, "QSFP41"}, //50-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 1, "QSFP42"}, //51-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 3, "QSFP43"}, //52-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 2, "QSFP44"}, //53-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 4, "QSFP45"}, //54-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 5, "QSFP46"}, //55-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 7, "QSFP47"}, //56-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 6, "QSFP48"}, //57-0050 PASS
    /* PCA9548_7*/
    {I2C_MASTER_CH_3, 0x74, 0, "QSFP49"}, //58-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 1, "QSFP50"}, //59-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 3, "QSFP51"}, //60-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 2, "QSFP52"}, //61-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 4, "QSFP53"}, //62-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 5, "QSFP54"}, //63-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 7, "QSFP55"}, //64-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 6, "QSFP56"}, //65-0050 PASS
    /* PCA9548_8*/
    {I2C_MASTER_CH_3, 0x75, 0, "QSFP57"}, //66-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 1, "QSFP58"}, //67-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 3, "QSFP59"}, //68-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 2, "QSFP60"}, //69-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 4, "QSFP61"}, //70-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 5, "QSFP62"}, //71-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 7, "QSFP63"}, //72-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 6, "QSFP64"}, //73-0050 PASS
    /* BUS1 SFP+ Exported as virtual bus */
    /* PCA9548_9*/
    {I2C_MASTER_CH_1, 0x72, 0, "SFP1"},
    {I2C_MASTER_CH_1, 0x72, 1, "SFP2"},
    /* BUS4 CPLD Access via SYSFS */
    {I2C_MASTER_CH_4, 0xFF, 0, "CPLD"},
    /* BUS5 CPLD_B */
    {I2C_MASTER_CH_5, 0xFF, 0, "CPLD_B"},
    /* BUS6 POWER CHIP Exported as virtual bus */
    {I2C_MASTER_CH_6, 0xFF, 0, "POWER"},
    /* BUS7 PSU */
    {I2C_MASTER_CH_7, 0xFF, 0, "PSU"},
    /* BUS8 FAN */
    /* Channel 2 is no hardware connected */
    {I2C_MASTER_CH_8, 0x77, 0, "FAN5"},
    {I2C_MASTER_CH_8, 0x77, 1, "FAN4"},
    {I2C_MASTER_CH_8, 0x77, 3, "FAN2"},
    {I2C_MASTER_CH_8, 0x77, 4, "FAN1"},
    /* BUS9 POWER MONITOR */
    {I2C_MASTER_CH_9, 0xFF, 0, "UCD90120"},
    /* BUS10 LM75 */
    {I2C_MASTER_CH_10, 0xFF, 0, "LM75"},
    /* BUS11 EEPROM */
    {I2C_MASTER_CH_11, 0xFF, 0, "EEPROM"},
#else
    /* BUS2 QSFP Exported as virtual bus */
    /* PCA9548_1*/
    {I2C_MASTER_CH_2, 0x72, 0, "QSFP1"}, //10-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 1, "QSFP2"}, //11-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 2, "QSFP3"}, //12-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 3, "QSFP4"}, //13-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 4, "QSFP5"}, //14-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 5, "QSFP6"}, //15-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 6, "QSFP7"}, //16-0050 PASS
    {I2C_MASTER_CH_2, 0x72, 7, "QSFP8"}, //17-0050 PASS
    /* PCA9548_2*/
    {I2C_MASTER_CH_2, 0x73, 0, "QSFP9"}, //18-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 1, "QSFP10"}, //19-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 2, "QSFP11"}, //20-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 3, "QSFP12"}, //21-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 4, "QSFP13"}, //22-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 5, "QSFP14"}, //23-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 6, "QSFP15"}, //24-0050 PASS
    {I2C_MASTER_CH_2, 0x73, 7, "QSFP16"}, //25-0050 PASS
    /* PCA9548_3*/
    {I2C_MASTER_CH_2, 0x74, 0, "QSFP17"}, //26-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 1, "QSFP18"}, //27-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 2, "QSFP19"}, //28-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 3, "QSFP20"}, //29-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 4, "QSFP21"}, //30-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 5, "QSFP22"}, //31-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 6, "QSFP23"}, //32-0050 PASS
    {I2C_MASTER_CH_2, 0x74, 7, "QSFP24"}, //33-0050 PASS
    /* PCA9548_4*/
    {I2C_MASTER_CH_2, 0x75, 0, "QSFP25"}, //34-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 1, "QSFP26"}, //35-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 2, "QSFP27"}, //36-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 3, "QSFP28"}, //37-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 4, "QSFP29"}, //38-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 5, "QSFP30"}, //39-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 6, "QSFP31"}, //40-0050 PASS
    {I2C_MASTER_CH_2, 0x75, 7, "QSFP32"}, //41-0050 PASS
    /* BUS3 QSFP Exported as virtual bus */
    /* PCA9548_5*/
    {I2C_MASTER_CH_3, 0x72, 0, "QSFP33"}, //42-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 1, "QSFP34"}, //43-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 2, "QSFP35"}, //44-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 3, "QSFP36"}, //45-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 4, "QSFP37"}, //46-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 5, "QSFP38"}, //47-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 6, "QSFP39"}, //48-0050 PASS
    {I2C_MASTER_CH_3, 0x72, 7, "QSFP40"}, //49-0050 PASS
    /* PCA9548_6*/
    {I2C_MASTER_CH_3, 0x73, 0, "QSFP41"}, //50-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 1, "QSFP42"}, //51-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 2, "QSFP43"}, //52-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 3, "QSFP44"}, //53-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 4, "QSFP45"}, //54-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 5, "QSFP46"}, //55-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 6, "QSFP47"}, //56-0050 PASS
    {I2C_MASTER_CH_3, 0x73, 7, "QSFP48"}, //57-0050 PASS
    /* PCA9548_7*/
    {I2C_MASTER_CH_3, 0x74, 0, "QSFP49"}, //58-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 1, "QSFP50"}, //59-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 2, "QSFP51"}, //60-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 3, "QSFP52"}, //61-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 4, "QSFP53"}, //62-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 5, "QSFP54"}, //63-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 6, "QSFP55"}, //64-0050 PASS
    {I2C_MASTER_CH_3, 0x74, 7, "QSFP56"}, //65-0050 PASS
    /* PCA9548_8*/
    {I2C_MASTER_CH_3, 0x75, 0, "QSFP57"}, //66-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 1, "QSFP58"}, //67-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 2, "QSFP59"}, //68-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 3, "QSFP60"}, //69-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 4, "QSFP61"}, //70-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 5, "QSFP62"}, //71-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 6, "QSFP63"}, //72-0050 PASS
    {I2C_MASTER_CH_3, 0x75, 7, "QSFP64"}, //73-0050 PASS
    /* BUS1 SFP+ Exported as virtual bus */
    /* PCA9548_9*/
    {I2C_MASTER_CH_1, 0x72, 0, "SFP1"},
    {I2C_MASTER_CH_1, 0x72, 1, "SFP2"},
    /* BUS4 CPLD Access via SYSFS */
    {I2C_MASTER_CH_4, 0xFF, 0, "CPLD"},
    /* BUS5 CPLD_B */
    {I2C_MASTER_CH_5, 0xFF, 0, "CPLD_B"},
    /* BUS6 POWER CHIP Exported as virtual bus */
    {I2C_MASTER_CH_6, 0xFF, 0, "POWER"},
    /* BUS7 PSU */
    {I2C_MASTER_CH_7, 0xFF, 0, "PSU"},
    /* BUS8 FAN */
    /* Channel 2 is no hardware connected */
    {I2C_MASTER_CH_8, 0x77, 0, "FAN5"},
    {I2C_MASTER_CH_8, 0x77, 1, "FAN4"},
    {I2C_MASTER_CH_8, 0x77, 3, "FAN2"},
    {I2C_MASTER_CH_8, 0x77, 4, "FAN1"},
    /* BUS9 POWER MONITOR */
    {I2C_MASTER_CH_9, 0xFF, 0, "UCD90120"},
    /* BUS10 LM75 */
    {I2C_MASTER_CH_10, 0xFF, 0, "LM75"},
    /* BUS11 EEPROM */
    {I2C_MASTER_CH_11, 0xFF, 0, "EEPROM"},
#endif
};

#endif // END IF COMMON_HEAD