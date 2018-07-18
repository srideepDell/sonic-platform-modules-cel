/*
 * questone2_baseboard_cpld.c - driver for Questone2 Base Board CPLD
 * This driver implement sysfs for CPLD register access using LPC bus.
 * Copyright (C) 2017 Celestica Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

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
#include <linux/platform_device.h>
#include <linux/types.h>
#include <uapi/linux/stat.h>
#include <linux/string.h>

#define DRIVER_NAME "sys_cpld"
/**
 * CPLD register address for read and write.
 */
#define VERSION_ADDR 0xA100
#define SCRATCH_ADDR 0xA101

// For getting the sys_cpld kobj path.
extern struct kobject *sys_cpld;

struct baseboard_cpld_data {
    struct mutex       cpld_lock;
    uint16_t           read_addr;
};

struct baseboard_cpld_data *cpld_data;

/**
 * Read the value from scratch register as hex string.
 * @param  dev     kernel device
 * @param  devattr kernel device attribute
 * @param  buf     buffer for get value
 * @return         Hex string read from scratch register.
 */
static ssize_t scratch_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
    unsigned char data = 0;
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SCRATCH_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return sprintf(buf,"0x%2.2x\n", data);
}

/**
 * Set scratch register with specific hex string.
 * @param  dev     kernel device
 * @param  devattr kernel device attribute
 * @param  buf     buffer of set value
 * @param  count   number of bytes in buffer
 * @return         number of bytes written, or error code < 0.
 */
static ssize_t scratch_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    unsigned long data;
    int err;
    mutex_lock(&cpld_data->cpld_lock);

    err = kstrtoul(buf, 16, &data);
    if (err)
    {
        mutex_unlock(&cpld_data->cpld_lock);
        return err;
    }
    outb(data, SCRATCH_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_RW(scratch);


/* CPLD version attributes */
static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    // CPLD register is one byte
    mutex_lock(&cpld_data->cpld_lock);
    int len = sprintf(buf, "0x%2.2x\n",inb(VERSION_ADDR));
    mutex_unlock(&cpld_data->cpld_lock);
    return len;
}
static DEVICE_ATTR_RO(version);


static ssize_t dump_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    // CPLD register is one byte
    uint16_t addr;
    int err;
    addr = (uint16_t)strtoul(buf,NULL,16);
    if(err < 0){
        return 0;
    }
    cpld_data->read_addr = addr;
    return count;
}

static ssize_t dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    // CPLD register is one byte
    mutex_lock(&cpld_data->cpld_lock);
    int len = sprintf(buf, "0x%2.2x\n",inb(cpld_data->read_addr));
    mutex_unlock(&cpld_data->cpld_lock);
    return len;
}
static DEVICE_ATTR_RW(dump);

static ssize_t setreg_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    // CPLD register is one byte
    uint16_t addr;
    uint8_t value;
    char *tok;
    char clone[count];
    char *pclone = clone;
    int err;

    strcpy(clone, buf);

    mutex_lock(&cpld_data->cpld_lock);
    tok = strsep((char**)&pclone, " ");
    if(tok == NULL){
        mutex_unlock(&cpld_data->cpld_lock);
        return 0;
    }
    addr = (uint16_t)strtoul(tok,NULL,16);
    tok = strsep((char**)&pclone, " ");
    if(tok == NULL){
        mutex_unlock(&cpld_data->cpld_lock);
        return 0;
    }
    value = (uint8_t)strtoul(tok,NULL,16);
    outb(value,addr);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_WO(setreg);

static struct attribute *baseboard_cpld_attrs[] = {
    &dev_attr_version.attr,
    &dev_attr_scratch.attr,
    &dev_attr_dump.attr,
    &dev_attr_setreg.attr,
    NULL,
};

static struct attribute_group baseboard_cpld_attrs_grp = {
    .attrs = baseboard_cpld_attrs,
};

static struct resource baseboard_cpld_resources[] = {
    {
        .start  = 0xA100,
        .end    = 0xA1FF,
        .flags  = IORESOURCE_IO,
    },
};

static void baseboard_cpld_dev_release( struct device * dev)
{
    return;
}

static struct platform_device baseboard_cpld_dev = {
    .name           = DRIVER_NAME,
    .id             = -1,
    .num_resources  = ARRAY_SIZE(baseboard_cpld_resources),
    .resource       = baseboard_cpld_resources,
    .dev = {
        .release = baseboard_cpld_dev_release,
    }
};

static int baseboard_cpld_drv_probe(struct platform_device *pdev)
{
    struct resource *res;
    int ret =0;
    int portid_count;

    cpld_data = devm_kzalloc(&pdev->dev, sizeof(struct baseboard_cpld_data),
        GFP_KERNEL);
    if (!cpld_data)
        return -ENOMEM;

    mutex_init(&cpld_data->cpld_lock);

    cpld_data->read_addr = VERSION_ADDR;

    res = platform_get_resource(pdev, IORESOURCE_IO, 0);
    if (unlikely(!res)) {
        printk(KERN_ERR "Specified Resource Not Available...\n");
        return -1;
    }

    ret = sysfs_create_group(&pdev->dev.kobj, &baseboard_cpld_attrs_grp);
    if (ret) {
        printk(KERN_ERR "Cannot create sysfs for baseboard CPLD\n");
    }
    return 0;
}

static int baseboard_cpld_drv_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &baseboard_cpld_attrs_grp);
    return 0;
}

static struct platform_driver baseboard_cpld_drv = {
    .probe  = baseboard_cpld_drv_probe,
    .remove = __exit_p(baseboard_cpld_drv_remove),
    .driver = {
        .name   = DRIVER_NAME,
    },
};

int baseboard_cpld_init(void)
{
    // Register platform device and platform driver
    platform_device_register(&baseboard_cpld_dev);
    platform_driver_register(&baseboard_cpld_drv);
    return 0;
}

void baseboard_cpld_exit(void)
{
    // Unregister platform device and platform driver
    platform_driver_unregister(&baseboard_cpld_drv);
    platform_device_unregister(&baseboard_cpld_dev);
}

module_init(baseboard_cpld_init);
module_exit(baseboard_cpld_exit);

MODULE_AUTHOR("Prapatsorn W.  <pwisutti@celestica.com>");
MODULE_DESCRIPTION("Celestica Questone2 Baseboard LPC-CPLD Driver");
MODULE_LICENSE("GPL");
