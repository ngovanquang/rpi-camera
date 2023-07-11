#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DEVICE_NAME "neopixel"
#define GPIO_PIN 10

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("NeoPixel Linux Driver");

static int neopixel_open(struct inode *inode, struct file *file)
{
    gpio_direction_output(GPIO_PIN, 0); // Configure GPIO pin as output
    return 0;
}

static int neopixel_release(struct inode *inode, struct file *file)
{
    gpio_set_value(GPIO_PIN, 0); // Turn off NeoPixels
    return 0;
}

static ssize_t neopixel_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    char color;
    ssize_t retval = -1;
    if (copy_from_user(&color, buf, sizeof(char)) == 0)
    {
        if (color == 'R')
        {
            gpio_set_value(GPIO_PIN, 1); // Set GPIO pin HIGH for red color
            retval = 1;
        }
        else if (color == 'G')
        {
            gpio_set_value(GPIO_PIN, 0); // Set GPIO pin LOW for green color
            retval = 1;
        }
        else if (color == 'B')
        {
            gpio_set_value(GPIO_PIN, 1); // Set GPIO pin HIGH for blue color
            retval = 1;
        }
    }
    return retval;
}

static struct file_operations neopixel_fops = {
    .owner = THIS_MODULE,
    .open = neopixel_open,
    .release = neopixel_release,
    .write = neopixel_write,
};

static int __init neopixel_init(void)
{
    int result = register_chrdev(0, DEVICE_NAME, &neopixel_fops);
    if (result < 0)
    {
        printk(KERN_ALERT "Failed to register the device\n");
        return result;
    }

    if (gpio_request(GPIO_PIN, "neopixel") < 0)
    {
        printk(KERN_ALERT "Failed to request GPIO pin\n");
        unregister_chrdev(result, DEVICE_NAME);
        return -1;
    }

    printk(KERN_INFO "NeoPixel driver loaded\n");
    return 0;
}

static void __exit neopixel_exit(void)
{
    gpio_set_value(GPIO_PIN, 0); // Turn off NeoPixels
    gpio_free(GPIO_PIN);         // Free GPIO pin
    unregister_chrdev(0, DEVICE_NAME);
    printk(KERN_INFO "NeoPixel driver unloaded\n");
}

module_init(neopixel_init);
module_exit(neopixel_exit);
