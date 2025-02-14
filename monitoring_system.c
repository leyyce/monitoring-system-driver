/*
monitoring_system.c implementiert einen Treiber für unser Monitoring System und dient als Schnittstelle zwischen unserem Userspace-Service und dem Kernel.

Der vom sysmond Service erhaltene Datenframe wird im Kernel mit einem CRC versehen und per Bitbashing über die GPIOs übertragen.
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/crc32.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#define MONITORING_SYS_ADDR 0x10
#define MAX_BUFFER_SIZE 773 // 1 byte address + (256 * 1 byte value id) + (256 * 2 byte value) + 4 byte crc = 773 bytes

/* Meta Information */
MODULE_AUTHOR("Leya Wehner & Julian Frank");
MODULE_DESCRIPTION("Monitoring System I2C Driver");
MODULE_LICENSE("GPL");

/* CRC-32/JAMCRC */
static uint32_t calculate_crc(const uint8_t *data, size_t len)
{
    return crc32(0xFFFFFFFF, data, len);
}

// Probe function - called when the device is detected
static int monitoring_sys_probe(struct platform_device *pdev);

// Remove function - called when the device is removed
static int monitoring_sys_remove(struct platform_device *pdev);

// Device Tree Match Table
static const struct of_device_id monitoring_sys_of_match[] = {
    {.compatible = "embedded_linux,monitoring_system"},
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, monitoring_sys_of_match);

// GPIO Driver Structure
static struct platform_driver monitoring_sys_driver = {
    .driver = {
        .name = "monitoring-system",
        .of_match_table = monitoring_sys_of_match,
    },
    .probe = monitoring_sys_probe,
    .remove = monitoring_sys_remove,
};

/* GPIO variables */
struct gpio_desc *msd = NULL;
struct gpio_desc *msc = NULL;

static struct proc_dir_entry *proc_file = NULL;

static ssize_t monitoring_sys_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offs) {
    pr_info("monitoring-sys: In the monitoring_sys_write function. count: %zu\n", count);
    uint8_t kernel_buffer[MAX_BUFFER_SIZE];
    uint32_t crc;
    int ret;
    size_t total_len;

    if (count > MAX_BUFFER_SIZE - 4)
    {
        pr_err("monitoring-sys: count [%zu] > MAX_BUFFER_SIZE\n", count);
        return -EINVAL;
    }

    if ((ret = copy_from_user(kernel_buffer, user_buffer, count)))
    {
        pr_err("monitoring-sys: Couldn't copy %d of %zu bytes from user buffer to kernel buffer\n", ret, count);
        return -EFAULT;
    }

    crc = calculate_crc(kernel_buffer, count);
    pr_info("monitoring-sys: crc=0x%08X\n", crc);
    kernel_buffer[count] = crc & 0xFF;
    kernel_buffer[count + 1] = (crc >> 8) & 0xFF;
    kernel_buffer[count + 2] = (crc >> 16) & 0xFF;
    kernel_buffer[count + 3] = (crc >> 24) & 0xFF;

    total_len = count + 4;

    pr_info("monitoring-sys: total_len=%zu\n", total_len);

    for (int i = 0; i < total_len; i++) {
        pr_info("monitoring-sys: kernel_buffer[%d] = 0x%02X\n", i, kernel_buffer[i]);
        for (int j = 0; j < 8; j++) {
            pr_info("monitoring-sys: kernel_buffer[%d] bit [%d] = %u\n", i, j, (kernel_buffer[i] >> j) & 1);
            gpiod_set_value(msd, (kernel_buffer[i] >> j) & 1);
            usleep_range(100, 100);
            gpiod_set_value(msc, 1);
            usleep_range(200, 200);
            gpiod_set_value(msc, 0);
            usleep_range(100, 100);
        }
    }
    gpiod_set_value(msd, 0);
	return total_len;
};

static struct proc_ops fops = {
    .proc_write = monitoring_sys_write,
};

// Probe function - called when the device is detected
static int monitoring_sys_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    pr_info("monitoring-sys: Device probed\n");

    if (!device_property_present(dev, "msd-gpio"))
    {
        pr_err("monitoring-sys: No msd-gpio property found\n");
        return -EINVAL;
    }
    if (!device_property_present(dev, "msc-gpio"))
    {
        pr_err("monitoring-sys: No msc-gpio property found\n");
        return -EINVAL;
    }

    /* Init GPIO */
    msd = gpiod_get(dev, "msd", GPIOD_OUT_LOW);
    if (IS_ERR(msd))
    {
        pr_err("monitoring-sys: Couldn't get msd GPIO\n");
        return PTR_ERR(msd);
    }

    msc = gpiod_get(dev, "msc", GPIOD_OUT_LOW);
    if (IS_ERR(msc))
    {
        pr_err("monitoring-sys: Couldn't get msd GPIO\n");
        return PTR_ERR(msc);
    }

    proc_file = proc_create("monitoring-system", 0666, NULL, &fops);
    if (proc_file == NULL)
    {
        pr_info("monitoring-sys: Error creating /proc/monitoring-system\n");
        gpiod_put(msd);
        gpiod_put(msc);
        return -ENOMEM;
    }

    return 0;
};

// Remove function - called when the device is removed
static int monitoring_sys_remove(struct platform_device *pdev)
{
    pr_info("monitoring-sys: Device removed\n");
    gpiod_put(msd);
    gpiod_put(msc);
    proc_remove(proc_file);
    proc_file = NULL;
    msd = NULL;
    msc = NULL;
    return 0;
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init monitoring_system_init(void) {
	printk("monitoring-sys: Loading the driver...\n");
	if(platform_driver_register(&monitoring_sys_driver)) {
		printk("monitoring-sys: Error! Could not load driver\n");
		return -1;
	}
	return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit monitoring_system_exit(void) {
	printk("monitoring sys: Unloading the driver...\n");
	platform_driver_unregister(&monitoring_sys_driver);
}

module_init(monitoring_system_init);
module_exit(monitoring_system_exit);
