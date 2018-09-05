/*
*	midstone_switchboard.c -> Driver for Midstone-100TH2 Switch board FPGA/CPLD.
*
*
*
*	Copyright (C) 2018 Celestica Corp.
*
*	Jakkapan Jangmuang <jjangmua@celestica.com>
*   Aug 01, 2018
*/

#include "pcie_fpga.h"
#include "cpld_driver.h"

/*******************************
 * Golbal Variable
 * ****************************/

/*******************************
 * Module Init
 * ****************************/

int midstone100th2_init(void)
{
    // pci register driver
    int rc;
    
    rc = pci_register_driver(&pci_dev_ops);

    if (rc)
        return rc;
    if (fpga_dev.data_base_addr == NULL)
    {
        printk(KERN_ALERT "FPGA PCIe device not found\n");
        pci_unregister_driver(&pci_dev_ops);
        return -ENODEV;
    }
    else
    {
        printk(KERN_INFO "FPGA DATA BASE not NULL\n");
    }

    platform_device_register(&midstone100th2_dev);
    platform_driver_register(&midstone100th2_drv);
    return 0;
}

void midstone100th2_exit(void)
{
    int rc;
    platform_driver_unregister(&midstone100th2_drv);
    platform_device_unregister(&midstone100th2_dev);
    pci_unregister_driver(&pci_dev_ops);

    printk(KERN_INFO "FPGA PCIe unload done.\n");
}

module_init(midstone100th2_init);
module_exit(midstone100th2_exit);

MODULE_AUTHOR("Jakkapan J. <jjangmua@celestica.com>");
MODULE_DESCRIPTION("Celestica Midstone-100TH2 platform driver");
MODULE_VERSION(MOD_VERSION);
MODULE_LICENSE("GPL");
