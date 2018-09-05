/*
*   sys_led.c -> system led driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "panel_led.h"
#include "pcie_fpga.h"
#include "midstone100th2_i2c_mapping.h"

#define SYSTEM_LED_CONTROL_REGISTER 0x62

/* sysled control
 * Operation : R/W
 * Status    : on
 *           : off
 *           : 1hz
 *           : 5hz
 * Usage
 * $ cat sys_led
 * $ echo status > sys_led
 * ex
 * $ cat sys_led
 * $ echo on > sys_led
 */
static ssize_t sys_led_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
  int err;
  uint8_t data;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_READ, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  data = data & 0x03;

  return sprintf(buf, "%s\n",
                 data == 0x03 ? "off" : data == 0x02 ? "5hz" : data ==0x01 ? "1hz": "on");
}
static ssize_t sys_led_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
  int err;
  uint8_t data;
  uint8_t led_status;

  if(sysfs_streq(buf, "off"))
    led_status = 0x03;
  else if(sysfs_streq(buf, "5hz"))
    led_status = 0x02;
  else if(sysfs_streq(buf, "1hz"))
    led_status = 0x01;
  else if(sysfs_streq(buf, "on"))
    led_status = 0x00;
  else
  {
    count = -EINVAL;
    return count;
  }

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_READ, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);

  if(err < 0)
    return err;

  data = data & ~(0x3);
  data = data | led_status;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_WRITE, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  return count;
}
static DEVICE_ATTR_RW(sys_led);

/* sysled color control
 * Operation : R/W
 * Status    : yellow
 *           : green
 *           : both
 *           : off
 * Usage
 * $ cat sys_led_color
 * $ echo status > sys_led_color
 * ex
 * $ cat sys_led_color
 * $ echo green > sys_led_color
 */
static ssize_t sys_led_color_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
  int err;
  uint8_t data;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_READ, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  data = (data >> 4) & 0x03;

  return sprintf(buf, "%s\n",
                 data == 0x03 ? "off" : data == 0x02 ? "yellow" : data ==0x01 ? "green": "both");
}
static ssize_t sys_led_color_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
  int err;
  uint8_t data;
  uint8_t led_status;

  if(sysfs_streq(buf, "off"))
    led_status = 0x03;
  else if(sysfs_streq(buf, "yellow"))
    led_status = 0x02;
  else if(sysfs_streq(buf, "green"))
    led_status = 0x01;
  else if(sysfs_streq(buf, "both"))
    led_status = 0x00;
  else
  {
    count = -EINVAL;
    return count;
  }

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_READ, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);

  if(err < 0)
    return err;

  data = data & ~(0x3 << 4);
  data = data | (led_status << 4);

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_B_INDEX], CPLD_B_SLAVE_ADDR, 0x00,
                        I2C_SMBUS_WRITE, SYSTEM_LED_CONTROL_REGISTER, I2C_SMBUS_BYTE_DATA,
                        (union i2c_smbus_data*)&data);
  if(err < 0)
    return err;

  return count;
}
static DEVICE_ATTR_RW(sys_led_color);

static struct attribute *sys_led_attrs[] = {
  &dev_attr_sys_led.attr,
  &dev_attr_sys_led_color.attr,
  NULL,

};

struct attribute_group panel_led_attr_grp = {
  .attrs = sys_led_attrs,
};
