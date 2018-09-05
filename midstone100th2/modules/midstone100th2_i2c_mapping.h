#ifndef MIDSTONE_I2C_MAP_HEAD
#define MIDSTONE_I2C_MAP_HEAD

#include "common.h"

#define GET_REG_BIT(REG, BIT) ((ioread8(REG) >> BIT) & 0x01)
#define SET_REG_BIT_H(REG, BIT) iowrite8(ioread8(REG) | (0x01 << BIT), REG)
#define SET_REG_BIT_L(REG, BIT) iowrite8(ioread8(REG) & ~(0x01 << BIT), REG)

struct i2c_dev_data
{
    int portid;
    struct i2c_switch pca9548;
};

static int smbus_access(struct i2c_adapter *adapter, u16 addr,
                        unsigned short flags, char rw, u8 cmd,
                        int size, union i2c_smbus_data *data);
extern int fpga_i2c_access(struct i2c_adapter *adapter, u16 addr,
                           unsigned short flags, char rw, u8 cmd,
                           int size, union i2c_smbus_data *data);

extern struct i2c_adapter *midstone100th2_i2c_init(struct platform_device *pdev, int portid, int bus_number_offset);
extern struct device *midstone100th2_sff_init(int portid);
#endif // END IF MIDSTONE_I2C_MAP_HEAD
