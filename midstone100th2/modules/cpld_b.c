/*
*   cpld_b.c -> CPLD_B driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "cpld_b.h"
#include "pcie_fpga.h"
#include "midstone100th2_i2c_mapping.h"

#define SLAVE_CPLD_RESET_REGISTER 0x0030

/* cpld_b dump register
 * Operation : R/W
 * reg_addr  : start register address
 * lenght    : read lenght
 * Usage
 * $ echo reg_addr lenght > dump
 * $ hd dump
 * ex
 * $ echo 0x0 12 > dump
 * $ hd dump
 */
static ssize_t cpld_b_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int index;
  uint8_t data[256]={0};

  for (index = 0 ; index < fpga_data->cpld2_read_length ; index++)
  {
    fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                    I2C_SMBUS_READ, fpga_data->cpld_b_read_addr + index, I2C_SMBUS_BYTE_DATA,
                    (union i2c_smbus_data*)&data[index]);
  }

  memcpy(buf, data, fpga_data->cpld_b_read_length);
  
  return fpga_data->cpld_b_read_length;
}
static ssize_t cpld_b_dump_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint8_t addr;
  uint8_t length;
  char *tok;
  char *pclone = buf;
  int err;
  char *last;

  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  addr = (uint8_t)strtoul(tok, &last, 0);
  if(addr == 0 && tok == last)
    return -EINVAL;
  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  length = (uint8_t)strtoul(tok, &last, 0);
  if(length == 0 && tok == last)
    return -EINVAL;

  fpga_data->cpld_b_read_addr = addr;
  fpga_data->cpld_b_read_length = length;

  return size;
}
struct device_attribute dev_attr_cpld_b_dump = __ATTR(dump, 0600, cpld_b_dump_show, cpld_b_dump_store);

/* cpld_b set register value
 * Operation : WO
 * reg_addr  : register address which want to write
 * data      : data which want to write into register
 * Usage
 * $ echo reg_addr data > setreg
 * ex
 * $ echo 0x01 0x77 > setreg
 */
static ssize_t cpld_b_setreg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  int err;
  uint8_t addr;
  uint8_t data;
  char *tok;
  char *pclone = buf;
  char *last;

  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  addr = (uint8_t)strtoul(tok, &last, 0);
  if(addr == 0 && tok == last)
    return -EINVAL;
  tok = strsep((char**)&pclone, " ");
  if(tok == NULL)
    return -EINVAL;
  data = (uint8_t)strtoul(tok, &last, 0);
  if(data == 0 && tok == last)
    return -EINVAL;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00, I2C_SMBUS_WRITE, addr, I2C_SMBUS_BYTE_DATA, (union i2c_smbus_data*)&data);
  if(err < 0)
    return sprintf(buf,"ERROR line %d",__LINE__);

  return size;
}
struct device_attribute dev_attr_cpld_b_setreg = __ATTR(setreg, 0200, NULL, cpld_b_setreg_store);

/* cpld_b scratch register for test reading/writing
 * Operation : R/W
 * data      : data which want to write into register
 * Usage
 * $ echo data > scratch
 * $ cat scratch
 * ex
 * $ echo 0x77 > scratch
 * $ cat scratch
 */
static ssize_t cpld_b_scratch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int err;
  uint8_t data;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX],CPLD_B_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x01,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  return sprintf(buf, "%#2.2x\n",data);
}
static ssize_t cpld_b_scratch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  int err;
  uint8_t data;
  char *last;

  data = (uint8_t)strtoul(buf,&last,16);
  if(data == 0 && buf == last)
  {
    return -EINVAL;
  }
  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX],CPLD_B_SLAVE_ADDR,0x00,I2C_SMBUS_WRITE,0x01,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  return size;
}
struct device_attribute dev_attr_cpld_b_scratch = __ATTR(scratch, 0600, cpld_b_scratch_show, cpld_b_scratch_store);

/* cpld_b version
 * Operation : RO
 * Usage
 * $ cat version
 * ex
 * $ cat version
 */
static ssize_t cpld_b_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int err;
  uint8_t data;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_READ, 0x00, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);

  return sprintf(buf, "%#x\n", data);
}
struct device_attribute dev_attr_cpld_b_version = __ATTR(version, 0600, cpld_b_version_show, NULL);

/* Software reset of Switch CPLD_B
 * Operation : R/W
 * Status    : 1 Normal operation
 *           : 0 Reset
 * Usage
 * $ echo status > reset
 * $ cat reset
 * ex
 * $ echo 0x01 > reset
 * $ cat reset
 */
static ssize_t cpld_b_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int err;
  uint32_t data;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr + SLAVE_CPLD_RESET_REGISTER);
  data = (data & 0x02) >> 1;
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%x\n",data);
}
static ssize_t cpld_b_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  int err;
  uint8_t data;
  uint32_t buffer;
  char *last;

  data = (uint8_t)strtoul(buf, &last, 0);
  if(data == 0 && buf == last)
  {
    return -EINVAL;
  }
  mutex_lock(&fpga_data->fpga_lock);
  buffer = ioread32(fpga_dev.data_base_addr + SLAVE_CPLD_RESET_REGISTER);
  buffer = (buffer & ~0x02) | data << 1;
  iowrite32(data, fpga_dev.data_base_addr + SLAVE_CPLD_RESET_REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return size;
}
struct device_attribute dev_attr_cpld_b_reset = __ATTR(reset, 0600, cpld_b_reset_show, cpld_b_reset_store);

static struct attribute *cpld_b_attrs[] = {
  &dev_attr_cpld_b_dump.attr,
  &dev_attr_cpld_b_setreg.attr,
  &dev_attr_cpld_b_scratch.attr,
  &dev_attr_cpld_b_version.attr,
  &dev_attr_cpld_b_reset.attr,
  NULL,
};

struct attribute_group cpld_b_attr_grp = {
  .attrs = cpld_b_attrs,
};
