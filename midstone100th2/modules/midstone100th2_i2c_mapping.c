/*
*   midstone100th2_i2c_mapping.c -> CPLD_1 driver for Midstone-100TH2.
*
*
*
*   Copyright (C) 2018 Celestica Corp.
*
*   Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "cpld_driver.h"
#include "midstone100th2_i2c_mapping.h"
#include "pcie_fpga.h"

/* PORT CTRL REGISTER
[31:7]  RSVD
[6]     LPMOD   6
[5]     RSVD
[4]     RST     4
[3:1]   RSVD
[0]     TXDIS   0
*/
#define CTRL_LPMOD   6
#define CTRL_RST     4
#define CTRL_TXDIS   0

/* PORT STATUS REGISTER
[31:6]  RSVD
[5]     IRQ         5
[4]     PRESENT     4
[3]     RSVD
[2]     TXFAULT     2
[1]     RXLOS       1
[0]     MODABS      0
*/
#define STAT_IRQ         5
#define STAT_PRESENT     4
#define STAT_TXFAULT     2
#define STAT_RXLOS       1
#define STAT_MODABS      0

/* PORT INTRPT REGISTER
[31:6]  RSVD
[5]     INT_N       5
[4]     PRESENT     4
[3]     RSVD
[2]     RSVD
[1]     RXLOS       1
[0]     MODABS      0
*/
#define INTR_INT_N      5
#define INTR_PRESENT    4
#define INTR_TXFAULT    2
#define INTR_RXLOS      1
#define INTR_MODABS     0

/* PORT INT MASK REGISTER
[31:6]  RSVD
[5]     INT_N       5
[4]     PRESENT     4
[3]     RSVD
[2]     RSVD
[1]     RXLOS_INT   1
[0]     MODABS      0
*/
#define MASK_INT_N      5
#define MASK_PRESENT    4
#define MASK_TXFAULT    2
#define MASK_RXLOS      1
#define MASK_MODABS     0

struct mutex fpga_i2c_master_locks[I2C_MASTER_CH_TOTAL];
uint16_t fpga_i2c_lasted_access_port[I2C_MASTER_CH_TOTAL];

static int i2c_wait_ack(struct i2c_adapter *a, unsigned timeout, int writing)
{
  int error = 0;
  unsigned tick = 0;
  int Status;

  struct i2c_dev_data *new_data = i2c_get_adapdata(a);
  void __iomem *pci_bar = fpga_dev.data_base_addr;

  unsigned int REG_FDR0;
  unsigned int REG_CR0;
  unsigned int REG_SR0;
  unsigned int REG_DR0;
  unsigned int REG_ID0;

  unsigned int master_bus = new_data->pca9548.master_bus;

  if (master_bus < I2C_MASTER_CH_1 || master_bus > I2C_MASTER_CH_TOTAL)
  {
    error = -ENXIO;
    return error;
  }

  REG_FDR0 = I2C_MASTER_FREQ_1 + (master_bus - 1) * 0x0100;
  REG_CR0 = I2C_MASTER_CTRL_1 + (master_bus - 1) * 0x0100;
  REG_SR0 = I2C_MASTER_STATUS_1 + (master_bus - 1) * 0x0100;
  REG_DR0 = I2C_MASTER_DATA_1 + (master_bus - 1) * 0x0100;
  REG_ID0 = I2C_MASTER_PORT_ID_1 + (master_bus - 1) * 0x0100;

  //check(pci_bar+REG_SR0);
  //check(pci_bar+REG_CR0);

  while (1)
  {
    Status = ioread8(pci_bar + REG_SR0);
    tick++;
    if (tick > timeout)
    {
      info("Status %2.2X", Status);
      info("Error Timeout");
      error = -ETIMEDOUT;
      break;
    }
    if (Status & (1 << I2C_SR_BIT_MIF))
      break;

    if (writing == 0 && (Status & (1 << I2C_SR_BIT_MCF)))
      break;
  }

  Status = ioread8(pci_bar + REG_SR0);
  iowrite8(0, pci_bar + REG_SR0);

  if (error < 0)
  {
    info("Status %2.2X", Status);
    return error;
  }

  if (!(Status & (1 << I2C_SR_BIT_MCF)))
  {
    info("Error Unfinish");
    return -EIO;
  }

  if (Status & (1 << I2C_SR_BIT_MAL))
  {
    info("Error MAL");
    return -EAGAIN;
  }

  if (Status & (1 << I2C_SR_BIT_RXAK))
  {
    info("SL No Acknowlege");
    if (writing)
    {
      info("Error No Acknowlege");
      iowrite8(1 << I2C_CR_BIT_MEN, pci_bar + REG_CR0);
      return -ENXIO;
    }
  }
  else
  {
    info("SL Acknowlege");
  }

  return 0;
}

static int smbus_access(struct i2c_adapter *adapter, u16 addr,
                        unsigned short flags, char rw, u8 cmd,
                        int size, union i2c_smbus_data *data)
{
  int error = 0;
  struct i2c_dev_data *dev_data;

  /* Write the command register */
  dev_data = i2c_get_adapdata(adapter);

  unsigned int portid = dev_data->portid;
  void __iomem *pci_bar = fpga_dev.data_base_addr;

#ifdef DEBUG_KERN
  printk(KERN_INFO "portid %2d|@ 0x%2.2X|f 0x%4.4X|(%d)%-5s| (%d)%-10s|CMD %2.2X ", portid, addr, flags, rw, rw == 1 ? "READ " : "WRITE", size, size == 0 ? "QUICK" : size == 1 ? "BYTE" : size == 2 ? "BYTE_DATA" : size == 3 ? "WORD_DATA" : size == 4 ? "PROC_CALL" : size == 5 ? "BLOCK_DATA" : "ERROR", cmd);
#endif

  /* Map the size to what the chip understands */
  switch (size)
  {
    case I2C_SMBUS_QUICK:
    case I2C_SMBUS_BYTE:
    case I2C_SMBUS_BYTE_DATA:
    case I2C_SMBUS_WORD_DATA:
    case I2C_SMBUS_BLOCK_DATA:
      break;
    default:
      printk(KERN_INFO "Unsupported transaction %d\n", size);
      error = -EOPNOTSUPP;
      goto Done;
  }

  unsigned int REG_FDR0;
  unsigned int REG_CR0;
  unsigned int REG_SR0;
  unsigned int REG_DR0;
  unsigned int REG_ID0;

  unsigned int master_bus = dev_data->pca9548.master_bus;

  if (master_bus < I2C_MASTER_CH_1 || master_bus > I2C_MASTER_CH_TOTAL)
  {
    error = -ENXIO;
    goto Done;
  }

  REG_FDR0 = I2C_MASTER_FREQ_1 + (master_bus - 1) * 0x0100;
  REG_CR0 = I2C_MASTER_CTRL_1 + (master_bus - 1) * 0x0100;
  REG_SR0 = I2C_MASTER_STATUS_1 + (master_bus - 1) * 0x0100;
  REG_DR0 = I2C_MASTER_DATA_1 + (master_bus - 1) * 0x0100;
  REG_ID0 = I2C_MASTER_PORT_ID_1 + (master_bus - 1) * 0x0100;

  iowrite8(portid, pci_bar + REG_ID0);

  int timeout = 0;
  int cnt = 0;

  ////[S][ADDR/R]
  //Clear status register
  iowrite8(0, pci_bar + REG_SR0);
  iowrite8(1 << I2C_CR_BIT_MIEN | 1 << I2C_CR_BIT_MTX | 1 << I2C_CR_BIT_MSTA, pci_bar + REG_CR0);
  SET_REG_BIT_H(pci_bar + REG_CR0, I2C_CR_BIT_MEN);

  if (rw == I2C_SMBUS_READ &&
        (size == I2C_SMBUS_QUICK || size == I2C_SMBUS_BYTE))
  {
    // sent device address with Read mode
    iowrite8(addr << 1 | 0x01, pci_bar + REG_DR0);
  }
  else
  {
    // sent device address with Write mode
    iowrite8(addr << 1 | 0x00, pci_bar + REG_DR0);
  }

  //info( "MS Start");

  //// Wait {A}
  error = i2c_wait_ack(adapter, 50000, 1);
  if (error < 0)
  {
    info("get error %d", error);
    goto Done;
  }

  //// [CMD]{A}
  if (size == I2C_SMBUS_BYTE_DATA ||
      size == I2C_SMBUS_WORD_DATA ||
      size == I2C_SMBUS_BLOCK_DATA ||
     (size == I2C_SMBUS_BYTE && rw == I2C_SMBUS_WRITE))
  {
    //sent command code to data register
    iowrite8(cmd, pci_bar + REG_DR0);
    info("MS Send CMD 0x%2.2X", cmd);

    // Wait {A}
    error = i2c_wait_ack(adapter, 50000, 1);
    if (error < 0)
    {
      info("get error %d", error);
      goto Done;
    }
  }

  switch (size)
  {
    case I2C_SMBUS_BYTE_DATA:
      cnt = 1;
      break;
    case I2C_SMBUS_WORD_DATA:
      cnt = 2;
      break;
    case I2C_SMBUS_BLOCK_DATA:
      // in block data mode keep number of byte in block[0]
      cnt = data->block[0];
      break;
    default:
      cnt = 0;
      break;
  }

  // [CNT]  used only bloack data write
  if (size == I2C_SMBUS_BLOCK_DATA && rw == I2C_SMBUS_WRITE)
  {
    iowrite8(cnt, pci_bar + REG_DR0);
    info("MS Send CNT 0x%2.2X", cnt);

    // Wait {A}
    error = i2c_wait_ack(adapter, 50000, 1);
    if (error < 0)
    {
      info("get error %d", error);
      goto Done;
    }
  }

  // [DATA]{A}
  if (rw == I2C_SMBUS_WRITE && (size == I2C_SMBUS_BYTE ||
                                size == I2C_SMBUS_BYTE_DATA ||
                                size == I2C_SMBUS_WORD_DATA ||
                                size == I2C_SMBUS_BLOCK_DATA))
  {
    int bid = 0;
    info("MS prepare to sent [%d bytes]", cnt);
    if (size == I2C_SMBUS_BLOCK_DATA)
    {
      bid = 1;  // block[0] is cnt;
      cnt += 1; // offset from block[0]
    }
    for (; bid < cnt; bid++)
    {
      iowrite8(data->block[bid], pci_bar + REG_DR0);
      info("   Data > %2.2X", data->block[bid]);
      // Wait {A}
      error = i2c_wait_ack(adapter, 50000, 1);
      if (error < 0)
      {
        goto Done;
      }
    }
  }

  //REPEATE START
  if (rw == I2C_SMBUS_READ && (size == I2C_SMBUS_BYTE_DATA ||
                               size == I2C_SMBUS_WORD_DATA ||
                               size == I2C_SMBUS_BLOCK_DATA))
  {
    info("MS Repeated Start");

    SET_REG_BIT_L(pci_bar + REG_CR0, I2C_CR_BIT_MEN);
    iowrite8(1 << I2C_CR_BIT_MIEN |
             1 << I2C_CR_BIT_MTX |
             1 << I2C_CR_BIT_MSTA |
             1 << I2C_CR_BIT_RSTA,
             pci_bar + REG_CR0);
    SET_REG_BIT_H(pci_bar + REG_CR0, I2C_CR_BIT_MEN);

    // sent Address with Read mode
    iowrite8(addr << 1 | 0x1, pci_bar + REG_DR0);

    // Wait {A}
    error = i2c_wait_ack(adapter, 50000, 1);
    if (error < 0)
    {
      goto Done;
    }
  }

  if (rw == I2C_SMBUS_READ && (size == I2C_SMBUS_BYTE ||
                               size == I2C_SMBUS_BYTE_DATA ||
                               size == I2C_SMBUS_WORD_DATA ||
                               size == I2C_SMBUS_BLOCK_DATA))
  {
    switch (size)
    {
      case I2C_SMBUS_BYTE:
      case I2C_SMBUS_BYTE_DATA:
        cnt = 1;
        break;
      case I2C_SMBUS_WORD_DATA:
        cnt = 2;
        break;
      case I2C_SMBUS_BLOCK_DATA:
        //will be changed after recived first data
        cnt = 3;
        break;
      default:
        cnt = 0;
        break;
    }

    int bid = 0;
    info("MS Receive");

    //set to Receive mode
    iowrite8(1 << I2C_CR_BIT_MEN |
             1 << I2C_CR_BIT_MIEN |
             1 << I2C_CR_BIT_MSTA,
             pci_bar + REG_CR0);

    for (bid = -1; bid < cnt; bid++)
    {
      // Wait {A}
      error = i2c_wait_ack(adapter, 50000, 0);
      if (error < 0)
      {
        goto Done;
      }
      if (bid == cnt - 2)
      {
        info("SET NAK");
        SET_REG_BIT_H(pci_bar + REG_CR0, I2C_CR_BIT_TXAK);
      }

      if (bid < 0)
      {
        ioread8(pci_bar + REG_DR0);
        info("READ Dummy Byte");
      }
      else
      {
        if (bid == cnt - 1)
        {
          info("SET STOP in read loop");
          SET_REG_BIT_L(pci_bar + REG_CR0, I2C_CR_BIT_MSTA);
        }
        data->block[bid] = ioread8(pci_bar + REG_DR0);

        info("DATA IN [%d] %2.2X", bid, data->block[bid]);

        if (size == I2C_SMBUS_BLOCK_DATA && bid == 0)
        {
          cnt = data->block[0] + 1;
        }
      }
    }
  }

Stop:
  //[P]
  SET_REG_BIT_L(pci_bar + REG_CR0, I2C_CR_BIT_MSTA);
  info("MS STOP");

Done:
  iowrite8(1 << I2C_CR_BIT_MEN, pci_bar + REG_CR0);
  //check(pci_bar+REG_CR0);
  //check(pci_bar+REG_SR0);
#ifdef DEBUG_KERN
  printk(KERN_INFO "END --- Error code  %d", error);
#endif

  return error;
}

/**
 * Wrapper of smbus_access access with PCA9548 I2C switch management.
 * This function set PCA9548 switches to the proper slave channel.
 * Only one channel among switches chip is selected during communication time.
 *
 * Note: If the bus does not have any PCA9548 on it, the switch_addr must be
 * set to 0xFF, it will use normal smbus_access function.
 */
int fpga_i2c_access(struct i2c_adapter *adapter, u16 addr,
                           unsigned short flags, char rw, u8 cmd,
                           int size, union i2c_smbus_data *data)
{
  int error = 0;
  struct i2c_dev_data *dev_data;
  dev_data = i2c_get_adapdata(adapter);
  unsigned char master_bus = dev_data->pca9548.master_bus;
  unsigned char switch_addr = dev_data->pca9548.switch_addr;
  unsigned char channel = dev_data->pca9548.channel;

  // Acquire the master resource.
  mutex_lock(&fpga_i2c_master_locks[master_bus - 1]);
  uint16_t prev_port = fpga_i2c_lasted_access_port[master_bus - 1];

  if (switch_addr != 0xFF)
  {
    // Check lasted access switch address on a master
    if ((unsigned char)(prev_port >> 8) == switch_addr)
    {
      // check if channel is the same
      if ((unsigned char)(prev_port & 0x00FF) != channel)
      {
        // set new PCA9548 at switch_addr to current
        error = smbus_access(adapter, switch_addr, flags, I2C_SMBUS_WRITE, 1 << channel, I2C_SMBUS_BYTE, NULL);
        // update lasted port
        fpga_i2c_lasted_access_port[master_bus - 1] = switch_addr << 8 | channel;
      }
    }
    else
    {
      // reset prev_port PCA9548 chip
      error = smbus_access(adapter, (u16)(prev_port >> 8), flags, I2C_SMBUS_WRITE, 0x00, I2C_SMBUS_BYTE, NULL);
      // set PCA9548 to current channel
      error = smbus_access(adapter, switch_addr, flags, I2C_SMBUS_WRITE, 1 << channel, I2C_SMBUS_BYTE, NULL);
      // update lasted port
      fpga_i2c_lasted_access_port[master_bus - 1] = switch_addr << 8 | channel;
    }
  }

  // Do SMBus communication
  error = smbus_access(adapter, addr, flags, rw, cmd, size, data);
  // reset the channel
  mutex_unlock(&fpga_i2c_master_locks[master_bus - 1]);

  return error;
}

/**
 * A callback function show available smbus functions.
 */
static u32 fpga_i2c_func(struct i2c_adapter *a)
{
    return I2C_FUNC_SMBUS_QUICK |
           I2C_FUNC_SMBUS_BYTE |
           I2C_FUNC_SMBUS_BYTE_DATA |
           I2C_FUNC_SMBUS_WORD_DATA |
           I2C_FUNC_SMBUS_BLOCK_DATA;
}

static const struct i2c_algorithm midstone100th2_i2c_algorithm = {
    .smbus_xfer = fpga_i2c_access,
    .functionality = fpga_i2c_func,
};

/* PRESENT. Applicable to QSFP type modules
 * Indicate the module is present or not
 * Operation : RO
 * Status    : 1 The module is not present
 *           : 0 The module is present
 * Usage
 * $ cat qsfp_modprssta
 * ex
 * $ cat qsfp_modprssta
 */
static ssize_t qsfp_modprssta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_STATUS_BASE + (portid-1)*0x10;
  
  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> STAT_PRESENT) & 1U);
}
DEVICE_ATTR_RO(qsfp_modprssta);

/* PRESENT signal Interrupt. Applicable to QSFP type modules
 * This bit is set when there is 1 to 0 or 0 to1 transition of PRSNT signal
 * Operation : RWC
 * Usage
 * $ cat qsfp_modprsirq
 * $ echo status > qsfp_modprsirq
 * ex
 * $ cat qsfp_modprsirq
 * $ echo 0 > qsfp_modprsirq
 */
static ssize_t qsfp_modprsirq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);
  return sprintf(buf, "%d\n", (data >> INTR_PRESENT) & 1U);
}
static ssize_t qsfp_modprsirq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << INTR_PRESENT);
    else
      data = data | ((u32)0x1 << INTR_PRESENT);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_modprsirq);

/* PRESENT interrupt mask control. Applicable to QSFP type modules
 * Operation : R/W
 * Status    : 1 Disable mask
 *           : 0 Enable mask
 * Usage
 * $ cat qsfp_modprsmsk
 * $ echo status > qsfp_modprsmsk
 * ex
 * $ cat qsfp_modprsmsk
 * $ echo 0 > qsfp_modprsmsk
 */
static ssize_t qsfp_modprsmsk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> MASK_PRESENT) & 1U);
}
static ssize_t qsfp_modprsmsk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << MASK_PRESENT);
    else
      data = data | ((u32)0x1 << MASK_PRESENT);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_modprsmsk);

/* IRQ. Applicable to QSFP type modules
 * Indicate the  interrupt from Module issue  is pending or not
 * Operation : R0
 * Status    : 1 No interrupt
 *           : 0 The module interrupt is pending
 * Usage
 * $ cat qsfp_intsta
 * ex
 * $ cat qsfp_intsta
 */
static ssize_t qsfp_intsta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> STAT_IRQ) & 1U);
}
DEVICE_ATTR_RO(qsfp_intsta);

/* INT_N signal Interrupt. Applicable to QSFP type modules
 * This bit is set when there is interrupt transition of INT_N signal of QSFP port from 1 to 0.
 * Operation : RWC
 * Usage
 * $ cat qsfp_intirq
 * $ echo status > qsfp_intirq
 * ex
 * $ cat qsfp_intirq
 * $ echo 0 > qsfp_intirq
 */
static ssize_t qsfp_intirq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> INTR_INT_N) & 1U);
}
static ssize_t qsfp_intirq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << INTR_INT_N);
    else
      data = data | ((u32)0x1 << INTR_INT_N);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_intirq);

/* INT_N interrupt mask control. Applicable to QSFP type modules
 * Operation : R/W
 * Status    : 1 Disable mask
 *           : 0 Enable mask
 * Usage
 * $ cat qsfp_intmsk
 * $ echo status > qsfp_intmsk
 * ex
 * $ cat qsfp_intmsk
 * $ echo 0 > qsfp_intmsk
 */
static ssize_t qsfp_intmsk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> MASK_INT_N) & 1U);
}
static ssize_t qsfp_intmsk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << MASK_INT_N);
    else
      data = data | ((u32)0x1 << MASK_INT_N);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_intmsk);

/* LPMOD. Applicable to QSFP type modules
 * Enables the Low power mode of the optical module.
 * Operation : R/W
 * Status    : 1 The module is in Low power mode
 *           : 0 The module is not in Low power mode
 * Usage
 * $ cat qsfp_lpmode
 * $ echo status > qsfp_lpmode
 * ex
 * $ cat qsfp_lpmode
 */
static ssize_t qsfp_lpmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> CTRL_LPMOD) & 1U);
}
static ssize_t qsfp_lpmode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << CTRL_LPMOD);
    else
      data = data | ((u32)0x1 << CTRL_LPMOD);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_lpmode);

/* RST. Applicable to QSFP type modules
 * Indicate the module is under Reset or not
 * Operation : R/W
 * Status    : 1 The module is not in Reset
 *           : 0 The module is in Reset
 * Usage
 * $ cat qsfp_reset
 * $ echo status > qsfp_reset
 * ex
 * $ cat qsfp_reset
 */
static ssize_t qsfp_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> CTRL_RST) & 1U);
}
static ssize_t qsfp_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << CTRL_RST);
    else
      data = data | ((u32)0x1 << CTRL_RST);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(qsfp_reset);

static struct attribute *qsfp_attrs[] = {
    &dev_attr_qsfp_modprssta.attr,
    &dev_attr_qsfp_modprsirq.attr,
    &dev_attr_qsfp_modprsmsk.attr,
    &dev_attr_qsfp_intsta.attr,
    &dev_attr_qsfp_intirq.attr,
    &dev_attr_qsfp_intmsk.attr,
    &dev_attr_qsfp_lpmode.attr,
    &dev_attr_qsfp_reset.attr,
    NULL,
};

static struct attribute_group qsfp_attr_grp = {
    .attrs = qsfp_attrs,
};

static const struct attribute_group *qsfp_attr_grps[] = {
    &qsfp_attr_grp,
    NULL};

/* Applicable to SFP type modules
 * Indicate optical module is present or not. 
 * Operation : R0
 * Status    : 1 The module is not present
 *           : 0 The module is present
 * Usage
 * $ cat sfp_modabssta
 * ex
 * $ cat sfp_modabssta
 */
static ssize_t sfp_modabssta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> STAT_MODABS) & 1U);
}
DEVICE_ATTR_RO(sfp_modabssta);

/* MODABS signal Interrupt. Applicable to SFP type modules
 * This bit is set when there is 1 to 0 or 0 to1 transition of RXLOS signal
 * Operation : RWC
 * Usage
 * $ cat sfp_modabsirq
 * $ echo status > sfp_modabsirq
 * ex
 * $ cat sfp_modabsirq
 * $ echo 0 > sfp_modabsirq
 */
static ssize_t sfp_modabsirq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> INTR_MODABS) & 1U);
}
static ssize_t sfp_modabsirq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << INTR_MODABS);
    else
      data = data | ((u32)0x1 << INTR_MODABS);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(sfp_modabsirq);

/* MODABS interrupt mask control. Applicable to SFP type modules
 * Operation : R/W
 * Status    : 1 Disable mask
 *           : 0 Enable mask
 * Usage
 * $ cat sfp_modabsmsk
 * $ echo status > sfp_modabsmsk
 * ex
 * $ cat sfp_modabsmsk
 * $ echo 0 > sfp_modabsmsk
 */
static ssize_t sfp_modabsmsk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> MASK_MODABS) & 1U);
}
static ssize_t sfp_modabsmsk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << MASK_MODABS);
    else
      data = data | ((u32)0x1 << MASK_MODABS);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(sfp_modabsmsk);

/* RXLOS. Applicable to SFP type modules
 * Indicate the Optical module receiver has asserted Loss of Signal or not
 * Operation : R0
 * Status    : 1 Loss of signal
 *           : 0 No loss of signal
 * Usage
 * $ cat sfp_modabssta
 * ex
 * $ cat sfp_modabssta
 */
static ssize_t sfp_rxlossta_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> STAT_RXLOS) & 1U);
}
DEVICE_ATTR_RO(sfp_rxlossta);

/* RXLOS signal Interrupt. Applicable to SFP type modules
 * This bit is set when there is 1 to 0 transition of RXLOS signal
 * Operation : RWC
 * Usage
 * $ cat sfp_rxlosirq
 * $ echo status > sfp_rxlosirq
 * ex
 * $ cat sfp_rxlosirq
 * $ echo 0 > sfp_rxlosirq
 */
static ssize_t sfp_rxlosirq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> INTR_RXLOS) & 1U);
}
static ssize_t sfp_rxlosirq_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << INTR_RXLOS);
    else
      data = data | ((u32)0x1 << INTR_RXLOS);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(sfp_rxlosirq);

/* RXLOS interrupt mask control. Applicable to SFP type modules
 * Operation : R/W
 * Status    : 1 Disable mask
 *           : 0 Enable mask
 * Usage
 * $ cat sfp_rxlosmsk
 * $ echo status > sfp_rxlosmsk
 * ex
 * $ cat sfp_rxlosmsk
 * $ echo 0 > sfp_rxlosmsk
 */
static ssize_t sfp_rxlosmsk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> MASK_RXLOS) & 1U);
}
static ssize_t sfp_rxlosmsk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_INT_MASK_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << MASK_RXLOS);
    else
      data = data | ((u32)0x1 << MASK_RXLOS);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(sfp_rxlosmsk);

/* TXFAULT. Applicable to SFP type modules
 * Indicate the Optical module receiver has asserted TX Fault
 * Operation : R0
 * Status    : 1 TX Fault
 *           : 0 No Tx Fault
 * Usage
 * $ cat sfp_txfault
 * ex
 * $ cat sfp_txfault
 */
static ssize_t sfp_txfault_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_STATUS_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> STAT_TXFAULT) & 1U);
}
DEVICE_ATTR_RO(sfp_txfault);

/* TX_DIS. Applicable to SFP type modules
 * Optical module transmitter output is turn off or not
 * Operation : R/W
 * Status    : 1 Optical module transmitter is turn OFF
 *           : 0 Optical module transmitter is turn ON
 * Usage
 * $ cat sfp_txdisable
 * $ echo status > sfp_txdisable
 * ex
 * $ cat sfp_txdisable
 * $ echo 0 > sfp_txdisable
 */
static ssize_t sfp_txdisable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  uint32_t data;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  data = ioread32(fpga_dev.data_base_addr+REGISTER);
  mutex_unlock(&fpga_data->fpga_lock);

  return sprintf(buf, "%d\n", (data >> CTRL_TXDIS) & 1U);
}
static ssize_t sfp_txdisable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
  uint32_t data;
  ssize_t status;
  long value;

  struct sff_device_data *dev_data = dev_get_drvdata(dev);
  unsigned int portid = dev_data->portid;
  unsigned int REGISTER = SFF_PORT_CTRL_BASE + (portid-1)*0x10;

  mutex_lock(&fpga_data->fpga_lock);
  status = kstrtol(buf, 0, &value);
  if (status == 0)
  {
    // check if value is 0 clear
    data = ioread32(fpga_dev.data_base_addr+REGISTER);
    if(!value)
      data = data & ~( (u32)0x1 << CTRL_TXDIS);
    else
      data = data | ((u32)0x1 << CTRL_TXDIS);

    iowrite32(data,fpga_dev.data_base_addr+REGISTER);
    status = size;
  }
  mutex_unlock(&fpga_data->fpga_lock);

  return status;
}
DEVICE_ATTR_RW(sfp_txdisable);

static struct attribute *sfp_attrs[] = {
    &dev_attr_sfp_modabssta.attr,
    &dev_attr_sfp_modabsirq.attr,
    &dev_attr_sfp_modabsmsk.attr,
    &dev_attr_sfp_rxlossta.attr,
    &dev_attr_sfp_rxlosirq.attr,
    &dev_attr_sfp_rxlosmsk.attr,
    &dev_attr_sfp_txfault.attr,
    &dev_attr_sfp_txdisable.attr,
    NULL,
};

static struct attribute_group sfp_attr_grp = {
    .attrs = sfp_attrs,
};

static const struct attribute_group *sfp_attr_grps[] = {
    &sfp_attr_grp,
    NULL};

/**
 * Create virtual I2C bus adapter for switch devices
 * @param  pdev             platform device pointer
 * @param  portid           virtual i2c port id for switch device mapping
 * @param  bus_number_offset bus offset for virtual i2c adapter in system
 * @return                  i2c adapter.
 *
 * When bus_number_offset is -1, created adapter with dynamic bus number.
 * Otherwise create adapter at i2c bus = bus_number_offset + portid.
 */

struct i2c_adapter *midstone100th2_i2c_init(struct platform_device *pdev, int portid, int bus_number_offset)
{
  int error;
  struct i2c_adapter *new_adapter;
  struct i2c_dev_data *new_data;

  new_adapter = kzalloc(sizeof(*new_adapter), GFP_KERNEL);
  if (!new_adapter)
  {
    printk(KERN_ALERT "Cannot alloc i2c adapter for %s", new_data->pca9548.calling_name);
    return NULL;
  }

  new_adapter->owner = THIS_MODULE;
  new_adapter->class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
  new_adapter->algo = &midstone100th2_i2c_algorithm;

  if (bus_number_offset == -1)
  {
    new_adapter->nr = -1;
  }
  else
  {
    new_adapter->nr = bus_number_offset + portid;
  }

  new_data = kzalloc(sizeof(*new_data), GFP_KERNEL);
  if (!new_data)
  {
    printk(KERN_ALERT "Cannot alloc i2c data for %s", new_data->pca9548.calling_name);
    kzfree(new_adapter);
    return NULL;
  }

  new_data->portid = portid;
  new_data->pca9548.master_bus = fpga_i2c_bus_dev[portid].master_bus;
  new_data->pca9548.switch_addr = fpga_i2c_bus_dev[portid].switch_addr;
  new_data->pca9548.channel = fpga_i2c_bus_dev[portid].channel;
  strcpy(new_data->pca9548.calling_name, fpga_i2c_bus_dev[portid].calling_name);

  snprintf(new_adapter->name, sizeof(new_adapter->name),
             "SMBus I2C Adapter PortID: %s", new_data->pca9548.calling_name);

  void __iomem *i2c_freq_base_reg = fpga_dev.data_base_addr + I2C_MASTER_FREQ_1;
  iowrite8(0x1F, i2c_freq_base_reg + (new_data->pca9548.master_bus - 1) * 0x100); // 0x1F 100kHz
  i2c_set_adapdata(new_adapter, new_data);
  error = i2c_add_numbered_adapter(new_adapter);
  if (error < 0)
  {
    printk(KERN_ALERT "Cannot add i2c adapter %s", new_data->pca9548.calling_name);
    kzfree(new_adapter);
    kzfree(new_data);
    return NULL;
  }

  return new_adapter;
}

struct device *midstone100th2_sff_init(int portid)
{
  struct sff_device_data *new_data;
  struct device *new_device;

  new_data = kzalloc(sizeof(*new_data), GFP_KERNEL);
  if (!new_data)
  {
    printk(KERN_ALERT "Cannot alloc sff device data @port%d", portid);
    return NULL;
  }
  /* The QSFP port ID start from 1  to 64
         SFP+ port ID start from 65 to 66*/
  new_data->portid = portid + 1;

  if(portid < VIRTUAL_I2C_QSFP_PORT)
    new_device = device_create_with_groups(fpgafwclass, sff_dev, MKDEV(0, 0), new_data, qsfp_attr_grps, "%s", fpga_i2c_bus_dev[portid].calling_name);
  else
    new_device = device_create_with_groups(fpgafwclass, sff_dev, MKDEV(0, 0), new_data, sfp_attr_grps, "%s", fpga_i2c_bus_dev[portid].calling_name);

  if (IS_ERR(new_device))
  {
    printk(KERN_ALERT "Cannot create sff device @port%d", portid);
    kfree(new_data);
    return NULL;
  }

  return new_device;
}
