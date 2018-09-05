/*
*   timing_module.c -> FPGA timing module driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "timing_module.h"
#include "pcie_fpga.h"

#define PERIPHERALS_RESET_CONTROL_REGISTER  0x0034
#define INTERRUPT_SOURCE_STATUS_REGISTER    0x0044
#define INTERRUPT_FLAG_REGISTER             0x0048
#define INTERRUPT_MASK_REGISTER             0x004c
#define MISC_CONTROL_REGISTER               0x0050

#define SPI_DELAY_COUNT                     100
#define SPI_DELAY_TIME                      10000

#define SPI_STATUS_SOFT_CONTROL_REGISTER    0x133c
#define SPI_CS_CONTROL_REGISTER             0x1320
#define SPI_OP_CONTROL_REGISTER             0x1324
#define SPI_CLOCK_DIVIDER_CONTROL_REGISTER  0x1328
#define SPI_DEVICE_ADDRESS_BYTE0_REGISTER   0X132c
#define SPI_DEVICE_ADDRESS_BYTE1_REGISTER   0x1330
#define SPI_DEVICE_ADDRESS_BYTE2_REGISTER   0x1334
#define SPI_DEVICE_ADDRESS_BYTE3_REGISTER   0x1338
#define SPI_WRITE_DATA_BYTE_REGISTER        0x1340
#define SPI_WRITE_DATA_SIZE                 0x04
#define SPI_READ_DATA_BYTE_REGISTER         0x1350
#define SPI_READ_DATA_SIZE                  0x04

#define SPI_START_SHIFT (5)
#define SPI_START_MASK (0x1 << SPI_START_SHIFT)
#define SPI_BUS_BUSY_SHIFT (6)
#define SPI_BUS_BUSY_MASK (0x1 << SPI_BUS_BUSY_SHIFT)

#define SPI_BUSY_ERROR (-1)
#define SPI_TIMEOUT_ERROR (-2)
#define SPI_WR_RD_ERROR (-3)

struct timmod_data {
  uint16_t read_reg_addr;
  uint8_t read_length; //unit in byte
};

static struct timmod_data tim_data;

int read_spi(uint8_t devAddr, uint16_t reg, uint8_t *data, int len)
{
  uint32_t buffer;
  int delay = 0;

  while(ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER) & SPI_BUS_BUSY_MASK)
  {
    if(delay++ < SPI_DELAY_COUNT)
      udelay(SPI_DELAY_TIME);
    else
      return SPI_BUSY_ERROR;
  }

  //Set chip operation id
  iowrite32(devAddr, fpga_dev.data_base_addr + SPI_CS_CONTROL_REGISTER);

  //Set read/write operation
  iowrite32(0x03, fpga_dev.data_base_addr + SPI_OP_CONTROL_REGISTER);

  //Set data, address byte length 1, 2 8bits, 16bits
  buffer = ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);
  buffer = (buffer & 0xfffffff0) | ((0x01 << 2) | 0x02);
  iowrite32(buffer, fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);

  //Set register address which want to operation 
  iowrite32((reg & 0x00ff), fpga_dev.data_base_addr + SPI_DEVICE_ADDRESS_BYTE0_REGISTER);
  iowrite32((reg & 0xff00) >> 8, fpga_dev.data_base_addr + SPI_DEVICE_ADDRESS_BYTE1_REGISTER);

  //Set start bit
  buffer = ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);
  buffer = (buffer & 0xFFFFFFDF) | 0x01 << 5;
  iowrite32(buffer, fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);

  while(ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER) & SPI_BUS_BUSY_MASK)
  {
    if(delay++ < SPI_DELAY_COUNT)
      udelay(SPI_DELAY_TIME);
    else
      return SPI_BUSY_ERROR;
  }

  *data = (uint8_t)ioread32(fpga_dev.data_base_addr + SPI_READ_DATA_BYTE_REGISTER);

  return 0;
}

int write_spi(uint8_t devAddr, uint16_t reg, uint8_t data, uint8_t len)
{
  uint32_t buffer;
  int delay = 0;

  while(ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER) & SPI_BUS_BUSY_MASK)
  {
    if(delay++ < SPI_DELAY_COUNT)
      udelay(SPI_DELAY_TIME);
    else
      return SPI_BUSY_ERROR;
  }

  //Set chip operation id
  iowrite32(devAddr, fpga_dev.data_base_addr + SPI_CS_CONTROL_REGISTER);

  //Set read/write operation
  iowrite32(0x02, fpga_dev.data_base_addr + SPI_OP_CONTROL_REGISTER);

  //Set data, address byte length 1, 2 8bits, 16bits
  buffer = ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);
  buffer = (buffer & 0xfffffff0) | ((0x01 << 2) | 0x02);
  iowrite32(buffer, fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);

  //Set register address which want to operation 
  iowrite32((reg & 0x00ff), fpga_dev.data_base_addr + SPI_DEVICE_ADDRESS_BYTE0_REGISTER);
  iowrite32((reg & 0xff00) >> 8, fpga_dev.data_base_addr + SPI_DEVICE_ADDRESS_BYTE1_REGISTER);

  //Set data which want writing to register
  iowrite32((uint32_t)data, fpga_dev.data_base_addr + SPI_WRITE_DATA_BYTE_REGISTER);

  //Set start bit
  buffer = ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);
  buffer = (buffer & 0xFFFFFFDF) | 0x01 << 5;
  iowrite32(buffer, fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER);

  while(ioread32(fpga_dev.data_base_addr + SPI_STATUS_SOFT_CONTROL_REGISTER) & SPI_BUS_BUSY_MASK)
  {
    if(delay++ < SPI_DELAY_COUNT)
      udelay(SPI_DELAY_TIME);
    else
      return SPI_BUSY_ERROR;
  }

  return 0;
}

/* dpll get register value
 * Operation : R/W
 * reg_addr  : register address which want to read
 * Usage
 * $ echo reg_addr > dpll_getreg
 * $ cat dpll_getreg
 * ex
 * $ echo 0x0 > dpll_getreg
 * $ cat dpll_getreg
 */
static ssize_t dpll_getreg_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
  uint8_t data;

  mutex_lock(&fpga_data->fpga_lock);
  read_spi(1, tim_data.read_reg_addr, &data, tim_data.read_length);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%#2.2x\n",data);
}
static ssize_t dpll_getreg_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
  uint16_t reg_addr;
  char *last;

  reg_addr = (uint16_t)strtoul(buf, &last, 0);
  if(reg_addr == 0 && buf == last)
  {
    return -EINVAL;
  }
  tim_data.read_reg_addr = reg_addr;
  tim_data.read_length = 1;

  return count;
}
static DEVICE_ATTR_RW(dpll_getreg);

/* dpll set register value
 * Operation : WO
 * reg_addr  : register address which want to write
 * data      : data which want to write into register
 * Usage
 * $ echo reg_addr data > dpll_setreg
 * ex
 * $ echo 0x0 0x01 > dpll_setreg
 */
static ssize_t dpll_setreg_store(struct device *dev, struct device_attribute *devattr,
                            const char *buf, size_t count)
{
  uint16_t reg_addr;
  uint8_t data;
  char *tok;
  char *pclone = buf;
  int err;
  char *last;

  mutex_lock(&fpga_data->fpga_lock);
  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  reg_addr = (uint16_t)strtoul(tok, &last, 0);
  if(reg_addr == 0 && tok == last)
    return -EINVAL;
  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  data = (uint8_t)strtoul(tok, &last, 0);
  if(data == 0 && tok == last)
    return -EINVAL;

  write_spi(1, reg_addr, data, 1);
  mutex_unlock(&fpga_data->fpga_lock);

  return count;
}
static DEVICE_ATTR_WO(dpll_setreg);

/* DPLL_50M_CLK_SEL control
 * Operation : R/W
 * Status    : 1 local OSC
 *           : 0 DPLL
 * Usage
 * $ cat dpll_clk_sel
 * $ echo status > dpll_clk_sel
 * ex
 * $ cat dpll_clk_sel
 * $ echo 0 > dpll_clk_sel
 */
static ssize_t dpll_clk_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buffer;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + MISC_CONTROL_REGISTER);
  buffer = (buffer & 0x01);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n", buffer);
}
static ssize_t dpll_clk_sel_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t buffer;
  uint8_t data;
  char *last;

  data = (uint8_t)strtoul(buf, &last, 0);
  if(buf == last)
    return -EINVAL;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + MISC_CONTROL_REGISTER);
  buffer = (buffer & ~(1));
  buffer = (buffer | (data));

  iowrite32(buffer, fpga_dev.data_base_addr + MISC_CONTROL_REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);
  return size;
}
DEVICE_ATTR_RW(dpll_clk_sel);

/* Source Status of DPLL_INT_N
 * Operation : RO
 * Status    : 1 Status high
 *           : 0 Status low
 * Usage
 * $ cat dpll_intsta
 * ex
 * $ cat dpll_intsta
 */
static ssize_t dpll_intsta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buffer;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + INTERRUPT_SOURCE_STATUS_REGISTER);
  buffer = (buffer & 0x04) >> 2;
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n", buffer);
}
DEVICE_ATTR_RO(dpll_intsta);

/* Interrupt Status of DPLL_INT_N
 * Operation : RWC
 * Status    : 1 Interrupt
 *           : 0 Not interrupt
 * Usage
 * $ cat dpll_intirq
 * $ echo status > dpll_intirq
 * ex
 * $ cat dpll_intirq
 * $ echo 0 > dpll_intirq
 */
static ssize_t dpll_intirq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buffer;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + INTERRUPT_FLAG_REGISTER);
  buffer = (buffer & 0x04) >> 2;
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n", buffer);
}
static ssize_t dpll_intirq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t buffer;
  uint8_t data;
  char *last;

  data = (uint8_t)strtoul(buf, &last, 0);
  if(buf == last)
    return -EINVAL;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + INTERRUPT_FLAG_REGISTER);
  buffer = (buffer & ~(1 << 2));
  buffer = (buffer | (data << 2));

  iowrite32(buffer, fpga_dev.data_base_addr + INTERRUPT_FLAG_REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);
  return size;
}
DEVICE_ATTR_RW(dpll_intirq);

/* Interrupt mask of DPLL_INT_N
 * Operation : R/W
 * Status    : 1 Mask
 *           : 0 Not mask
 * Usage
 * $ cat dpll_intmsk
 * $ echo status > dpll_intmsk
 * ex
 * $ cat dpll_intmsk
 * $ echo 1 > dpll_intmsk
 */
static ssize_t dpll_intmsk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buffer;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + INTERRUPT_MASK_REGISTER);
  buffer = (buffer & 0x04) >> 2;
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n", buffer);
}
static ssize_t dpll_intmsk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t buffer;
  uint8_t data;
  char *last;

  data = (uint8_t)strtoul(buf, &last, 0);
  if(buf == last)
    return -EINVAL;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + INTERRUPT_MASK_REGISTER);
  buffer = (buffer & ~(1 << 2));
  buffer = (buffer | (data << 2));

  iowrite32(buffer, fpga_dev.data_base_addr + INTERRUPT_MASK_REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);
  return size;
}
DEVICE_ATTR_RW(dpll_intmsk);

/* Reset Timing Card DPLL
 * Operation : R/W
 * Status    : 1 Normal operation
 *           : 0 Reset
 * Usage
 * $ cat dpll_reset
 * $ echo status > dpll_reset
 * ex
 * $ cat dpll_reset
 * $ echo 1 > dpll_reset
 */
static ssize_t dpll_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buffer;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + PERIPHERALS_RESET_CONTROL_REGISTER);
  buffer = (buffer & 0x04) >> 2;
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n", buffer);
}
static ssize_t dpll_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t buffer;
  uint8_t data;
  char *last;

  data = (uint8_t)strtoul(buf, &last, 0);
  if(buf == last)
    return -EINVAL;

  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + PERIPHERALS_RESET_CONTROL_REGISTER);
  buffer = (buffer & ~(1 << 2));
  buffer = (buffer | (data << 2));

  iowrite32(buffer, fpga_dev.data_base_addr + PERIPHERALS_RESET_CONTROL_REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);
  return size;
}
DEVICE_ATTR_RW(dpll_reset);

static struct attribute *timmod_attrs[] = {
  &dev_attr_dpll_getreg.attr,
  &dev_attr_dpll_setreg.attr,
  &dev_attr_dpll_clk_sel.attr,
  &dev_attr_dpll_intsta.attr,
  &dev_attr_dpll_intirq.attr,
  &dev_attr_dpll_intmsk.attr,
  &dev_attr_dpll_reset.attr,
  NULL,
};

struct attribute_group timmod_attr_grp = {
    .attrs = timmod_attrs,
};
