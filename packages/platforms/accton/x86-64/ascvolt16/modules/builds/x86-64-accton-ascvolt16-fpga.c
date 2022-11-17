/* INCLUDE FILE DECLARTIONS
 */
#include <linux/types.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define DRVNAME "ascvolt16_pci_fpga"

#define QSFPDD_NUM 2
#define QSFP28_NUM 8
#define PON_NUM 16

/* BAR0 Internal Register */
#define FPGA_REG_PON_ACT_LINK			0x0000
#define PON_ACT_LED_MASK(port)		(0x000000ff << ((port) * 8))
#define PON_LINK_LED_MASK(port)		(0x00ff0000 << ((port) * 8))
#define PON_ACT_LED_OFFSET(port)	((port) * 8)
#define PON_LINK_LED_OFFSET(port)	(((port) + 2) * 8)

static ssize_t show_pon_port_act_led(struct device *dev, struct device_attribute *da,
             char *buf);
static ssize_t set_pon_port_act_led(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);
static ssize_t show_pon_port_link_led(struct device *dev, struct device_attribute *da,
             char *buf);
static ssize_t set_pon_port_link_led(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count);

/* STATIC VARIABLE DEFINITIONS
 */
struct mutex pcie_lock;

struct ascvolt16_fpga_data {
	struct device *hwmon_dev;
	struct mutex   update_lock;
	u8  index; /* FPGA index */
};

struct fpga_device
{
	struct mutex driver_lock;
	char __iomem* hw_bar0;
	char __iomem* hw_bar1;
	char __iomem* hw_bar2;
	char __iomem* hw_bar3;
};

#define PON_PORT_ACT_LED_ATTR_ID(index)         PON_PORT_ACT_LED_##index
#define PON_PORT_LINK_LED_ATTR_ID(index)        PON_PORT_LINK_LED_##index

enum ascvolt16_fpga_sysfs_attributes {
	/* transceiver attributes */
	PON_PORT_ACT_LED_1,
	PON_PORT_ACT_LED_2,
	PON_PORT_LINK_LED_1,
	PON_PORT_LINK_LED_2,
};

/* gpon transceiver attributes */
#define DECLARE_GPON_PORT_LED_SENSOR_DEVICE_ATTR(index) \
    static SENSOR_DEVICE_ATTR(pon_port_act_led_##index, S_IRUGO | S_IWUSR, show_pon_port_act_led, set_pon_port_act_led, PON_PORT_ACT_LED_##index); \
    static SENSOR_DEVICE_ATTR(pon_port_act_led_##index, S_IRUGO | S_IWUSR, show_pon_port_act_led, set_pon_port_act_led, PON_PORT_ACT_LED_##index); \

#define DECLARE_GPON_PORT_LED_ATTR(index)  \
    &sensor_dev_attr_pon_port_act_led_##index.dev_attr.attr, \
    &sensor_dev_attr_pon_port_link_led_##index.dev_attr.attr

/* transceiver attributes */
static SENSOR_DEVICE_ATTR(pon_port_act_led_1, S_IRUGO | S_IWUSR, show_pon_port_act_led, set_pon_port_act_led, PON_PORT_ACT_LED_1);
static SENSOR_DEVICE_ATTR(pon_port_act_led_2, S_IRUGO | S_IWUSR, show_pon_port_act_led, set_pon_port_act_led, PON_PORT_ACT_LED_2);
static SENSOR_DEVICE_ATTR(pon_port_link_led_1, S_IRUGO | S_IWUSR, show_pon_port_link_led, set_pon_port_link_led, PON_PORT_LINK_LED_1);
static SENSOR_DEVICE_ATTR(pon_port_link_led_2, S_IRUGO | S_IWUSR, show_pon_port_link_led, set_pon_port_link_led, PON_PORT_LINK_LED_2);

static struct attribute* ascvolt16_fpga_attributes[] =
{
	&sensor_dev_attr_pon_port_act_led_1.dev_attr.attr,
	&sensor_dev_attr_pon_port_act_led_2.dev_attr.attr,
	&sensor_dev_attr_pon_port_link_led_1.dev_attr.attr,
	&sensor_dev_attr_pon_port_link_led_2.dev_attr.attr,
	NULL,
};

static const struct attribute_group ascvolt16_fpga_group = {
	.attrs = ascvolt16_fpga_attributes,
};

static const struct attribute_group *fpga_groups[] = {
	&ascvolt16_fpga_group,
};

static u32 ascvolt16_fpga_read32(void *addr)
{
	u32 value;

	mutex_lock(&pcie_lock);
	value = ioread32(addr);
	mutex_unlock(&pcie_lock);

	return le32_to_cpu(value);
}

static void ascvolt16_fpga_write32(u32 value, void *addr)
{
	u32 data = cpu_to_le32(value);

	mutex_lock(&pcie_lock);
	iowrite32(data, addr);
	mutex_unlock(&pcie_lock);
}

static ssize_t show_pon_port_act_led(struct device *dev, struct device_attribute *da,
             char *buf){
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct fpga_device* fpga_dev = dev_get_drvdata(dev);
	void __iomem *addr;
	u32 value, mask, offset;

	switch (attr->index) {
	case PON_PORT_ACT_LED_1 ... PON_PORT_ACT_LED_2:
		addr = fpga_dev->hw_bar0 + FPGA_REG_PON_ACT_LINK;
		mask = PON_ACT_LED_MASK(attr->index - PON_PORT_ACT_LED_1);
		offset = PON_ACT_LED_OFFSET(attr->index - PON_PORT_ACT_LED_1);
		break;
	default:
		dev_err(dev, "Unknow attribute.\r\n");
		return -EINVAL;
	}

	value = ascvolt16_fpga_read32(addr);

	return sprintf(buf, "0x%02x\n", (value & mask) >> offset);
}
static ssize_t set_pon_port_act_led(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count){
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct fpga_device* fpga_dev = dev_get_drvdata(dev);
	void __iomem *addr;
	u32 set_led, value, mask, offset;

	if (sscanf(buf, "0x%x", &set_led) != 1) {
		return -EINVAL;
	}

	if(set_led < 0 || set_led > 0xff){
		return -EINVAL;
	}

	switch (attr->index) {
	case PON_PORT_ACT_LED_1 ... PON_PORT_ACT_LED_2:
		addr = fpga_dev->hw_bar0 + FPGA_REG_PON_ACT_LINK;
		mask = PON_ACT_LED_MASK(attr->index - PON_PORT_ACT_LED_1);
		offset = PON_ACT_LED_OFFSET(attr->index - PON_PORT_ACT_LED_1);
		break;
	default:
		dev_err(dev, "Unknow attribute.\r\n");
		return -EINVAL;
	}

	mutex_lock(&fpga_dev->driver_lock);
	/* Read current status */
	value = ascvolt16_fpga_read32(addr);

	/* Update led status */
	value &= ~mask;
	value = value | (set_led << offset);

	ascvolt16_fpga_write32(value, addr);
	mutex_unlock(&fpga_dev->driver_lock);

	return count;
}
static ssize_t show_pon_port_link_led(struct device *dev, struct device_attribute *da,
             char *buf){
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct fpga_device* fpga_dev = dev_get_drvdata(dev);
	void __iomem *addr;
	u32 value, mask, offset;

	switch (attr->index) {
	case PON_PORT_LINK_LED_1 ... PON_PORT_LINK_LED_2:
		addr = fpga_dev->hw_bar0 + FPGA_REG_PON_ACT_LINK;
		mask = PON_LINK_LED_MASK(attr->index - PON_PORT_LINK_LED_1);
		offset = PON_LINK_LED_OFFSET(attr->index - PON_PORT_LINK_LED_1);
		break;
	default:
		dev_err(dev, "Unknow attribute.\r\n");
		return -EINVAL;
	}

	value = ascvolt16_fpga_read32(addr);

	return sprintf(buf, "0x%02x\n", (value & mask) >> offset);
}
static ssize_t set_pon_port_link_led(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count){
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct fpga_device* fpga_dev = dev_get_drvdata(dev);
	void __iomem *addr;
	u32 set_led, value, mask, offset;

	if (sscanf(buf, "0x%x", &set_led) != 1) {
		return -EINVAL;
	}

	if(set_led < 0 || set_led > 0xff){
		return -EINVAL;
	}

	switch (attr->index) {
	case PON_PORT_LINK_LED_1 ... PON_PORT_LINK_LED_2:
		addr = fpga_dev->hw_bar0 + FPGA_REG_PON_ACT_LINK;
		mask = PON_LINK_LED_MASK(attr->index - PON_PORT_LINK_LED_1);
		offset = PON_LINK_LED_OFFSET(attr->index - PON_PORT_LINK_LED_1);
		break;
	default:
		dev_err(dev, "Unknow attribute.\r\n");
		return -EINVAL;
	}

	mutex_lock(&fpga_dev->driver_lock);
	/* Read current status */
	value = ascvolt16_fpga_read32(addr);

	/* Update led status */
	value &= ~mask;
	value = value | (set_led << offset);

	ascvolt16_fpga_write32(value, addr);
	mutex_unlock(&fpga_dev->driver_lock);

	return count;
}

static int ascvolt16_fpga_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id)
{
	struct fpga_device* fpga_dev;
	int pci_dev_busy = 0;
	int rc = -EBUSY;

	dev_info(&pdev->dev, "vend    %d\n", dev_id->vendor);
	dev_info(&pdev->dev, "dev     %d\n", dev_id->device);
	dev_info(&pdev->dev, "subvend %d\n", dev_id->subvendor);
	dev_info(&pdev->dev, "subdev  %d\n", dev_id->subdevice);

	/* Enable pci dev. */
	rc = pci_enable_device(pdev);
	if(rc){
		dev_err(&pdev->dev, "failed to enable pci device.\r\n");
		return rc;
	}

	/* Set PCI host mastering DMA. */
	pci_set_master(pdev);

	/* Make pci request regions for this driver. */
	rc = pci_request_regions(pdev, DRVNAME);
	if(rc){
		pci_dev_busy = 1;
		goto err_out;
	}

	pci_intx(pdev, 1);

	fpga_dev = kzalloc(sizeof(*fpga_dev), GFP_KERNEL);
	if(fpga_dev == NULL){
		dev_err(&pdev->dev, "unable to allocate device memory.\r\n");
		goto err_out_int;
	}

	mutex_init(&fpga_dev->driver_lock);
	mutex_init(&pcie_lock);
	pci_set_drvdata(pdev, fpga_dev);

	/* Remap the BAR0 address of PCI/PCI-E configuration space. */
	fpga_dev->hw_bar0 = pci_ioremap_bar(pdev, 0);
	if(!fpga_dev->hw_bar0){
		dev_err(&pdev->dev, "mapping I/O device memory failure.\r\n");
		rc =  - ENOMEM;
		goto err_out_free;
	}

	/* Remap the BAR1 address of PCI/PCI-E configuration space. */
	fpga_dev->hw_bar1 = pci_ioremap_bar(pdev, 1);
	if(!fpga_dev->hw_bar1){
		dev_err(&pdev->dev, "mapping I/O device memory failure.\r\n");
		rc =  - ENOMEM;
		goto err_out_free;
	}

	/* Remap the BAR2 address of PCI/PCI-E configuration space. */
	fpga_dev->hw_bar2 = pci_ioremap_bar(pdev, 2);
	if(!fpga_dev->hw_bar2){
		dev_err(&pdev->dev, "mapping I/O device memory failure.\r\n");
		rc =  - ENOMEM;
		goto err_out_free;
	}

	/* Remap the BAR3 address of PCI/PCI-E configuration space. */
	fpga_dev->hw_bar3 = pci_ioremap_bar(pdev, 3);
	if(!fpga_dev->hw_bar3){
		dev_err(&pdev->dev, "mapping I/O device memory failure.\r\n");
		rc =  - ENOMEM;
		goto err_out_free;
	}

	rc = sysfs_create_groups(&pdev->dev.kobj, fpga_groups);
	if (rc) {
		dev_err(&pdev->dev, "failed to create attrs.\r\n");
		rc =  - ENOMEM;
		goto err_out_unmap;
	}

	dev_dbg(&pdev->dev, "initialization successful.\r\n");
	return 0;

err_out_unmap:
	iounmap(fpga_dev->hw_bar0);
	iounmap(fpga_dev->hw_bar1);
	iounmap(fpga_dev->hw_bar2);
	iounmap(fpga_dev->hw_bar3);
err_out_free:
	pci_set_drvdata(pdev, NULL);
	kfree(fpga_dev);
err_out_int:
	pci_intx(pdev, 0);
	pci_release_regions(pdev);
err_out:
	if(!pci_dev_busy)
	{
		pci_disable_device(pdev);
	}
	dev_err(&pdev->dev, "initialization failed.\r\n");
	return rc;
}

static void ascvolt16_fpga_remove(struct pci_dev* pdev)
{
	struct fpga_device* fpga_dev = pci_get_drvdata(pdev);

	sysfs_remove_groups(&pdev->dev.kobj, fpga_groups);
	pci_set_drvdata(pdev, NULL);

	iounmap(fpga_dev->hw_bar0);
	iounmap(fpga_dev->hw_bar1);
	iounmap(fpga_dev->hw_bar2);
	iounmap(fpga_dev->hw_bar3);
	pci_intx(pdev, 0);
	pci_release_regions(pdev);

	pci_disable_device(pdev);
	kfree(fpga_dev);
}

static const struct pci_device_id ascvolt_pci_tbl[] =
{
	{ PCI_DEVICE(0x1172, 0xe001) },
	/* Required last entry. */
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, ascvolt_pci_tbl);

static struct pci_driver ascvolt16_fpga_driver = {
	.name = DRVNAME,
	.id_table = ascvolt_pci_tbl,
	.probe = ascvolt16_fpga_probe,
	.remove = ascvolt16_fpga_remove,
};

module_pci_driver(ascvolt16_fpga_driver);

MODULE_AUTHOR("Jake Lin <jake_lin@edge-core.com>");
MODULE_DESCRIPTION("ascvolt16_fpga driver");
MODULE_LICENSE("GPL");