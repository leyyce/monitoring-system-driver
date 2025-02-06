#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/crc32.h>
#include <linux/uaccess.h>

#define MONITORING_SYS_ADDR 0x10
#define MAX_BUFFER_SIZE 773

/* Meta Information */
MODULE_AUTHOR("Leya Wehner & Julian Frank");
MODULE_DESCRIPTION("Monitoring System I2C Driver");
MODULE_LICENSE("GPL");

static uint32_t calculate_crc(const uint8_t *data, size_t len) {
    return crc32(0, data, len);
}


static struct i2c_client *monitoring_sys_client = NULL;

// Probe function - called when the device is detected
static int monitoring_sys_probe(struct i2c_client *client);

// Remove function - called when the device is removed
static void monitoring_sys_remove(struct i2c_client *client);

// Device Tree Match Table
static const struct of_device_id monitoring_sys_of_match[] = {
    { .compatible = "embedded_linux,monitoring_system" },
    { /* sentinel */}
};
MODULE_DEVICE_TABLE(of, monitoring_sys_of_match);

// I2C Device ID Table (not strictly needed if only using DT)
static const struct i2c_device_id monitoring_sys_id[] = {
    { "monitoring_system", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, monitoring_sys_id);

// I2C Driver Structure
static struct i2c_driver monitoring_sys_driver = {
    .driver = {
        .name = "monitoring_system",
        .of_match_table = monitoring_sys_of_match,
    },
    .probe = monitoring_sys_probe,
    .remove = monitoring_sys_remove,
    .id_table = monitoring_sys_id,
};

static struct proc_dir_entry *proc_file;

static ssize_t monitoring_sys_write(struct file *File, const char __user *user_buffer, size_t count, loff_t *offs) {
    pr_info("monitoring_sys: In the monitoring_sys_write function. count: %ld", count);
    uint8_t kernel_buffer[MAX_BUFFER_SIZE];
    uint32_t crc;

    if (count > MAX_BUFFER_SIZE) {
        pr_err("monitoring_sys: count [%ld] > MAX_BUFFER_SIZE", count);
        return -EINVAL;
    }
    int ret;
    if ((ret = copy_from_user(kernel_buffer, user_buffer, count))) {
        pr_err("monitoring_sys: Couldn't copy %d of %ldbytes from user buffer to kernel buffer", ret, count);
        return -EFAULT;
    }
    // CRC Berechnung
    crc = calculate_crc(kernel_buffer, count);
    kernel_buffer[count - 1] = crc & 0xFF;
    kernel_buffer[count] = (crc >> 8) & 0xFF;
    kernel_buffer[count + 1] = (crc >> 16) & 0xFF;
    kernel_buffer[count + 2] = (crc >> 24) & 0xFF;

    pr_info("CRC done");
    
    // Check if client is initialized
    if (!monitoring_sys_client) {
        pr_err("monitoring_sys: I2C client not initialized!\n");
        return -ENODEV;
    }

    pr_info("Before write");

    if ((ret = i2c_smbus_write_i2c_block_data(monitoring_sys_client, 0, count + 4, kernel_buffer)) < 0) {
        pr_err("monitoring_sys: I²C write failed: %d\n", ret);
        return -EIO;
    }

    pr_info("monitoring_sys: Wrote value to I2C device\n");
    return count;
};


static struct proc_ops fops = {
    .proc_write = monitoring_sys_write,
};

// Probe function - called when the device is detected
static int monitoring_sys_probe(struct i2c_client *client) {
    pr_info("monitoring_sys: I²C Device probed: Address 0x%02x\n", client->addr);

    if (client->addr != 0x10)
    {
        pr_info("monitoring_sys: Wrong I²C adress!\n");
        return -1; 
    }

    monitoring_sys_client = client;

    proc_file = proc_create("monitoring-system", 0666, NULL, &fops);
    if (proc_file == NULL) {
        pr_info("monitoring_sys: Error creating /proc/monitoring-system\n");
        return -ENOMEM;
    }

    return 0;
};

// Remove function - called when the device is removed
static void monitoring_sys_remove(struct i2c_client *client) {
    pr_info("monitoring_sys: I²C Device removed: Address 0x%02x\n", client->addr);
    proc_remove(proc_file);
    proc_file = NULL;
};

// Module Init and Exit
module_i2c_driver(monitoring_sys_driver);
