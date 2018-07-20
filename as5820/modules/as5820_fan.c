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

#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#define DRIVER_NAME "as5820_fan"
/**
 * CPLD register address for fan control and status.
 * There are 5 fans registers, with fan3 reserved.
 * Only 4 fans in use in system.
 * Fan register offset by 4 bytes.
 */

enum {
    fan1 = 0,
    fan2 = 1,
    // none = 2, 
    fan3 = 3,
    fan4 = 4,
};

#define NUM_FAN             4
#define FAN_BASE_PWM        0xA140
#define FAN_BASE_MISC       0xA141
#define FAN_BASE_RPM_REAR   0xA142
#define FAN_BASE_RPM_FRONT  0xA143
#define FAN_WDT             0xA134

/** MISC register bitfield */
#define FAN_DIR_BIT  3 // RO
#define FAN_PRS_BIT  2 // RO
#define FAN_GRN_LED  1 // RW
#define FAN_RED_LED  0 // RW

/** device accesss lock */
struct mutex cpld_lock;

/**
 * led_param - LED access parameters:
 * @offset - offset for LED access in CPLD device
 * @mask - mask for LED access in CPLD device
 */
struct led_param {
    unsigned char offset;
    unsigned char mask;
};

/**
 * cpld_led - LED private data:
 * @cdev - LED class device instance
 * @param - LED CPLD access parameters
 */
struct cpld_led {
    struct led_classdev cdev;
    struct led_param param;
};

#define cdev_to_priv(c)     container_of(c, struct cpld_led, cdev)

struct fan_cpld_data {
    struct platform_device *pdev;
    struct device *fan_hwmon;
};

static ssize_t set_wdt(struct device *dev, struct device_attribute *da,
            const char *buf, size_t count)
{
    unsigned char wdt;
    int error;
    int wdt_reg = 0;

    wdt_reg = FAN_WDT;
    error = kstrtou8(buf, 0, &wdt);
    if (error)
        return error;

    mutex_lock(&cpld_lock);
    outb( wdt ? 1 : 0, wdt_reg );
    mutex_unlock(&cpld_lock);
    return count;
}

static ssize_t show_wdt(struct device *dev, struct device_attribute *da,
            char *buf)
{
    unsigned char wdt;
    int wdt_reg = 0;

    wdt_reg = FAN_WDT;
    mutex_lock(&cpld_lock);
    wdt = inb(wdt_reg);
    mutex_unlock(&cpld_lock);
    return sprintf(buf, "%d\n", wdt & 1u );
}

static ssize_t show_speed(struct device *dev, struct device_attribute *da,
             char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    int rpm, rpm_reg = 0;

    rpm_reg = FAN_BASE_RPM_REAR + ( offset * 4 );
    mutex_lock(&cpld_lock);
    rpm = 150 * (unsigned char)inb(rpm_reg);
    mutex_unlock(&cpld_lock);

    return sprintf(buf, "%d\n", rpm);
}

static ssize_t show_pwm(struct device *dev, struct device_attribute *da,
            char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    unsigned char pwm;
    int pwm_reg = 0;

    pwm_reg = FAN_BASE_PWM + ( offset * 4 );
    mutex_lock(&cpld_lock);
    pwm = inb(pwm_reg);
    mutex_unlock(&cpld_lock);
    return sprintf(buf, "%x\n", pwm);
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *da,
            const char *buf, size_t count)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    unsigned char pwm;
    int error;
    int pwm_reg = 0;

    pwm_reg = FAN_BASE_PWM + ( offset * 4 );
    error = kstrtou8(buf, 0, &pwm);
    if (error)
        return error;

    mutex_lock(&cpld_lock);
    outb(pwm, pwm_reg);
    mutex_unlock(&cpld_lock);
    return count;
}

static ssize_t show_prs(struct device *dev, struct device_attribute *da,
            char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    unsigned char prs;
    int prs_reg = 0;

    prs_reg = FAN_BASE_MISC + ( offset * 4 );
    mutex_lock(&cpld_lock);
    prs = inb(prs_reg);
    mutex_unlock(&cpld_lock);
    return sprintf(buf, "%d\n", (prs >> FAN_PRS_BIT) & 1u ? 0 : 1);
}

static ssize_t show_dir(struct device *dev, struct device_attribute *da,
            char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
    int offset = attr->index;
    unsigned char dir;
    int dir_reg = 0;

    dir_reg = FAN_BASE_MISC + ( offset * 4 );
    mutex_lock(&cpld_lock);
    dir = inb(dir_reg);
    mutex_unlock(&cpld_lock);
    return sprintf(buf, "%s\n", (dir >> FAN_DIR_BIT) & 1u ? "B2F" : "F2B");
}

enum led_brightness as5820_led_brightness_get(struct led_classdev *led_cdev)
{
    struct cpld_led *pled = cdev_to_priv(led_cdev);
    unsigned char led;
    int reg = 0;

    reg = FAN_BASE_MISC + ( pled->param.offset * 4 );
    mutex_lock(&cpld_lock);
    led = inb(reg);
    mutex_unlock(&cpld_lock);
    return ( (led >> pled->param.mask) & 1u ? LED_OFF : 1 );
}

static void as5820_led_brightness_set(struct led_classdev *led_cdev,
                                        enum led_brightness brightness)
{
    struct cpld_led *pled = cdev_to_priv(led_cdev);
    unsigned char led;
    int reg = 0;

    reg = FAN_BASE_MISC + ( pled->param.offset * 4 );

    mutex_lock(&cpld_lock);
    led = inb(reg);
    mutex_unlock(&cpld_lock);
    led &= ~(1 << pled->param.mask);
    if (!brightness)
            led |= (1 << pled->param.mask);

    mutex_lock(&cpld_lock);
    outb(led, reg);
    mutex_unlock(&cpld_lock);
}

/** hardware monitor attributes */
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_speed, NULL, fan1);
static SENSOR_DEVICE_ATTR(fan2_input, S_IRUGO, show_speed, NULL, fan2);
static SENSOR_DEVICE_ATTR(fan3_input, S_IRUGO, show_speed, NULL, fan3);
static SENSOR_DEVICE_ATTR(fan4_input, S_IRUGO, show_speed, NULL, fan4);
static SENSOR_DEVICE_ATTR(pwm1, S_IWUSR | S_IRUGO, show_pwm , set_pwm, fan1);
static SENSOR_DEVICE_ATTR(pwm2, S_IWUSR | S_IRUGO, show_pwm , set_pwm, fan2);
static SENSOR_DEVICE_ATTR(pwm3, S_IWUSR | S_IRUGO, show_pwm , set_pwm, fan3);
static SENSOR_DEVICE_ATTR(pwm4, S_IWUSR | S_IRUGO, show_pwm , set_pwm, fan4);
static SENSOR_DEVICE_ATTR(fan1_prs, S_IRUGO, show_prs , NULL, fan1);
static SENSOR_DEVICE_ATTR(fan2_prs, S_IRUGO, show_prs , NULL, fan2);
static SENSOR_DEVICE_ATTR(fan3_prs, S_IRUGO, show_prs , NULL, fan3);
static SENSOR_DEVICE_ATTR(fan4_prs, S_IRUGO, show_prs , NULL, fan4);
static SENSOR_DEVICE_ATTR(fan1_dir, S_IRUGO, show_dir , NULL, fan1);
static SENSOR_DEVICE_ATTR(fan2_dir, S_IRUGO, show_dir , NULL, fan2);
static SENSOR_DEVICE_ATTR(fan3_dir, S_IRUGO, show_dir , NULL, fan3);
static SENSOR_DEVICE_ATTR(fan4_dir, S_IRUGO, show_dir , NULL, fan4);
static DEVICE_ATTR(wdt_en, S_IWUSR | S_IRUGO, show_wdt , set_wdt);

static struct attribute *fan_attrs[] = {
    &sensor_dev_attr_fan1_input.dev_attr.attr,
    &sensor_dev_attr_fan2_input.dev_attr.attr,
    &sensor_dev_attr_fan3_input.dev_attr.attr,
    &sensor_dev_attr_fan4_input.dev_attr.attr,
    &sensor_dev_attr_pwm1.dev_attr.attr,
    &sensor_dev_attr_pwm2.dev_attr.attr,
    &sensor_dev_attr_pwm3.dev_attr.attr,
    &sensor_dev_attr_pwm4.dev_attr.attr,
    &sensor_dev_attr_fan1_prs.dev_attr.attr,
    &sensor_dev_attr_fan2_prs.dev_attr.attr,
    &sensor_dev_attr_fan3_prs.dev_attr.attr,
    &sensor_dev_attr_fan4_prs.dev_attr.attr,
    &sensor_dev_attr_fan1_dir.dev_attr.attr,
    &sensor_dev_attr_fan2_dir.dev_attr.attr,
    &sensor_dev_attr_fan3_dir.dev_attr.attr,
    &sensor_dev_attr_fan4_dir.dev_attr.attr,
    &dev_attr_wdt_en.attr,
    NULL,
};

ATTRIBUTE_GROUPS(fan);

/** fan LEDs devices */
static struct cpld_led fan_leds[] = {
    {
        .param = {
            .offset = fan1,
            .mask = FAN_GRN_LED
        },
        .cdev = {
            .name           = "as5820:fan1:green",
            .brightness     = 1,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    },
    {
        .param = {
            .offset = fan1,
            .mask = FAN_RED_LED
        },
        .cdev = {
            .name           = "as5820:fan1:red",
            .brightness     = LED_OFF,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    },    
    {
        .param = {
            .offset = fan2,
            .mask = FAN_GRN_LED
        },
        .cdev = {
            .name           = "as5820:fan2:green",
            .brightness     = 1,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    },
    {
        .param = {
            .offset = fan2,
            .mask = FAN_RED_LED
        },
        .cdev = {
            .name           = "as5820:fan2:red",
            .brightness     = LED_OFF,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    }, 
    {
        .param = {
            .offset = fan3,
            .mask = FAN_GRN_LED
        },
        .cdev = {
            .name           = "as5820:fan3:green",
            .brightness     = 1,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    },
    {
        .param = {
            .offset = fan3,
            .mask = FAN_RED_LED
        },
        .cdev = {
            .name           = "as5820:fan3:red",
            .brightness     = LED_OFF,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    }, 
    {
        .param = {
            .offset = fan4,
            .mask = FAN_GRN_LED
        },
        .cdev = {
            .name           = "as5820:fan4:green",
            .brightness     = 1,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    },
    {
        .param = {
            .offset = fan4,
            .mask = FAN_RED_LED
        },
        .cdev = {
            .name           = "as5820:fan4:red",
            .brightness     = LED_OFF,
            .max_brightness = 1,
            .brightness_get = as5820_led_brightness_get,
            .brightness_set = as5820_led_brightness_set,
            .flags          = LED_CORE_SUSPENDRESUME,
        }
    }    
};

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
    int i, err;

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
        printk(KERN_ERR "Error: canot create fan hwmon device\n");
        return PTR_ERR(data->fan_hwmon);
    }

    for (i = 0; i < ARRAY_SIZE(fan_leds); i++) {
        err = led_classdev_register(&pdev->dev, &fan_leds[i].cdev);
        if (err)
            goto err_free_leds;
    }

    return 0;


err_free_leds:
    for (i = 0; i < ARRAY_SIZE(fan_leds); i++) {
        led_classdev_unregister(&fan_leds[i].cdev);
    }
    return err;

}

static int fan_cpld_drv_remove(struct platform_device *pdev)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(fan_leds); i++) {
        led_classdev_unregister(&fan_leds[i].cdev);
    }
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
    platform_device_register(&fan_cpld_dev);
    platform_driver_register(&fan_cpld_drv);
    return 0;
}

void fan_cpld_exit(void)
{
    platform_driver_unregister(&fan_cpld_drv);
    platform_device_unregister(&fan_cpld_dev);
}

module_init(fan_cpld_init);
module_exit(fan_cpld_exit);

MODULE_AUTHOR("Pradchaya P.  <pphuchar@celestica.com>");
MODULE_DESCRIPTION("Celestica as5820 fan hardware monitor");
MODULE_LICENSE("GPL");
