#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xe3ae0b4d, "module_layout" },
	{ 0x59e4b36a, "loongson_map_base" },
	{ 0xdb38c297, "up_read" },
	{ 0x7aa1756e, "kvfree" },
	{ 0xadaf718e, "down_read" },
	{ 0xb242b30, "follow_pfn" },
	{ 0x7c32d0f0, "printk" },
	{ 0xa14d0577, "get_user_pages_locked" },
	{ 0x54c99fac, "mem_section" },
	{ 0x4a0322fa, "cpu_data" },
	{ 0xc5bc25de, "kvmalloc_node" },
	{ 0x3002e03e, "find_vma" },
	{ 0x235a7fff, "__put_page" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0BFB7CA8EADEEFAA945D6EF");
