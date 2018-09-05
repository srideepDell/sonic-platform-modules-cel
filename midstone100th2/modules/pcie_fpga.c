/*
*   pcie_fpga.c -> PCIe FPGA driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "pcie_fpga.h"

static int majorNumber;
struct class *fpgafwclass = NULL;
struct device *fpgafwdev = NULL; ///< The device-driver device struct pointerY

struct midstone100th2_fpga_data *fpga_data;

struct fpga_device fpga_dev = {
    .data_base_addr = NULL,
    .data_mmio_start = NULL,
    .data_mmio_len = NULL,
};

/* fpga get register value
 * Operation : R/W
 * reg_addr  : register address which want to read
 * Usage
 * $ echo reg_addr > getreg
 * $ cat getreg
 * ex
 * $ echo 0x4 > getreg
 * $ cat getreg
 */
static ssize_t get_fpga_reg_value(struct device *dev, struct device_attribute *devattr,
                                    char *buf)
{
  uint32_t data;

  data = ioread32(fpga_data->fpga_read_addr);

  return sprintf(buf, "0x%8.8x\n", data);
}
static ssize_t set_fpga_reg_address(struct device *dev, struct device_attribute *devattr,
                                    const char *buf, size_t count)
{
  uint32_t addr;
  char *last;

  addr = (uint32_t)strtoul(buf, &last, 16);
  if (addr == 0 && buf == last)
  {
    return -EINVAL;
  }
  fpga_data->fpga_read_addr = fpga_dev.data_base_addr + addr;

  return count;
}
DEVICE_ATTR(getreg, 0600, get_fpga_reg_value, set_fpga_reg_address);

/* fpga set register value
 * Operation : WO
 * reg_addr  : register address which want to write
 * data      : data which want to write into register
 * Usage
 * $ echo reg_addr data > setreg
 * ex
 * $ echo 0x4 0x77665544 > setreg
 */
static ssize_t set_fpga_reg_value(struct device *dev, struct device_attribute *devattr,
                                    const char *buf, size_t count)
{
  //register is 4 bytes
  uint32_t addr;
  uint32_t value;
  uint32_t mode = 8;
  char *tok;
  char clone[count];
  char *pclone = clone;
  int err;
  char *last;

  strcpy(clone, buf);

  mutex_lock(&fpga_data->fpga_lock);
  tok = strsep((char **)&pclone, " ");
  if (tok == NULL)
  {
    mutex_unlock(&fpga_data->fpga_lock);
    return -EINVAL;
  }
  addr = (uint32_t)strtoul(tok, &last, 16);
  if (addr == 0 && tok == last)
  {
    mutex_unlock(&fpga_data->fpga_lock);
    return -EINVAL;
  }
  tok = strsep((char **)&pclone, " ");
  if (tok == NULL)
  {
    mutex_unlock(&fpga_data->fpga_lock);
    return -EINVAL;
  }
  value = (uint32_t)strtoul(tok, &last, 16);
  if (value == 0 && tok == last)
  {
    mutex_unlock(&fpga_data->fpga_lock);
    return -EINVAL;
  }
  tok = strsep((char **)&pclone, " ");
  if (tok == NULL)
  {
    mode = 32;
  }
  else
  {
    mode = (uint32_t)strtoul(tok, &last, 10);
    if (mode == 0 && tok == last)
    {
      mutex_unlock(&fpga_data->fpga_lock);
      return -EINVAL;
    }
  }
  if (mode == 32)
  {
    iowrite32(value, fpga_dev.data_base_addr + addr);
  }
  else if (mode == 8)
  {
    iowrite8(value, fpga_dev.data_base_addr + addr);
  }
  else
  {
    mutex_unlock(&fpga_data->fpga_lock);
    return -EINVAL;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return count;
}
DEVICE_ATTR(setreg, 0200, NULL, set_fpga_reg_value);

/* fpga scratch register for test reading/writing
 * Operation : R/W
 * data      : data which want to write into register
 * Usage
 * $ echo data > scratch
 * $ cat scratch
 * ex
 * $ echo 0x77665544 > scratch
 * $ cat scratch
 */
static ssize_t get_fpga_scratch(struct device *dev, struct device_attribute *devattr,
                                char *buf)
{
  return sprintf(buf, "0x%8.8lx\n", ioread32(fpga_dev.data_base_addr + FPGA_SCRATCH) & 0xffffffff);
}
static ssize_t set_fpga_scratch(struct device *dev, struct device_attribute *devattr,
                                const char *buf, size_t count)
{
  uint32_t data;
  char *last;

  data = (uint32_t)strtoul(buf, &last, 16);
  if (data == 0 && buf == last)
  {
    return -EINVAL;
  }
  iowrite32(data, fpga_dev.data_base_addr + FPGA_SCRATCH);

  return count;
}
DEVICE_ATTR(scratch, 0600, get_fpga_scratch, set_fpga_scratch);

/* show FPGA port XCVR ready status
 * Operation : RO
 * Usage
 * $ cat ready
 * ex
 * $ cat ready
 */
static ssize_t ready_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = FPGA_PORT_XCVR_READY;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr + REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> 0) & 1U);
}
DEVICE_ATTR_RO(ready);

/* fpga version
 * Operation : RO
 * Usage
 * $ cat version
 * ex
 * $ cat version
 */
static ssize_t fpga_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t buff;

  mutex_lock(&fpga_data->fpga_lock);
  buff = ioread32(fpga_dev.data_base_addr);
  mutex_unlock(&fpga_data->fpga_lock);


  return sprintf(buf, "%8.8x\n", buff);
}
struct device_attribute dev_attr_version = __ATTR(version, 0600, fpga_version_show, NULL);

struct attribute *fpga_attrs[] = {
  &dev_attr_getreg.attr,
  &dev_attr_setreg.attr,
  &dev_attr_scratch.attr,
  &dev_attr_ready.attr,
  &dev_attr_version.attr,
  NULL,
};

/* read all XCVR register in binary mode
 * Operation : RO
 * Usage
 * $ hd dump
 * ex
 * $ hd dump
 */
static ssize_t dump_read(struct file *filp, struct kobject *kobj,
                            struct bin_attribute *attr, char *buf,
                            loff_t off, size_t count)
{
  unsigned long i = 0;
  ssize_t status;
  uint8_t read_reg;

  if (off + count > PORT_XCVR_REGISTER_SIZE)
  {
    return -EINVAL;
  }
  mutex_lock(&fpga_data->fpga_lock);
  while (i < count)
  {
    read_reg = ioread8(fpga_dev.data_base_addr + SFF_PORT_CTRL_BASE + off + i);
    buf[i++] = read_reg;
  }
  status = count;
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
BIN_ATTR_RO(dump, PORT_XCVR_REGISTER_SIZE);

struct bin_attribute *fpga_bin_attrs[] = {
    &bin_attr_dump,
    NULL,
};

struct attribute_group fpga_attr_grp = {
    .attrs = fpga_attrs,
    .bin_attrs = fpga_bin_attrs,
};

static long fpgafw_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int ret;
  struct fpga_reg_data data;
  mutex_lock(&fpga_data->fpga_lock);

/*#ifdef TEST_MODE
  static uint32_t status_reg;
#endif*/

  // Switch function to read and write.
  switch (cmd)
  {
    case READREG:
      if (copy_from_user(&data, (void __user *)arg, sizeof(data)) != 0)
      {
        mutex_unlock(&fpga_data->fpga_lock);
        return -EFAULT;
      }
      data.value = ioread32(fpga_dev.data_base_addr + data.addr);
      if (copy_to_user((void __user *)arg, &data, sizeof(data)) != 0)
      {
        mutex_unlock(&fpga_data->fpga_lock);
        return -EFAULT;
      }
/*#ifdef TEST_MODE
      if (data.addr == 0x1210)
      {
        switch (status_reg)
        {
          case 0x0000:
            status_reg = 0x8000;
            break;

          case 0x8080:
            status_reg = 0x80C0;
            break;

          case 0x80C0:
            status_reg = 0x80F0;
            break;

          case 0x80F0:
            status_reg = 0x80F8;
            break;
        }
        iowrite32(status_reg, fpga_dev.data_base_addr + 0x1210);
      }
#endif*/
      break;

    case WRITEREG:
      if (copy_from_user(&data, (void __user *)arg, sizeof(data)) != 0)
      {
        mutex_unlock(&fpga_data->fpga_lock);
        return -EFAULT;
      }
      //printk(KERN_INFO "WRITEREG: %x %x", data.addr, data.value);
      iowrite32(data.value, fpga_dev.data_base_addr + data.addr);

/*#ifdef TEST_MODE
      if (data.addr == 0x1204)
      {
        status_reg = 0x8080;
        iowrite32(status_reg, fpga_dev.data_base_addr + 0x1210);
      }
#endif*/
      break;

    default:
      mutex_unlock(&fpga_data->fpga_lock);
      return -EINVAL;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return ret;
}

static const struct file_operations fpgafw_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fpgafw_unlocked_ioctl,
};

static int fpgafw_init(void)
{
  printk(KERN_INFO "Initializing the switchboard driver\n");

  // Try to dynamically allocate a major number for the device -- more difficult but worth it
  majorNumber = register_chrdev(0, DEVICE_NAME, &fpgafw_fops);
  if (majorNumber < 0)
  {
    printk(KERN_ALERT "Failed to register a major number\n");
    return majorNumber;
  }

  printk(KERN_INFO "Device registered correctly with major number %d\n", majorNumber);

  // Register the device class
  fpgafwclass = class_create(THIS_MODULE, CLASS_NAME);
  if (IS_ERR(fpgafwclass))
  {
    // Check for error and clean up if there is
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Failed to register device class\n");
    return PTR_ERR(fpgafwclass); // Correct way to return an error on a pointer
  }

  printk(KERN_INFO "Device class registered correctly\n");

  // Register the device driver
  fpgafwdev = device_create(fpgafwclass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR(fpgafwdev))
  {
    // Clean up if there is an error
    class_destroy(fpgafwclass); // Repeated code but the alternative is goto statements
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_ALERT "Failed to create the FW upgrade device node\n");
    return PTR_ERR(fpgafwdev);
  }

  printk(KERN_INFO "FPGA fw upgrade device node created correctly\n"); // Made it! device was initialized

  return 0;
}

static void fpgafw_exit(void)
{
  device_destroy(fpgafwclass, MKDEV(majorNumber, 0)); // remove the device
  class_unregister(fpgafwclass);                      // unregister the device class
  class_destroy(fpgafwclass);                         // remove the device class
  unregister_chrdev(majorNumber, DEVICE_NAME);        // unregister the major number
  printk(KERN_INFO "Goodbye!\n");
}

static const struct pci_device_id fpga_id_table[] = {
  {  PCI_VDEVICE(XILINX, FPGA_PCIE_DEVICE_ID) },
  //{PCI_VDEVICE(REDHAT_QUMRANET, 0x1110)},
  //{PCI_VDEVICE(ARECA, TEST_PCIE_DEVICE_ID)},
  {0, }
};

static int fpga_drv_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
  int ret = 0;
  int err;
  int index;
  struct device *dev = &pdev->dev;

  if ((err = pci_enable_device(pdev)))
  {
    dev_err(dev, "pci_enable_device probe error %d for device %s\n",
            err, pci_name(pdev));
    return err;
  }
  if ((err = pci_request_regions(pdev, FPGA_PCI_NAME)) < 0)
  {
    dev_err(dev, "pci_request_regions error %d\n", err);
  }

  fpga_dev.data_mmio_start = pci_resource_start(pdev, FPGA_PCI_BAR_NUM);
  fpga_dev.data_mmio_len = pci_resource_len(pdev, FPGA_PCI_BAR_NUM);
  fpga_dev.data_base_addr = pci_iomap(pdev, FPGA_PCI_BAR_NUM, 0);
  if (!fpga_dev.data_base_addr)
  {
    dev_err(dev, "cann't iomap region of size %lu\n",
            (unsigned long)fpga_dev.data_mmio_len);
    goto pci_release;
  }
  dev_info(dev, "data_mmio iomap base = 0x%lx \n",
           (unsigned long)fpga_dev.data_base_addr);
  dev_info(dev, "data_mmio_start = 0x%lx data_mmio_len = %lu\n",
           (unsigned long)fpga_dev.data_mmio_start,
           (unsigned long)fpga_dev.data_mmio_len);

  printk(KERN_INFO "FPGA PCIe driver probe OK.\n");
  printk(KERN_INFO "FPGA ioremap registers of size %lu\n", (unsigned long)fpga_dev.data_mmio_len);
  printk(KERN_INFO "FPGA Virtual BAR %d at %8.8lx - %8.8lx\n", FPGA_PCI_BAR_NUM, fpga_dev.data_base_addr, fpga_dev.data_base_addr + fpga_dev.data_mmio_len);
  printk(KERN_INFO "");
  uint32_t buff = ioread32(fpga_dev.data_base_addr);
  printk(KERN_INFO "FPGA VERSION : %8.8x\n", buff);
  fpgafw_init();
  return 0;

reg_release:
  pci_iounmap(pdev, fpga_dev.data_base_addr);
pci_release:
  pci_release_regions(pdev);
pci_disable:
  pci_disable_device(pdev);

  return -EBUSY;
}

static void fpga_drv_remove(struct pci_dev *pdev)
{
  fpgafw_exit();
  pci_iounmap(pdev, fpga_dev.data_base_addr);
  pci_release_regions(pdev);
  pci_disable_device(pdev);
  printk(KERN_INFO "FPGA PCIe driver remove OK.\n");
};

struct pci_driver pci_dev_ops = {
  .name = FPGA_PCI_NAME,
  .probe = fpga_drv_probe,
  .remove = fpga_drv_remove,
  .id_table = fpga_id_table,
};
