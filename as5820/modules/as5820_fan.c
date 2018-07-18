/*
 * as5820_fan.c - driver for Questone2 as5820 fan control and monitor.
 * This driver implement hwmon for fan control using CPLDs`.
 * Copyright (C) 2018 Celestica Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/dmi.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mutex.h>

#define DRIVER_NAME "as5820_fan"
/**
 * CPLD register address for fan control and status.
 * There are 5 fans registers, fan3 is reserved.
 * 
 * Fan register offset by 4 bytes.
 */

#define NUM_FAN             5
#define FAN_BASE_PWM        0xA140
#define FAN_BASE_MISC       0xA141
#define FAN_BASE_RPM_REAR   0xA142
#define FAN_BASE_RPM_FRONT  0xA143

// MISC register bitfield
#define FAN_DIR_BIT  3
#define FAN_PRS_BIT  2
#define FAN_GRN_LED  1
#define FAN_RED_LED  0

struct mutex cpld_lock;

struct fan_cpld_data {
    struct platform_device *pdev;
    struct device *fan_hwmon;
};

static ssize_t show_speed(struct device *dev, struct device_attribute *da,
             char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    int rpm, rpm_reg = 0;

    rpm_reg = FAN_1_RPM_REAR + ( offset * 4 );
    mutex_lock(&cpld_lock);
    rpm = 150 * (unsigned char)inb(rpm_reg);
    mutex_unlock(&cpld_lock);

    return sprintf(buf, "%d\n", rpm);
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *da,
            const char *buf, size_t count)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    unsigned char pwm;
    int error;
    int pwm_reg = 0;

    pwm_reg = FAN_1_PWM + ( offset * 4 );
    error = kstrtou8(buf, 10, &pwm);
    if (error)
        return error;

    mutex_lock(&cpld_lock);
    outb(pwm, pwm_reg);
    mutex_unlock(&cpld_lock);
    return count;
}

static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_speed, NULL, 0);
static SENSOR_DEVICE_ATTR(pwm1, S_IWUSR, NULL ,set_pwm, 0);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO,show_speed, NULL, 1);
static SENSOR_DEVICE_ATTR(pwm2, S_IWUSR, NULL,set_pwm, 1);
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO,show_speed, NULL, 3);
static SENSOR_DEVICE_ATTR(pwm3, S_IWUSR, NULL ,set_pwm, 3);
static SENSOR_DEVICE_ATTR(fan4_input, S_IRUGO,show_speed, NULL, 4);
static SENSOR_DEVICE_ATTR(pwm4, S_IWUSR, NULL ,set_pwm, 4);

static struct attribute *fan_attrs[] = {
    &sensor_dev_attr_fan1_input.dev_attr.attr,
    &sensor_dev_attr_pwm1.dev_attr.attr,
    &sensor_dev_attr_fan2_input.dev_attr.attr,
    &sensor_dev_attr_pwm2.dev_attr.attr,
    &sensor_dev_attr_fan3_input.dev_attr.attr,
    &sensor_dev_attr_pwm3.dev_attr.attr,
    &sensor_dev_attr_fan4_input.dev_attr.attr,
    &sensor_dev_attr_pwm4.dev_attr.attr,
    NULL,
};

ATTRIBUTE_GROUPS(fan);

static struct resource fan_cpld_resources[] = {
    {
        .start  = 0xA140,
        .end    = 0xA153,
        .flags  = IORESOURCE_IO,
    },
};

static void fan_cpld_dev_release( struct device * dev)
{
    return;
}

static struct platform_device fan_cpld_dev = {
    .name           = DRIVER_NAME,
    .id             = -1,
    .num_resources  = ARRAY_SIZE(fan_cpld_resources),
    .resource       = fan_cpld_resources,
    .dev = {
        .release = fan_cpld_dev_release,
    }
};

static int fan_cpld_drv_probe(struct platform_device *pdev)
{
    struct resource *res;
    struct fan_cpld_data *data;

    data = devm_kzalloc(&pdev->dev, sizeof(struct fan_cpld_data),
        GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    platform_set_drvdata(pdev, data);
    data->pdev = pdev;

    mutex_init(&cpld_lock);

    res = platform_get_resource(pdev, IORESOURCE_IO, 0);
    if (unlikely(!res)) {
        printk(KERN_ERR "Specified Resource Not Available...\n");
        return -1;
    }

    data->fan_hwmon = devm_hwmon_device_register_with_groups(&pdev->dev, DRIVER_NAME, NULL, fan_groups);
    if( IS_ERR(data->fan_hwmon) ){
        printk(KERN_ERR "Error: canot craete fan hwmon device\n");
        return PTR_ERR(data->fan_hwmon);
    }
    return 0;
}

static int fan_cpld_drv_remove(struct platform_device *pdev)
{
    return 0;
}

static struct platform_driver fan_cpld_drv = {
    .probe  = fan_cpld_drv_probe,
    .remove = __exit_p(fan_cpld_drv_remove),
    .driver = {
        .name   = DRIVER_NAME,
    },
};

int fan_cpld_init(void)
{
    // Register platform device and platform driver
    platform_device_register(&fan_cpld_dev);
    platform_driver_register(&fan_cpld_drv);
    return 0;
}

void fan_cpld_exit(void)
{
    // Unregister platform device and platform driver
    platform_driver_unregister(&fan_cpld_drv);
    platform_device_unregister(&fan_cpld_dev);
}

module_init(fan_cpld_init);
module_exit(fan_cpld_exit);

MODULE_AUTHOR("Pradchaya P.  <pphuchar@celestica.com>");
MODULE_DESCRIPTION("Celestica as5820 fan hardware monitor");
MODULE_LICENSE("GPL");

