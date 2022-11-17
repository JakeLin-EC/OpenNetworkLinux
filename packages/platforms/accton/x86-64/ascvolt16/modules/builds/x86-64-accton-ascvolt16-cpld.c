/*
 * A hwmon driver for the ascvolt16_cpld
 *
 * Copyright (C) 2021  Edgecore Networks Corporation.
 * Jake Lin <jake_lin@edge-core.com>
 *
 * Based on ad7414.c
 * Copyright 2006 Stefan Roese <sr at denx.de>, DENX Software Engineering
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/pci.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/list.h>

#define DRVNAME "ascvolt16_cpld"

static LIST_HEAD(cpld_client_list);
static struct mutex list_lock;

struct cpld_client_node {
	struct i2c_client *client;
	struct list_head   list;
};

enum cpld_type {
	ascvolt16_cpld
};

#define I2C_RW_RETRY_COUNT    10
#define I2C_RW_RETRY_INTERVAL 60 /* ms */
#define PON_PORT_NUM 16

static ssize_t show_status(struct device *dev, struct device_attribute *da,
			 char *buf);
static ssize_t show_gpon_type(struct device *dev, struct device_attribute *da,
			char *buf);
static ssize_t show_present_all(struct device *dev, struct device_attribute *da,
			 char *buf);
static ssize_t set_tx_disable(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t set_gpon_type(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t set_control(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t access(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t show_version(struct device *dev, struct device_attribute *da,
			char *buf);


struct ascvolt16_cpld_data {
	struct device *hwmon_dev;
	struct mutex   update_lock;
	u8  index; /* CPLD index */
};

/* Addresses scanned for ascvolt16_cpld
 */
static const unsigned short normal_i2c[] = { I2C_CLIENT_END };

#define TRANSCEIVER_PRESENT_ATTR_ID(index) MODULE_PRESENT_##index
#define TRANSCEIVER_RESET_ATTR_ID(index) MODULE_RESET_##index
#define TRANSCEIVER_LPMODE_ATTR_ID(index) MODULE_LPMODE_##index
#define TRANSCEIVER_INTERRUPT_ATTR_ID(index) MODULE_INTERRUPT_##index
#define TRANSCEIVER_TXDISABLE_ATTR_ID(index) MODULE_TXDISABLE_##index
#define TRANSCEIVER_TXFAULT_ATTR_ID(index) MODULE_TXFAULT_##index
#define TRANSCEIVER_RXLOS_ATTR_ID(index) MODULE_RXLOS_##index
#define TRANSCEIVER_GPONTYPE_ATTR_ID(index) MODULE_GPONTYPE_##index

enum ascvolt16_cpld_sysfs_attributes {
	/* transceiver attributes */
	TRANSCEIVER_PRESENT_ATTR_ID(1),
	TRANSCEIVER_PRESENT_ATTR_ID(2),
	TRANSCEIVER_PRESENT_ATTR_ID(3),
	TRANSCEIVER_PRESENT_ATTR_ID(4),
	TRANSCEIVER_PRESENT_ATTR_ID(5),
	TRANSCEIVER_PRESENT_ATTR_ID(6),
	TRANSCEIVER_PRESENT_ATTR_ID(7),
	TRANSCEIVER_PRESENT_ATTR_ID(8),
	TRANSCEIVER_PRESENT_ATTR_ID(9),
	TRANSCEIVER_PRESENT_ATTR_ID(10),
	TRANSCEIVER_PRESENT_ATTR_ID(11),
	TRANSCEIVER_PRESENT_ATTR_ID(12),
	TRANSCEIVER_PRESENT_ATTR_ID(13),
	TRANSCEIVER_PRESENT_ATTR_ID(14),
	TRANSCEIVER_PRESENT_ATTR_ID(15),
	TRANSCEIVER_PRESENT_ATTR_ID(16),
	TRANSCEIVER_PRESENT_ATTR_ID(17),
	TRANSCEIVER_PRESENT_ATTR_ID(18),
	TRANSCEIVER_PRESENT_ATTR_ID(19),
	TRANSCEIVER_PRESENT_ATTR_ID(20),
	TRANSCEIVER_PRESENT_ATTR_ID(21),
	TRANSCEIVER_PRESENT_ATTR_ID(22),
	TRANSCEIVER_PRESENT_ATTR_ID(23),
	TRANSCEIVER_PRESENT_ATTR_ID(24),
	TRANSCEIVER_PRESENT_ATTR_ID(25),
	TRANSCEIVER_PRESENT_ATTR_ID(26),

	TRANSCEIVER_RESET_ATTR_ID(1),
	TRANSCEIVER_RESET_ATTR_ID(2),

	TRANSCEIVER_LPMODE_ATTR_ID(1),
	TRANSCEIVER_LPMODE_ATTR_ID(2),

	TRANSCEIVER_INTERRUPT_ATTR_ID(1),
	TRANSCEIVER_INTERRUPT_ATTR_ID(2),

	TRANSCEIVER_TXDISABLE_ATTR_ID(3),
	TRANSCEIVER_TXDISABLE_ATTR_ID(4),
	TRANSCEIVER_TXDISABLE_ATTR_ID(5),
	TRANSCEIVER_TXDISABLE_ATTR_ID(6),
	TRANSCEIVER_TXDISABLE_ATTR_ID(7),
	TRANSCEIVER_TXDISABLE_ATTR_ID(8),
	TRANSCEIVER_TXDISABLE_ATTR_ID(9),
	TRANSCEIVER_TXDISABLE_ATTR_ID(10),
	TRANSCEIVER_TXDISABLE_ATTR_ID(11),
	TRANSCEIVER_TXDISABLE_ATTR_ID(12),
	TRANSCEIVER_TXDISABLE_ATTR_ID(13),
	TRANSCEIVER_TXDISABLE_ATTR_ID(14),
	TRANSCEIVER_TXDISABLE_ATTR_ID(15),
	TRANSCEIVER_TXDISABLE_ATTR_ID(16),
	TRANSCEIVER_TXDISABLE_ATTR_ID(17),
	TRANSCEIVER_TXDISABLE_ATTR_ID(18),
	TRANSCEIVER_TXDISABLE_ATTR_ID(19),
	TRANSCEIVER_TXDISABLE_ATTR_ID(20),
	TRANSCEIVER_TXDISABLE_ATTR_ID(21),
	TRANSCEIVER_TXDISABLE_ATTR_ID(22),
	TRANSCEIVER_TXDISABLE_ATTR_ID(23),
	TRANSCEIVER_TXDISABLE_ATTR_ID(24),
	TRANSCEIVER_TXDISABLE_ATTR_ID(25),
	TRANSCEIVER_TXDISABLE_ATTR_ID(26),

	TRANSCEIVER_TXFAULT_ATTR_ID(3),
	TRANSCEIVER_TXFAULT_ATTR_ID(4),
	TRANSCEIVER_TXFAULT_ATTR_ID(5),
	TRANSCEIVER_TXFAULT_ATTR_ID(6),
	TRANSCEIVER_TXFAULT_ATTR_ID(7),
	TRANSCEIVER_TXFAULT_ATTR_ID(8),
	TRANSCEIVER_TXFAULT_ATTR_ID(9),
	TRANSCEIVER_TXFAULT_ATTR_ID(10),
	TRANSCEIVER_TXFAULT_ATTR_ID(11),
	TRANSCEIVER_TXFAULT_ATTR_ID(12),
	TRANSCEIVER_TXFAULT_ATTR_ID(13),
	TRANSCEIVER_TXFAULT_ATTR_ID(14),
	TRANSCEIVER_TXFAULT_ATTR_ID(15),
	TRANSCEIVER_TXFAULT_ATTR_ID(16),
	TRANSCEIVER_TXFAULT_ATTR_ID(17),
	TRANSCEIVER_TXFAULT_ATTR_ID(18),
	TRANSCEIVER_TXFAULT_ATTR_ID(19),
	TRANSCEIVER_TXFAULT_ATTR_ID(20),
	TRANSCEIVER_TXFAULT_ATTR_ID(21),
	TRANSCEIVER_TXFAULT_ATTR_ID(22),
	TRANSCEIVER_TXFAULT_ATTR_ID(23),
	TRANSCEIVER_TXFAULT_ATTR_ID(24),
	TRANSCEIVER_TXFAULT_ATTR_ID(25),
	TRANSCEIVER_TXFAULT_ATTR_ID(26),

	TRANSCEIVER_RXLOS_ATTR_ID(3),
	TRANSCEIVER_RXLOS_ATTR_ID(4),
	TRANSCEIVER_RXLOS_ATTR_ID(5),
	TRANSCEIVER_RXLOS_ATTR_ID(6),
	TRANSCEIVER_RXLOS_ATTR_ID(7),
	TRANSCEIVER_RXLOS_ATTR_ID(8),
	TRANSCEIVER_RXLOS_ATTR_ID(9),
	TRANSCEIVER_RXLOS_ATTR_ID(10),

	TRANSCEIVER_GPONTYPE_ATTR_ID(11),
	TRANSCEIVER_GPONTYPE_ATTR_ID(12),
	TRANSCEIVER_GPONTYPE_ATTR_ID(13),
	TRANSCEIVER_GPONTYPE_ATTR_ID(14),
	TRANSCEIVER_GPONTYPE_ATTR_ID(15),
	TRANSCEIVER_GPONTYPE_ATTR_ID(16),
	TRANSCEIVER_GPONTYPE_ATTR_ID(17),
	TRANSCEIVER_GPONTYPE_ATTR_ID(18),
	TRANSCEIVER_GPONTYPE_ATTR_ID(19),
	TRANSCEIVER_GPONTYPE_ATTR_ID(20),
	TRANSCEIVER_GPONTYPE_ATTR_ID(21),
	TRANSCEIVER_GPONTYPE_ATTR_ID(22),
	TRANSCEIVER_GPONTYPE_ATTR_ID(23),
	TRANSCEIVER_GPONTYPE_ATTR_ID(24),
	TRANSCEIVER_GPONTYPE_ATTR_ID(25),
	TRANSCEIVER_GPONTYPE_ATTR_ID(26),

	MODULE_PRESENT_ALL,
	CPLD_VERSION,
	ACCESS,
	ASPEN1_RESET,
	ASPEN2_RESET,
	MAC_RESET,
};

/* sysfs attributes for hwmon
 */

/* qsfp transceiver attributes */
#define DECLARE_QSFP28_TRANSCEIVER_SENSOR_DEVICE_ATTR(index) \
	static SENSOR_DEVICE_ATTR(module_present_##index, S_IRUGO, show_status, \
								NULL, MODULE_PRESENT_##index); \
	static SENSOR_DEVICE_ATTR(module_reset_##index, S_IRUGO | S_IWUSR, \
								show_status, set_control, MODULE_RESET_##index); \
	static SENSOR_DEVICE_ATTR(module_lpmode_##index, S_IRUGO | S_IWUSR, \
								show_status, set_control, MODULE_LPMODE_##index); \
	static SENSOR_DEVICE_ATTR(module_interrupt_##index, S_IRUGO | S_IWUSR, \
								show_status, set_control, MODULE_INTERRUPT_##index)

#define DECLARE_QSFP28_TRANSCEIVER_ATTR(index)  \
	&sensor_dev_attr_module_present_##index.dev_attr.attr, \
	&sensor_dev_attr_module_reset_##index.dev_attr.attr, \
	&sensor_dev_attr_module_lpmode_##index.dev_attr.attr, \
	&sensor_dev_attr_module_interrupt_##index.dev_attr.attr

/* sfp transceiver attributes */
#define DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(index) \
	static SENSOR_DEVICE_ATTR(module_present_##index, S_IRUGO, show_status, \
								NULL, MODULE_PRESENT_##index); \
	static SENSOR_DEVICE_ATTR(module_tx_disable_##index, S_IRUGO | S_IWUSR, \
								show_status, set_tx_disable, \
								MODULE_TXDISABLE_##index); \
	static SENSOR_DEVICE_ATTR(module_tx_fault_##index, S_IRUGO, show_status, \
								NULL, MODULE_TXFAULT_##index); \
	static SENSOR_DEVICE_ATTR(module_rx_los_##index, S_IRUGO, show_status, \
								NULL, MODULE_RXLOS_##index)

#define DECLARE_SFP_TRANSCEIVER_ATTR(index) \
	&sensor_dev_attr_module_present_##index.dev_attr.attr, \
	&sensor_dev_attr_module_tx_disable_##index.dev_attr.attr, \
	&sensor_dev_attr_module_tx_fault_##index.dev_attr.attr, \
	&sensor_dev_attr_module_rx_los_##index.dev_attr.attr

/* gpon transceiver attributes */
#define DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(index) \
	static SENSOR_DEVICE_ATTR(module_present_##index, S_IRUGO, show_status, \
								NULL, MODULE_PRESENT_##index); \
	static SENSOR_DEVICE_ATTR(module_tx_disable_##index, S_IRUGO | S_IWUSR, \
								show_status, set_tx_disable, \
								MODULE_TXDISABLE_##index); \
	static SENSOR_DEVICE_ATTR(module_tx_fault_##index, S_IRUGO, show_status, \
								NULL, MODULE_TXFAULT_##index); \
	static SENSOR_DEVICE_ATTR(module_gpon_type_##index, S_IRUGO | S_IWUSR, \
								show_gpon_type, set_gpon_type, \
								MODULE_GPONTYPE_##index)

#define DECLARE_GPON_TRANSCEIVER_ATTR(index) \
	&sensor_dev_attr_module_present_##index.dev_attr.attr, \
	&sensor_dev_attr_module_tx_disable_##index.dev_attr.attr, \
	&sensor_dev_attr_module_tx_fault_##index.dev_attr.attr, \
	&sensor_dev_attr_module_gpon_type_##index.dev_attr.attr

static SENSOR_DEVICE_ATTR(version, S_IRUGO, show_version, NULL, CPLD_VERSION);
static SENSOR_DEVICE_ATTR(access, S_IWUSR, NULL, access, ACCESS);
static SENSOR_DEVICE_ATTR(aspen1_reset, S_IRUGO | S_IWUSR, show_status, set_control, ASPEN1_RESET);
static SENSOR_DEVICE_ATTR(aspen2_reset, S_IRUGO | S_IWUSR, show_status, set_control, ASPEN2_RESET);
static SENSOR_DEVICE_ATTR(mac_reset, S_IRUGO | S_IWUSR, show_status, set_control, MAC_RESET);
static SENSOR_DEVICE_ATTR(module_present_all, S_IRUGO, show_present_all, \
							NULL, MODULE_PRESENT_ALL);

/* transceiver attributes */
DECLARE_QSFP28_TRANSCEIVER_SENSOR_DEVICE_ATTR(1);
DECLARE_QSFP28_TRANSCEIVER_SENSOR_DEVICE_ATTR(2);

DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(3);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(4);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(5);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(6);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(7);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(8);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(9);
DECLARE_SFP_TRANSCEIVER_SENSOR_DEVICE_ATTR(10);

DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(11);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(12);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(13);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(14);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(15);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(16);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(17);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(18);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(19);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(20);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(21);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(22);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(23);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(24);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(25);
DECLARE_GPON_TRANSCEIVER_SENSOR_DEVICE_ATTR(26);

static struct attribute *ascvolt16_cpld_attributes[] = {
	/* transceiver attributes */
	DECLARE_QSFP28_TRANSCEIVER_ATTR(1),
	DECLARE_QSFP28_TRANSCEIVER_ATTR(2),

	DECLARE_SFP_TRANSCEIVER_ATTR(3),
	DECLARE_SFP_TRANSCEIVER_ATTR(4),
	DECLARE_SFP_TRANSCEIVER_ATTR(5),
	DECLARE_SFP_TRANSCEIVER_ATTR(6),
	DECLARE_SFP_TRANSCEIVER_ATTR(7),
	DECLARE_SFP_TRANSCEIVER_ATTR(8),
	DECLARE_SFP_TRANSCEIVER_ATTR(9),
	DECLARE_SFP_TRANSCEIVER_ATTR(10),

	DECLARE_GPON_TRANSCEIVER_ATTR(11),
	DECLARE_GPON_TRANSCEIVER_ATTR(12),
	DECLARE_GPON_TRANSCEIVER_ATTR(13),
	DECLARE_GPON_TRANSCEIVER_ATTR(14),
	DECLARE_GPON_TRANSCEIVER_ATTR(15),
	DECLARE_GPON_TRANSCEIVER_ATTR(16),
	DECLARE_GPON_TRANSCEIVER_ATTR(17),
	DECLARE_GPON_TRANSCEIVER_ATTR(18),
	DECLARE_GPON_TRANSCEIVER_ATTR(19),
	DECLARE_GPON_TRANSCEIVER_ATTR(20),
	DECLARE_GPON_TRANSCEIVER_ATTR(21),
	DECLARE_GPON_TRANSCEIVER_ATTR(22),
	DECLARE_GPON_TRANSCEIVER_ATTR(23),
	DECLARE_GPON_TRANSCEIVER_ATTR(24),
	DECLARE_GPON_TRANSCEIVER_ATTR(25),
	DECLARE_GPON_TRANSCEIVER_ATTR(26),

	&sensor_dev_attr_module_present_all.dev_attr.attr,
	&sensor_dev_attr_version.dev_attr.attr,
	&sensor_dev_attr_access.dev_attr.attr,
	&sensor_dev_attr_aspen1_reset.dev_attr.attr,
	&sensor_dev_attr_aspen2_reset.dev_attr.attr,
	&sensor_dev_attr_mac_reset.dev_attr.attr,
	NULL
};

static const struct attribute_group ascvolt16_cpld_group = {
	.attrs = ascvolt16_cpld_attributes,
};


static const struct attribute_group* cpld_groups[] = {
	&ascvolt16_cpld_group,
};

int ascvolt16_cpld_read(int bus_num, unsigned short cpld_addr, u8 reg)
{
	struct list_head   *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int ret = -EPERM;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node, list);

		if (cpld_node->client->addr == cpld_addr
			&& cpld_node->client->adapter->nr == bus_num) {
			ret = i2c_smbus_read_byte_data(cpld_node->client, reg);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}
EXPORT_SYMBOL(ascvolt16_cpld_read);

int ascvolt16_cpld_write(int bus_num, unsigned short cpld_addr, u8 reg, u8 value)
{
	struct list_head *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int ret = -EIO;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node, list);

		if (cpld_node->client->addr == cpld_addr
			&& cpld_node->client->adapter->nr == bus_num) {
			ret = i2c_smbus_write_byte_data(cpld_node->client, reg, value);
			break;
		}
	}

	mutex_unlock(&list_lock);

	return ret;
}
EXPORT_SYMBOL(ascvolt16_cpld_write);

static ssize_t show_status(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	int status = 0;
	u8 reg = 0, mask = 0, invert = 1;

	switch (attr->index) {
	case MODULE_PRESENT_1 ... MODULE_PRESENT_2:
		reg  = 0x4;
		mask = 0x1 << (attr->index - MODULE_PRESENT_1);
		break;
	case MODULE_PRESENT_3 ... MODULE_PRESENT_10:
		reg  = 0xD;
		mask = 0x1 << (attr->index - MODULE_PRESENT_3);
		break;
	case MODULE_PRESENT_11 ... MODULE_PRESENT_17:
		reg  = 0x39 + (attr->index - MODULE_PRESENT_11);
		mask = 0x8;
		break;
	case MODULE_PRESENT_18 ... MODULE_PRESENT_26:
		reg  = 0x41 + (attr->index - MODULE_PRESENT_18);;
		mask = 0x8;
		break;
	case MODULE_RESET_1 ... MODULE_RESET_2:
		reg  = 0x5;
		mask = 0x1 << (attr->index - MODULE_RESET_1);
		break;
	case MODULE_LPMODE_1 ... MODULE_LPMODE_2:
		reg  = 0x3;
		mask = 0x1 << (attr->index - MODULE_LPMODE_1);
		invert=0;
		break;
	case MODULE_INTERRUPT_1 ... MODULE_INTERRUPT_2:
		reg  = 0x7;
		mask = 0x1 << (attr->index - MODULE_INTERRUPT_1);
		break;
	case MODULE_TXDISABLE_3 ... MODULE_TXDISABLE_10:
		reg  = 0xE;
		mask = 0x1 << (attr->index - MODULE_TXDISABLE_3);
		invert=0;
		break;
	case MODULE_TXDISABLE_11 ... MODULE_TXDISABLE_17:
		reg  = 0x39 + (attr->index - MODULE_TXDISABLE_11);
		mask = 0x1;
		invert=0;
		break;
	case MODULE_TXDISABLE_18 ... MODULE_TXDISABLE_26:
		reg  = 0x41 + (attr->index - MODULE_TXDISABLE_18);;
		mask = 0x1;
		invert=0;
		break;
	case MODULE_TXFAULT_3 ... MODULE_TXFAULT_10:
		reg  = 0x11;
		mask = 0x1 << (attr->index - MODULE_TXDISABLE_3);
		invert=0;
		break;
	case MODULE_TXFAULT_11 ... MODULE_TXFAULT_17:
		reg  = 0x39 + (attr->index - MODULE_TXDISABLE_11);
		mask = 0x4;
		invert=0;
		break;
	case MODULE_TXFAULT_18 ... MODULE_TXFAULT_26:
		reg  = 0x41 + (attr->index - MODULE_TXDISABLE_18);;
		mask = 0x4;
		invert=0;
		break;
	case MODULE_RXLOS_3 ... MODULE_RXLOS_10:
		reg  = 0xC;
		mask = 0x1 << (attr->index - MODULE_RXLOS_3);
		invert=0;
		break;
	case ASPEN1_RESET ... ASPEN2_RESET:
		reg  = 0x32;
		mask = 0x80 >> (attr->index - ASPEN1_RESET);
		break;
	case MAC_RESET:
		reg  = 0x33;
		mask = 0x1;
		break;
 	default:
		return -ENXIO;
	}

	mutex_lock(&data->update_lock);
	switch(data->index) {
	/* Port 1-26 present status: read from i2c bus number '56'
		and CPLD slave address 0x60 */
	case ascvolt16_cpld:
		status = ascvolt16_cpld_read(56, 0x60, reg);
		break;
	default: status = -ENXIO;
		break;
	}

	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", invert? !(status & mask): !!(status & mask));

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t show_gpon_type(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	int status = 0;
	u8 reg = 0, mask = 0;

	switch (attr->index) {
	case MODULE_GPONTYPE_11 ... MODULE_GPONTYPE_21:
		reg  = 0x15 + (attr->index - MODULE_GPONTYPE_11);
		mask = 0x7;
		break;
	case MODULE_GPONTYPE_22 ... MODULE_GPONTYPE_26:
		reg  = 0x21 + (attr->index - MODULE_GPONTYPE_22);
		mask = 0x7;
		break;
 	default:
		return -ENXIO;
	}

	mutex_lock(&data->update_lock);
	switch(data->index) {
	case ascvolt16_cpld:
		status = ascvolt16_cpld_read(56, 0x60, reg);
		break;
	default:
		status = -ENXIO;
		break;
	}

	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%d\n", status & mask);

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t show_present_all(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	int i, status;
	u8 values[4]  = { 0 };
	u8 regs_cpld[] = { 0x04, 0x0D };
	u8 *regs[] = { regs_cpld };
	u8 size[] = { ARRAY_SIZE(regs_cpld) };
	u8 bus[] = { 56 };
	u8 addr[] = { 0x60 };
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->update_lock);

	for (i = 0; i < size[data->index]; i++) {
		status = ascvolt16_cpld_read(bus[data->index],
									addr[data->index], regs[data->index][i]);
		if (status < 0)
			goto exit;

		values[i] = ~(u8)status;
	}

	mutex_unlock(&data->update_lock);

	switch(data->index) {
	case ascvolt16_cpld:
		return sprintf(buf, "%.2x %.2x\n",
						values[0] & 0x3, values[1]);
	default:
		return -EINVAL;
	}

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t set_tx_disable(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	long disable;
	int status, bus, addr, val;
	u8 reg = 0, mask = 0;

	status = kstrtol(buf, 10, &disable);
	if (status)
		return status;

	
	switch (attr->index) {
	case MODULE_TXDISABLE_3 ... MODULE_TXDISABLE_10:
		reg  = 0xE;
		mask = 0x1 << (attr->index - MODULE_TXDISABLE_3);
		break;
	case MODULE_TXDISABLE_11 ... MODULE_TXDISABLE_17:
		reg  = 0x39 + (attr->index - MODULE_TXDISABLE_11);
		mask = 0x1;
		break;
	case MODULE_TXDISABLE_18 ... MODULE_TXDISABLE_26:
		reg  = 0x41 + (attr->index - MODULE_TXDISABLE_18);
		mask = 0x1;
		break;
	default:
		return 0;
	}
	mutex_lock(&data->update_lock);
	switch(data->index) {
	case ascvolt16_cpld:
		bus  = 56;
		addr = 0x60;
		break;
	default: status = -ENXIO;
		goto exit;
	}

	/* Read current status */
	val = ascvolt16_cpld_read(bus, addr, reg);
	if (unlikely(status < 0))
		goto exit;

	/* Update tx_disable status */
	if (disable)
		val |= mask;
	else
		val &= ~mask;

	status = ascvolt16_cpld_write(bus, addr, reg, val);
	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t set_control(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	long reset;
	int status, bus, addr;
	u8 reg = 0, mask = 0;

	status = kstrtol(buf, 10, &reset);
	if (status)
		return status;

	switch (attr->index) {
	case MODULE_RESET_1 ... MODULE_RESET_2:
		reg  = 0x5;
		mask = 0x1 << (attr->index - MODULE_RESET_1);
		break;
	case MODULE_LPMODE_1 ... MODULE_LPMODE_2:
		reg  = 0x3;
		mask = 0x1 << (attr->index - MODULE_LPMODE_1);
		break;
	case MODULE_INTERRUPT_1 ... MODULE_INTERRUPT_2:
		reg  = 0x7;
		mask = 0x1 << (attr->index - MODULE_INTERRUPT_1);
		break;
	case ASPEN1_RESET ... ASPEN2_RESET:
		/* Set SYS_RESET */
		reg  = 0x32;
		mask = 0x10 << (attr->index - ASPEN1_RESET);
		break;
	case MAC_RESET:
		reg  = 0x33;
		mask = 0x1;
		break;
	default:
		return -ENXIO;
	}

	mutex_lock(&data->update_lock);
	switch(data->index) {
	case ascvolt16_cpld:
		bus  = 56;
		addr = 0x60;
		break;
	default: status = -ENXIO;
		goto exit;
	}

	/* Read current status */
	status = ascvolt16_cpld_read(bus, addr, reg);
	if (unlikely(status < 0))
		goto exit;

	/* Update reset status */
	if (reset)
		status &= ~mask;
	else
		status |= mask;

	status = ascvolt16_cpld_write(bus, addr, reg, status);
	if (unlikely(status < 0))
		goto exit;

	if(attr->index == ASPEN1_RESET || attr->index == ASPEN2_RESET) {
		/* Set ASPEN_PCIE0 */
		reg  = 0x32;
		mask = 0x80 >> (attr->index - ASPEN1_RESET);
		
		/* Read current status */
		status = ascvolt16_cpld_read(bus, addr, reg);
		if (unlikely(status < 0))
			goto exit;

		/* Update reset status */
		if (reset)
			status &= ~mask;
		else
			status |= mask;

		/* wait 10 ms for SYS_RESET status */
		msleep(10);
		status = ascvolt16_cpld_write(bus, addr, reg, status);
		if (unlikely(status < 0))
			goto exit;
	}

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t set_gpon_type(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	long type;
	int status, bus, addr, val;
	u8 reg = 0, mask = 0;

	status = kstrtol(buf, 10, &type);
	if (status)
		return status;

	switch (attr->index) {
	case MODULE_GPONTYPE_11 ... MODULE_GPONTYPE_21:
		reg  = 0x15 + (attr->index - MODULE_GPONTYPE_11);
		mask = 0x7;
		break;
	case MODULE_GPONTYPE_22 ... MODULE_GPONTYPE_26:
		reg  = 0x21 + (attr->index - MODULE_GPONTYPE_22);
		mask = 0x7;
		break;
	default:
		return -ENXIO;
	}
	mutex_lock(&data->update_lock);

	switch(data->index) {
	case ascvolt16_cpld:
		bus  = 56;
		addr = 0x60;
		break;
	default: status = -ENXIO;
		goto exit;
	}

	/* Read current status */
	val = ascvolt16_cpld_read(bus, addr, reg);
	if (unlikely(status < 0))
		goto exit;

	val = (val & ~mask) | (type & mask);
	status = ascvolt16_cpld_write(bus, addr, reg, val);
	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static void ascvolt16_cpld_add_client(struct i2c_client *client)
{
	struct cpld_client_node *node = kzalloc(sizeof(struct cpld_client_node),
											GFP_KERNEL);

	if (!node) {
		dev_dbg(&client->dev, "Can't allocate cpld_client_node (0x%x)\n",
								client->addr);
		return;
	}

	node->client = client;

	mutex_lock(&list_lock);
	list_add(&node->list, &cpld_client_list);
	mutex_unlock(&list_lock);
}

static void ascvolt16_cpld_remove_client(struct i2c_client *client)
{
	struct list_head *list_node = NULL;
	struct cpld_client_node *cpld_node = NULL;
	int found = 0;

	mutex_lock(&list_lock);

	list_for_each(list_node, &cpld_client_list)
	{
		cpld_node = list_entry(list_node, struct cpld_client_node, list);

		if (cpld_node->client == client) {
			found = 1;
			break;
		}
	}

	if (found) {
		list_del(list_node);
		kfree(cpld_node);
	}

	mutex_unlock(&list_lock);
}

static ssize_t access(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	int status;
	u32 reg, val;
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);

	if (sscanf(buf, "0x%x 0x%x", &reg, &val) != 2)
		return -EINVAL;

	if (reg > 0xFF || val > 0xFF)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	switch(data->index) {
	case ascvolt16_cpld:
		status = ascvolt16_cpld_write(56, 0x60, reg, val);
		break;
	default: status = -ENXIO;
			break;
	}

	if (unlikely(status < 0))
		goto exit;

	mutex_unlock(&data->update_lock);
	return count;

exit:
	mutex_unlock(&data->update_lock);
	return status;
}

static ssize_t show_version(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);
	int status = 0;
	int reg1, reg2;

	mutex_lock(&data->update_lock);
	switch(data->index) {
	case ascvolt16_cpld:
		reg1 = ascvolt16_cpld_read(56, 0x60, 0x1);
		reg2 = ascvolt16_cpld_read(56, 0x60, 0x2);
		break;
	default: status = -1;
			break;
	}

	if (unlikely(status < 0)) {
		mutex_unlock(&data->update_lock);
		goto exit;
	}

	mutex_unlock(&data->update_lock);

	return sprintf(buf, "%02x.%02x\n", reg1, reg2);
exit:
	return status;
}


static int ascvolt16_cpld_probe(struct i2c_client *client,
			const struct i2c_device_id *dev_id)
{
	int status, reg;
	struct ascvolt16_cpld_data *data = NULL;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_dbg(&client->dev, "i2c_check_functionality failed (0x%x)\n",
								client->addr);
		status = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct ascvolt16_cpld_data), GFP_KERNEL);
	if (!data) {
		status = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->index = dev_id->driver_data;
	mutex_init(&data->update_lock);
	dev_info(&client->dev, "chip found\n");

	/* Register sysfs hooks */
	status = sysfs_create_group(&client->dev.kobj, cpld_groups[data->index]);
	if (status)
		goto exit_free;

	data->hwmon_dev = hwmon_device_register_with_info(&client->dev, DRVNAME, NULL, NULL, NULL);
	if (IS_ERR(data->hwmon_dev)) {
		status = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	ascvolt16_cpld_add_client(client);

	/* init pon port link led */
	mutex_lock(&data->update_lock);
	for (reg = 0x26; reg <= 0x2b; reg++) {
		status = ascvolt16_cpld_write(56, 0x60, reg, 0xff);
		if (unlikely(status < 0)) {
			goto exit_unlock;
		}
	}
	mutex_unlock(&data->update_lock);

	dev_info(&client->dev, "%s: cpld '%s'\n",
							dev_name(data->hwmon_dev), client->name);

	return 0;
exit_unlock:
	mutex_unlock(&data->update_lock);
exit_remove:
	sysfs_remove_group(&client->dev.kobj, cpld_groups[data->index]);
exit_free:
	kfree(data);
exit:

	return status;
}

static int ascvolt16_cpld_remove(struct i2c_client *client)
{
	struct ascvolt16_cpld_data *data = i2c_get_clientdata(client);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, cpld_groups[data->index]);
	kfree(data);
	ascvolt16_cpld_remove_client(client);

	return 0;
}

static const struct i2c_device_id ascvolt16_cpld_id[] = {
	{ "ascvolt16_cpld", ascvolt16_cpld },
	{}
};

MODULE_DEVICE_TABLE(i2c, ascvolt16_cpld_id);

static struct i2c_driver ascvolt16_cpld_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = DRVNAME,
	},
	.probe = ascvolt16_cpld_probe,
	.remove = ascvolt16_cpld_remove,
	.id_table = ascvolt16_cpld_id,
	.address_list = normal_i2c,
};

static int __init ascvolt16_cpld_init(void)
{
	mutex_init(&list_lock);
	return i2c_add_driver(&ascvolt16_cpld_driver);
}

static void __exit ascvolt16_cpld_exit(void)
{
	i2c_del_driver(&ascvolt16_cpld_driver);
}

module_init(ascvolt16_cpld_init);
module_exit(ascvolt16_cpld_exit);

MODULE_AUTHOR("Jake Lin <jake_lin@edge-core.com>");
MODULE_DESCRIPTION("ascvolt16_cpld driver");
MODULE_LICENSE("GPL");
