/*
*   cpld_driver.c -> CPLD driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "common.h"
#include "cpld_driver.h"
#include "pcie_fpga.h"
#include "timing_module.h"
#include "cpld_b.h"
#include "cpld_1.h"
#include "cpld_2.h"
#include "cpld_3.h"
#include "cpld_4.h"
#include "panel_led.h"
#include "sff_led_test.h"
#include "midstone100th2_i2c_mapping.h"

static int midstone100th2_drv_remove(struct platform_device *pdev);
static int midstone100th2_drv_probe(struct platform_device *pdev);

struct device *sff_dev = NULL;

static struct resource midstone100th2_resources[] = {
    {
        .start = 0x10000000,
        .end = 0x10001000,
        .flags = IORESOURCE_MEM,
    },
};

static void midstone100th2_dev_release(struct device *dev)
{
  return;
}

struct platform_driver midstone100th2_drv = {
    .probe = midstone100th2_drv_probe,
    .remove = __exit_p(midstone100th2_drv_remove),
    .driver = {
        .name = DRIVER_NAME,
    },
};

struct platform_device midstone100th2_dev = {
    .name = DRIVER_NAME,
    .id = -1,
    .num_resources = ARRAY_SIZE(midstone100th2_resources),
    .resource = midstone100th2_resources,
    .dev = {
        .release = midstone100th2_dev_release,
    }
};

static int midstone100th2_drv_probe(struct platform_device *pdev)
{
  int ret;
  int portid_count;
  struct resource *res;

  BUG_ON(fpgafwclass == NULL);

  fpga_data = devm_kzalloc(&pdev->dev, sizeof(struct midstone100th2_fpga_data),
                           GFP_KERNEL);

  // Set default read address to VERSION
  fpga_data->fpga_read_addr = fpga_dev.data_base_addr + FPGA_VERSION;
  fpga_data->cpld_b_read_addr = 0x00;
  fpga_data->cpld1_read_addr = 0x00;
  fpga_data->cpld2_read_addr = 0x00;
  fpga_data->cpld3_read_addr = 0x00;
  fpga_data->cpld4_read_addr = 0x00;
  fpga_data->cpld_b_read_length = 0x77;
  fpga_data->cpld1_read_length = 0x20;
  fpga_data->cpld2_read_length = 0x20;
  fpga_data->cpld3_read_length = 0x20;
  fpga_data->cpld4_read_length = 0x20;

  mutex_init(&fpga_data->fpga_lock);
  for (ret = I2C_MASTER_CH_1; ret <= I2C_MASTER_CH_TOTAL; ret++)
  {
    mutex_init(&fpga_i2c_master_locks[ret - 1]);
  }

  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (unlikely(!res))
  {
    printk(KERN_ERR "Specified Resource Not Available...\n");
    kzfree(fpga_data);
    return -1;
  }

  fpga = kobject_create_and_add("fpga", &pdev->dev.kobj);
  if (!fpga)
  {
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(fpga, &fpga_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create FPGA sysfs attributes\n");
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  timmod = kobject_create_and_add("timmod", fpga);
  if (!timmod)
  {
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(timmod, &timmod_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create timmod sysfs attributes\n");
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  cpld_b = kobject_create_and_add("cpld_b", &pdev->dev.kobj);
  if (!cpld_b)
  {
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(cpld_b, &cpld_b_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create CPLD_B sysfs attributes\n");
    kobject_put(cpld_b);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  panel_led = kobject_create_and_add("panel_led", &pdev->dev.kobj);
  if (!panel_led)
  {
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(panel_led, &panel_led_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create panel_led sysfs attributes\n");
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  cpld1 = kobject_create_and_add("cpld1", &pdev->dev.kobj);
  if (!cpld1)
  {
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(cpld1, &cpld1_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create CPLD1 sysfs attributes\n");
    kobject_put(cpld1);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  cpld2 = kobject_create_and_add("cpld2", &pdev->dev.kobj);
  if (!cpld2)
  {
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(cpld2, &cpld2_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create CPLD2 sysfs attributes\n");
    kobject_put(cpld2);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  cpld3 = kobject_create_and_add("cpld3", &pdev->dev.kobj);
  if (!cpld3)
  {
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(cpld3, &cpld3_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create CPLD3 sysfs attributes\n");
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  cpld4 = kobject_create_and_add("cpld4", &pdev->dev.kobj);
  if (!cpld4)
  {
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return -ENOMEM;
  }
  ret = sysfs_create_group(cpld4, &cpld4_attr_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create CPLD4 sysfs attributes\n");
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(cpld3, &cpld3_attr_grp);
    kobject_put(cpld3);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  sff_dev = device_create(fpgafwclass, NULL, MKDEV(0, 0), NULL, "sff_device");
  if (IS_ERR(sff_dev))
  {
    printk(KERN_ERR "Failed to create sff device\n");
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(cpld3, &cpld3_attr_grp);
    kobject_put(cpld3);
    sysfs_remove_group(cpld4, &cpld4_attr_grp);
    kobject_put(cpld4);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return PTR_ERR(sff_dev);
  }
  ret = sysfs_create_group(&sff_dev->kobj, &sff_led_test_grp);
  if (ret != 0)
  {
    printk(KERN_ERR "Cannot create SFF attributes\n");
    device_destroy(fpgafwclass, MKDEV(0, 0));
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(cpld3, &cpld3_attr_grp);
    kobject_put(cpld3);
    sysfs_remove_group(cpld4, &cpld4_attr_grp);
    kobject_put(cpld4);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }
  ret = sysfs_create_link(&pdev->dev.kobj, &sff_dev->kobj, "sff");
  if (ret != 0)
  {
    sysfs_remove_group(&sff_dev->kobj, &sff_led_test_grp);
    device_destroy(fpgafwclass, MKDEV(0, 0));
    sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
    kobject_put(cpld_b);
    sysfs_remove_group(cpld1, &cpld1_attr_grp);
    kobject_put(cpld1);
    sysfs_remove_group(cpld2, &cpld2_attr_grp);
    kobject_put(cpld2);
    sysfs_remove_group(cpld3, &cpld3_attr_grp);
    kobject_put(cpld3);
    sysfs_remove_group(cpld4, &cpld4_attr_grp);
    kobject_put(cpld4);
    sysfs_remove_group(fpga, &fpga_attr_grp);
    kobject_put(fpga);
    kzfree(fpga_data);
    return ret;
  }

  for (portid_count = 0; portid_count < VIRTUAL_I2C_PORT_LENGTH; portid_count++)
  {
    fpga_data->i2c_adapter[portid_count] = midstone100th2_i2c_init(pdev, portid_count, VIRTUAL_I2C_BUS_OFFSET);
  }

  printk(KERN_INFO "Create SFF sysfs ...\n");

  /* Init SFF devices */
  for (portid_count = 0; portid_count < SFF_PORT_TOTAL; portid_count++)
  {
    struct i2c_adapter *i2c_adap = fpga_data->i2c_adapter[portid_count];
    if (i2c_adap)
    {
      fpga_data->sff_devices[portid_count] = midstone100th2_sff_init(portid_count);
      fpga_data->sff_i2c_clients[portid_count] = i2c_new_device(i2c_adap, &sff_eeprom_info);  
      sysfs_create_link(&fpga_data->sff_devices[portid_count]->kobj,
                        &fpga_data->sff_i2c_clients[portid_count]->dev.kobj,
                        "i2c");
    }
  }

  printk(KERN_INFO "Virtual I2C buses created\n");

  return 0;
};

static int midstone100th2_drv_remove(struct platform_device *pdev)
{
  int portid_count;
  struct sff_device_data *rem_data;

  for (portid_count = 0; portid_count < SFF_PORT_TOTAL; portid_count++)
  {
    sysfs_remove_link(&fpga_data->sff_devices[portid_count]->kobj, "i2c");
    i2c_unregister_device(fpga_data->sff_i2c_clients[portid_count]);
  }

  for (portid_count = 0; portid_count < VIRTUAL_I2C_PORT_LENGTH; portid_count++)
  {
    if (fpga_data->i2c_adapter[portid_count] != NULL)
    {
      info(KERN_INFO "<%x>", fpga_data->i2c_adapter[portid_count]);
      i2c_del_adapter(fpga_data->i2c_adapter[portid_count]);
    }
  }

  for (portid_count = 0; portid_count < SFF_PORT_TOTAL; portid_count++)
  {
    if (fpga_data->sff_devices[portid_count] != NULL)
    {
      rem_data = dev_get_drvdata(fpga_data->sff_devices[portid_count]);
      device_unregister(fpga_data->sff_devices[portid_count]);
      put_device(fpga_data->sff_devices[portid_count]);
      kfree(rem_data);
    }
  }
  sysfs_remove_group(fpga, &fpga_attr_grp);
  sysfs_remove_group(cpld_b, &cpld_b_attr_grp);
  sysfs_remove_group(panel_led, &panel_led_attr_grp);
  sysfs_remove_group(cpld1, &cpld1_attr_grp);
  sysfs_remove_group(cpld2, &cpld2_attr_grp);
  sysfs_remove_group(cpld3, &cpld3_attr_grp);
  sysfs_remove_group(cpld4, &cpld4_attr_grp);
  sysfs_remove_group(&sff_dev->kobj, &sff_led_test_grp);
  kobject_put(fpga);
  kobject_put(cpld_b);
  kobject_put(panel_led);
  kobject_put(cpld1);
  kobject_put(cpld2);
  kobject_put(cpld3);
  kobject_put(cpld4);
  device_destroy(fpgafwclass, MKDEV(0, 0));
  devm_kfree(&pdev->dev, fpga_data);

  return 0;
}
