#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x533a1566, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x959a84fd, __VMLINUX_SYMBOL_STR(i2c_del_driver) },
	{ 0x904d0224, __VMLINUX_SYMBOL_STR(i2c_register_driver) },
	{ 0x9e96f632, __VMLINUX_SYMBOL_STR(sysfs_create_bin_file) },
	{ 0x2e51823, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x675f140a, __VMLINUX_SYMBOL_STR(i2c_new_dummy) },
	{ 0xd0df2206, __VMLINUX_SYMBOL_STR(devm_kmalloc) },
	{ 0x9c88d05d, __VMLINUX_SYMBOL_STR(i2c_smbus_read_byte) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x3bd1b1f6, __VMLINUX_SYMBOL_STR(msecs_to_jiffies) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x7e149286, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x78794840, __VMLINUX_SYMBOL_STR(i2c_smbus_write_byte_data) },
	{ 0xbe2bdb48, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x3a0df096, __VMLINUX_SYMBOL_STR(sysfs_remove_bin_file) },
	{ 0xd1fea71f, __VMLINUX_SYMBOL_STR(i2c_unregister_device) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=i2c-core";

MODULE_ALIAS("i2c:24lc64t");
