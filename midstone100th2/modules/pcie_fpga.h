/*
*	Copyright (C) 2017 Celestica Corp.
*
*	Pariwat Leamsumran <pleamsum@celestica.com>
*	June 8, 2018
*/

#ifndef PCIE_FPGA_HEAD
#define PCIE_FPGA_HEAD

#include "common.h"

/*
========================================
Defined
========================================
*/
#define DEVICE_NAME "fwupgrade"
#define FPGA_PCI_NAME "midstone100th2_fpga_pci"
#define CLASS_NAME "midstone100th2_fpga"
#define FPGA_PCIE_DEVICE_ID 0x7021
#define TEST_PCIE_DEVICE_ID 0x1110

#ifndef PCI_VENDOR_ID_XILINX
#define PCI_VENDOR_ID_XILINX 0x10EE
#endif

#define FPGA_PCI_BAR_NUM 0

/*
========================================
FPGA PCIe BAR 0 Registers
========================================
*/
#define FPGA_SCRATCH 0x0004         //Dummy
#define FPGA_PORT_XCVR_READY 0x000c //Dummy
#define FPGA_VERSION 0x0000

/*
========================================
Function Defined
========================================
*/

/*
========================================
Structure Defined
========================================
*/
struct fpga_device
{
    /* data mmio region */
    void __iomem *data_base_addr;
    resource_size_t data_mmio_start;
    resource_size_t data_mmio_len;
};

enum
{
    READREG,
    WRITEREG
};

struct fpga_reg_data
{
    uint32_t addr;
    uint32_t value;
};

struct midstone100th2_fpga_data
{
    struct device *sff_devices[SFF_PORT_TOTAL];
    struct i2c_client *sff_i2c_clients[SFF_PORT_TOTAL];
    struct i2c_adapter *i2c_adapter[VIRTUAL_I2C_PORT_LENGTH];
    struct mutex fpga_lock; // For FPGA internal lock
    unsigned long fpga_read_addr;
    uint8_t cpld_b_read_addr;
    uint8_t cpld_b_read_length;
    uint8_t cpld1_read_addr;
    uint8_t cpld1_read_length;
    uint8_t cpld2_read_addr;
    uint8_t cpld2_read_length;
    uint8_t cpld3_read_addr;
    uint8_t cpld3_read_length;
    uint8_t cpld4_read_addr;
    uint8_t cpld4_read_length;
};

extern struct fpga_device fpga_dev;
extern struct pci_driver pci_dev_ops;
extern struct class *fpgafwclass;
extern struct device *fpgafwdev;
extern struct midstone100th2_fpga_data *fpga_data;
extern struct bin_attribute *fpga_bin_attrs[];
extern struct attribute *fpga_attrs[];
extern struct attribute_group fpga_attr_grp;

#endif // END IF PCIE_FPGA_HEAD
