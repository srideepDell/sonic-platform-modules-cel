/*
*   sff_led_test.c -> CPLD_1 driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "sff_led_test.h"
#include "midstone100th2_i2c_mapping.h"

/* SFF port led control mode
 * Operation : R/W
 * Mode      : normal
 *           : test
 * Usage
 * $ cat port_led_mode
 * $ echo mode > port_led_mode
 * ex
 * $ cat port_led_mode
 * $ echo test > port_led_mode
 */
static ssize_t port_led_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int err;
  uint8_t led_mode_1;
  uint8_t led_mode_2;
  uint8_t led_mode_3;
  uint8_t led_mode_4;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD1_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_1);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD2_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_2);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD3_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_3);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD4_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_4);
  if(err < 0)
    return err;

  return sprintf(buf, "%s %s\n",
                 led_mode_1 ? "test" : "normal",
                 led_mode_2 ? "test" : "normal",
                 led_mode_3 ? "test" : "normal",
                 led_mode_4 ? "test" : "normal");
}
static ssize_t port_led_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  int status;
  uint8_t led_mode_1;

  if(sysfs_streq(buf, "test"))
    led_mode_1 = 0x01;
  else if(sysfs_streq(buf, "normal"))
    led_mode_1 = 0x00;
  else
    return -EINVAL;

  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD1_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_1);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD2_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_1);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD3_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_1);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD4_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_mode_1);

  return size;
}
DEVICE_ATTR_RW(port_led_mode);

/* SFF port led color control
 * Operation : R/W
 * Status    : red   red led is on
 *           : green green led is on
 *           : blue  blue led is on
 * Usage
 * $ cat port_led_color
 * $ echo status > port_led_color
 * ex
 * $ cat port_led_color
 * $ echo red > port_led_color
 */
static ssize_t port_led_color_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int err;
  uint8_t led_color1;
  uint8_t led_color2;
  uint8_t led_color3;
  uint8_t led_color4;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD1_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color1);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD2_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color2);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD3_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color3);
  if(err < 0)
    return err;

  err = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD4_SLAVE_ADDR,0x00,I2C_SMBUS_READ,0x09,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color4);
  if(err < 0)
    return err;

  return sprintf(buf, "%s %s %s %s\n",
                 led_color1 ==0x01 ? "red" : led_color1 == 0x02 ? "green" : led_color1 == 0x03 ? "blue" : "off",
                 led_color2 ==0x01 ? "red" : led_color2 == 0x02 ? "green" : led_color2 == 0x03 ? "blue" : "off",
                 led_color3 ==0x01 ? "red" : led_color3 == 0x02 ? "green" : led_color3 == 0x03 ? "blue" : "off",
                 led_color4 ==0x01 ? "red" : led_color4 == 0x02 ? "green" : led_color4 == 0x03 ? "blue" : "off");
}
static ssize_t port_led_color_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  int status;
  uint8_t led_color;

  if(sysfs_streq(buf, "red"))
    led_color = 0x01;
  else if(sysfs_streq(buf, "green"))
    led_color = 0x02;
  else if(sysfs_streq(buf, "blue"))
    led_color = 0x03;
  else
  {
    status = -EINVAL;
    return status;
  }

  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD1_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x0A,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD2_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x0A,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD3_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x0A,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color);
  status = fpga_i2c_access(fpga_data->i2c_adapter[VIRTUAL_I2C_CPLD_INDEX],CPLD4_SLAVE_ADDR,0x00,
                           I2C_SMBUS_WRITE,0x0A,I2C_SMBUS_BYTE_DATA,(union i2c_smbus_data*)&led_color);

  return size;
}
DEVICE_ATTR_RW(port_led_color);

static struct attribute *sff_led_test[] = {
    &dev_attr_port_led_mode.attr,
    &dev_attr_port_led_color.attr,
    NULL,
};

struct attribute_group sff_led_test_grp = {
    .attrs = sff_led_test,
};
