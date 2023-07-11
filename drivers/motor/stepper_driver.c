#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
// #include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "stepper_driver.h"

#define DRIVER_LICENSE "GPL"
#define DRIVER_AUTHOR "Ngo Van Quang <enviquy@gmail.com>"
#define DRIVER_DESC "A sample stepper motor driver to drive PTZ camera module"
#define DRIVER_VERSION "0.6"

// #define MAGICAL_NUMBER 243
// #define MOTOR_STOP 			_IO(MAGICAL_NUMBER, 0)
// #define MOTOR_RESET 		_IOW(MAGICAL_NUMBER, 1, motor_reset_data_t*)
// #define MOTOR_MOVE			_IOW(MAGICAL_NUMBER, 2, motors_steps_t*)
// #define MOTOR_SPEED 		_IOW(MAGICAL_NUMBER, 3, unsigned int*)
// #define MOTOR_GET_STATUS 	_IOR(MAGICAL_NUMBER, 4, motor_message_t*)

#define MOTOR_STOP 0x1
#define MOTOR_MOVE 0x3
#define MOTOR_GET_STATUS 0x4
#define MOTOR_SPEED 0x5
#define MOTOR_RESET 0x6
// #define MOTOR_GOBACK	0x6
// #define MOTOR_CRUISE	0x7

// Define the stepper motor sequence
static int sequence[][4] = {
	{1, 0, 0, 1},
	{1, 0, 0, 0},
	{1, 1, 0, 0},
	{0, 1, 0, 0},
	{0, 1, 1, 0},
	{0, 0, 1, 0},
	{0, 0, 1, 1},
	{0, 0, 0, 1}};

struct _stepper_drv
{
	dev_t dev_num;
	struct class *dev_class;
	struct device *dev;
	struct cdev *vcdev;
	unsigned int open_cnt;
} stepper_drv;

struct motors_steps_t init_steps = {
	.x = INIT_X_STEP,
	.y = INIT_Y_STEP,
};

struct stepper_gpio_pins_t hoziontal_motor_pins = {
	.motor_coil_a_1_pin = HORIRONTAL_COIL_A_1_PIN,
	.motor_coil_a_2_pin = HORIRONTAL_COIL_A_2_PIN,
	.motor_coil_b_1_pin = HORIRONTAL_COIL_B_1_PIN,
	.motor_coil_b_2_pin = HORIRONTAL_COIL_B_2_PIN,
};

struct stepper_gpio_pins_t vertical_motor_pins = {
	.motor_coil_a_1_pin = VERTICAL_COIL_A_1_PIN,
	.motor_coil_a_2_pin = VERTICAL_COIL_A_2_PIN,
	.motor_coil_b_1_pin = VERTICAL_COIL_B_1_PIN,
	.motor_coil_b_2_pin = VERTICAL_COIL_B_2_PIN,
};

struct stepper_device_t stepper_dev = {
	.hozirontal_motor_pins = &hoziontal_motor_pins,
	.vertical_motor_pins = &vertical_motor_pins,
	.cur_steps = &init_steps,
	.motor_speed = INIT_SPEED,
	.status = MOTOR_IS_STOP,
};

/******************** Device specific - START ***********************/

/* Ham khoi tao thiet bi*/

/* Ham giai phong thiet bi*/

/* Cac ham xu ly tu thiet bi*/

static void reset_gpio_pins(enum motor_type motor)
{
	if (motor == HORIRONTAL_MOTOR)
	{
		gpio_set_value(stepper_dev.hozirontal_motor_pins->motor_coil_a_1_pin, 0);
		gpio_set_value(stepper_dev.hozirontal_motor_pins->motor_coil_a_2_pin, 0);
		gpio_set_value(stepper_dev.hozirontal_motor_pins->motor_coil_b_1_pin, 0);
		gpio_set_value(stepper_dev.hozirontal_motor_pins->motor_coil_b_2_pin, 0);
	}
	else
	{
		gpio_set_value(stepper_dev.vertical_motor_pins->motor_coil_a_1_pin, 0);
		gpio_set_value(stepper_dev.vertical_motor_pins->motor_coil_a_2_pin, 0);
		gpio_set_value(stepper_dev.vertical_motor_pins->motor_coil_b_1_pin, 0);
		gpio_set_value(stepper_dev.vertical_motor_pins->motor_coil_b_2_pin, 0);
	}
}

static void motor_stepper(int steps, int delay, int clockwise, enum motor_type motor)
{
	struct stepper_gpio_pins_t *motor_pins;
	if (motor == HORIRONTAL_MOTOR)
	{
		motor_pins = stepper_dev.hozirontal_motor_pins;
	}
	else
	{
		motor_pins = stepper_dev.vertical_motor_pins;
	}

	int i, j;

	for (i = 0; i < steps; i++)
	{
		if (stepper_dev.status == MOTOR_IS_STOP)
		{
			if (motor == HORIRONTAL_MOTOR)
			{
				stepper_dev.cur_steps->x += (i)*clockwise;
			}
			else
			{
				stepper_dev.cur_steps->y += (i)*clockwise;
			}
			return;
		}
		if (clockwise == -1)
		{
			for (j = 0; j < 8; j++)
			{
				gpio_set_value(motor_pins->motor_coil_a_1_pin, sequence[j][0]);
				gpio_set_value(motor_pins->motor_coil_a_2_pin, sequence[j][1]);
				gpio_set_value(motor_pins->motor_coil_b_1_pin, sequence[j][2]);
				gpio_set_value(motor_pins->motor_coil_b_2_pin, sequence[j][3]);
				fsleep(delay);
			}
		}
		else
		{
			for (j = 7; j >= 0; --j)
			{
				gpio_set_value(motor_pins->motor_coil_a_1_pin, sequence[j][0]);
				gpio_set_value(motor_pins->motor_coil_a_2_pin, sequence[j][1]);
				gpio_set_value(motor_pins->motor_coil_b_1_pin, sequence[j][2]);
				gpio_set_value(motor_pins->motor_coil_b_2_pin, sequence[j][3]);
				fsleep(delay);
			}
		}
	}

	if (motor == HORIRONTAL_MOTOR)
	{
		stepper_dev.cur_steps->x += steps * clockwise;
	}
	else
	{
		stepper_dev.cur_steps->y += steps * clockwise;
	}

	// reset_gpio_pins(motor);
}

static void stepper_motor_stop(void)
{
	printk("Stepper motor is stop");
	if (stepper_dev.status == MOTOR_IS_RUNNING)
	{
		stepper_dev.status = MOTOR_IS_STOP;
	}
	return;
}

static int stepper_motor_move(int x, int y)
{
	stepper_dev.status = MOTOR_IS_RUNNING;
	unsigned int next_x_step = x + stepper_dev.cur_steps->x;
	unsigned int next_y_step = y + stepper_dev.cur_steps->y;
	if ((next_x_step > MAX_X_STEP || next_x_step < MIN_X_STEP) || (next_y_step > MAX_Y_STEP || next_y_step < MIN_Y_STEP))
	{
		printk("Can't move, out of range\n");
		stepper_dev.status = MOTOR_IS_STOP;
		return -1;
	}
	int x_clockwise = ((next_x_step > stepper_dev.cur_steps->x) ? 1 : -1);
	int y_clockwise = ((next_y_step > stepper_dev.cur_steps->y) ? 1 : -1);
	printk("X clockwise: %d, Y clockwise: %d\n", x_clockwise, y_clockwise);
	int x_step = (next_x_step - stepper_dev.cur_steps->x) * x_clockwise;
	int y_step = (next_y_step - stepper_dev.cur_steps->y) * y_clockwise;
	int delay = 1000000 / stepper_dev.motor_speed; // micro second
	if (x != 0)
	{
		motor_stepper(x_step, delay, x_clockwise, HORIRONTAL_MOTOR);
	}
	if (y != 0)
	{
		motor_stepper(y_step, delay, y_clockwise, VERTICAL_MOTOR);
	}
	printk("PTZ is move to pos: (%d,%d)\n", stepper_dev.cur_steps->x, stepper_dev.cur_steps->y);
	stepper_dev.status = MOTOR_IS_STOP;
	return 0;
}

static void stepper_motor_reset(void)
{
	stepper_motor_stop();
	fsleep(50000);
	int x = INIT_X_STEP - stepper_dev.cur_steps->x;
	int y = INIT_Y_STEP - stepper_dev.cur_steps->y;
	printk("Handle reset motor\n");
	stepper_motor_move(x, y);
	if (stepper_dev.cur_steps->x != INIT_X_STEP && stepper_dev.cur_steps->y != INIT_Y_STEP)
	{
		printk("Stepper reset unsuccessful\n");
		return;
	}
}

static void stepper_motor_get_status(struct motor_message_t *msg)
{
	msg->speed = stepper_dev.motor_speed;
	msg->status = stepper_dev.status;
	msg->x = stepper_dev.cur_steps->x;
	msg->y = stepper_dev.cur_steps->y;
}

static int stepper_motor_speed(int speed)
{
	if (speed > MAX_SPEED || speed < MIN_SPEED)
	{
		printk("Invalid speed: (%d,%d)\n", MIN_SPEED, MAX_SPEED);
		return -1;
	}
	stepper_dev.motor_speed = speed;
	printk("Motor has change speed to: %d\n", speed);
	return 0;
}
/* Ham xu ly tin hieu ngat gui tu thiet bi*/

/******************** Device specific - END ***********************/

/******************** OS specific - START ***********************/

/* cac ham entry point */
static int stepper_driver_open(struct inode *inode, struct file *filp)
{
	stepper_drv.open_cnt++;
	printk("Handle opened event (%d)\n", stepper_drv.open_cnt);
	return 0;
}

static int stepper_driver_release(struct inode *inode, struct file *filp)
{
	printk("Handle closed event\n");
	return 0;
}

static long stepper_driver_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	printk("Handle ioctl event (cmd: %u)\n", cmd);

	switch (cmd)
	{
	case MOTOR_STOP:
		stepper_motor_stop();
		break;
	case MOTOR_RESET:
		stepper_motor_reset();
		break;
	case MOTOR_MOVE:
	{
		struct motors_steps_t dst;
		if (copy_from_user(&dst, (void __user *)arg, sizeof(struct motors_steps_t)))
		{
			printk("[%s][%d] copy from user error\n", __func__, __LINE__);
			return -1;
		}
		ret = stepper_motor_move(dst.x, dst.y);
		if (ret < 0)
			printk("Can't move motor\n");
		else
			printk("motor has been move\n");
		break;
	}
	case MOTOR_SPEED:
	{
		int speed;
		if (copy_from_user(&speed, (void __user *)arg, sizeof(int)))
		{
			printk("[%s][%d] copy from user error\n", __func__, __LINE__);
			return -1;
		}
		ret = stepper_motor_speed(speed);
		if (ret < 0)
			printk("Can't set speed\n");
		else
			printk("motor has been set speed\n");
		break;
	}

	case MOTOR_GET_STATUS:
	{
		struct motor_message_t msg;
		stepper_motor_get_status(&msg);
		if (copy_to_user((void __user *)arg, &msg, sizeof(struct motor_message_t)))
		{
			printk("[%s][%d] copy to user error\n", __func__, __LINE__);
			return -1;
		}
		break;
	}

	default:
		break;
	}
	return ret;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = stepper_driver_open,
	.release = stepper_driver_release,
	.unlocked_ioctl = stepper_driver_ioctl,
};

/* ham khoi tao driver */

static int __init stepper_driver_init(void)
{
	int ret = 0;

	/* cap phat device number */
	stepper_drv.dev_num = 0;
	ret = alloc_chrdev_region(&stepper_drv.dev_num, 0, 1, "stepper_device");
	if (ret < 0)
	{
		printk("failed to register device number dynamically\n");
		goto failed_register_devnum;
	}
	printk("allocated device number (%d,%d)\n", MAJOR(stepper_drv.dev_num), MINOR(stepper_drv.dev_num));

	/* tao driver file */
	stepper_drv.dev_class = class_create(THIS_MODULE, "class_stepper_dev");
	if (stepper_drv.dev_class == NULL)
	{
		printk("failed to create a device class\n");
		goto failed_create_class;
	}

	stepper_drv.dev = device_create(stepper_drv.dev_class, NULL, stepper_drv.dev_num, NULL, "motor");
	if (IS_ERR(stepper_drv.dev))
	{
		printk("failed to create a device\n");
		goto failed_create_device;
	}

	/* cap phat bo nho cho cac cau truc du lieu cua driver va khoi tao */

	/* Khoi tao thiet bi vat ly */

	/* Dang ky cac entry point voi kernel */
	stepper_drv.vcdev = cdev_alloc();
	if (stepper_drv.vcdev == NULL)
	{
		printk("failed to allocate cdev structure\n");
		goto failed_allocate_cdev;
	}

	cdev_init(stepper_drv.vcdev, &fops);
	ret = cdev_add(stepper_drv.vcdev, stepper_drv.dev_num, 1);
	if (ret < 0)
	{
		printk("failed to add a char device to the system\n");
		goto failed_allocate_cdev;
	}
	/* Dang ky ham xu ly ngat */

	/*Request va configure gpio pins*/
	gpio_request(stepper_dev.hozirontal_motor_pins->motor_coil_a_1_pin, "Horizontal Motor Coil A 1");
	gpio_direction_output(stepper_dev.hozirontal_motor_pins->motor_coil_a_1_pin, 0);

	gpio_request(stepper_dev.hozirontal_motor_pins->motor_coil_a_2_pin, "Horizontal Motor Coil A 2");
	gpio_direction_output(stepper_dev.hozirontal_motor_pins->motor_coil_a_2_pin, 0);

	gpio_request(stepper_dev.hozirontal_motor_pins->motor_coil_b_1_pin, "Horizontal Motor Coil B 1");
	gpio_direction_output(stepper_dev.hozirontal_motor_pins->motor_coil_b_1_pin, 0);

	gpio_request(stepper_dev.hozirontal_motor_pins->motor_coil_b_2_pin, "Horizontal Motor Coil B 2");
	gpio_direction_output(stepper_dev.hozirontal_motor_pins->motor_coil_b_2_pin, 0);

	gpio_request(stepper_dev.vertical_motor_pins->motor_coil_a_1_pin, "Vertical Motor Coil A 1");
	gpio_direction_output(stepper_dev.vertical_motor_pins->motor_coil_a_1_pin, 0);

	gpio_request(stepper_dev.vertical_motor_pins->motor_coil_a_2_pin, "vertical Motor Coil A 2");
	gpio_direction_output(stepper_dev.vertical_motor_pins->motor_coil_a_2_pin, 0);

	gpio_request(stepper_dev.vertical_motor_pins->motor_coil_b_1_pin, "Vertical Motor Coil B 1");
	gpio_direction_output(stepper_dev.vertical_motor_pins->motor_coil_b_1_pin, 0);

	gpio_request(stepper_dev.vertical_motor_pins->motor_coil_b_2_pin, "Vertical Motor Coil B 2");
	gpio_direction_output(stepper_dev.vertical_motor_pins->motor_coil_b_2_pin, 0);

	printk("Initialize stepper driver successfully\n");
	return 0;

failed_allocate_cdev:

failed_create_device:
	class_destroy(stepper_drv.dev_class);
failed_create_class:
	unregister_chrdev_region(stepper_drv.dev_num, 1);
failed_register_devnum:
	return ret;
}

/* ham ket thuc driver */
static void free_gpio_pin(void)
{
	gpio_free(stepper_dev.hozirontal_motor_pins->motor_coil_a_1_pin);
	gpio_free(stepper_dev.hozirontal_motor_pins->motor_coil_a_2_pin);
	gpio_free(stepper_dev.hozirontal_motor_pins->motor_coil_b_1_pin);
	gpio_free(stepper_dev.hozirontal_motor_pins->motor_coil_b_2_pin);

	gpio_free(stepper_dev.vertical_motor_pins->motor_coil_a_1_pin);
	gpio_free(stepper_dev.vertical_motor_pins->motor_coil_a_2_pin);
	gpio_free(stepper_dev.vertical_motor_pins->motor_coil_b_1_pin);
	gpio_free(stepper_dev.vertical_motor_pins->motor_coil_b_2_pin);
}

static void __exit stepper_driver_exit(void)
{
	// Stop the stepper motor
	reset_gpio_pins(HORIRONTAL_MOTOR);
	reset_gpio_pins(VERTICAL_MOTOR);

	// Free the GPIO pins
	free_gpio_pin();

	/* huy dang ky xu ly ngat */

	/* huy dang ky entry point voi kernel */
	cdev_del(stepper_drv.vcdev);

	/* giai phong thiet bi vat ly */

	/* giai phong bo nho da cap phat cau truc du lieu cua driver */

	/* xoa bo nho device file */
	device_destroy(stepper_drv.dev_class, stepper_drv.dev_num);
	class_destroy(stepper_drv.dev_class);

	/* giai phong device number */
	unregister_chrdev_region(stepper_drv.dev_num, 1);

	printk("Exit stepper driver\n");
}

/******************** OS specific - END ***********************/

module_init(stepper_driver_init);
module_exit(stepper_driver_exit);

MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
// MODULE_SUPPORTED_DEVICE("stepper_driver");
