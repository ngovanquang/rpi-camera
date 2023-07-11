#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>

// Define the GPIO pins for stepper motor control
#define COIL_A_1_PIN 17
#define COIL_A_2_PIN 22
#define COIL_B_1_PIN 27
#define COIL_B_2_PIN 23

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

// Function to drive the stepper motor
static void drive_stepper(int delay, int steps, int clockwise)
{
    int i, j;

    for (i = 0; i < steps; i++)
    {
        if (clockwise == 0)
        {
            for (j = 0; j < 8; j++)
            {
                gpio_set_value(COIL_A_1_PIN, sequence[j][0]);
                gpio_set_value(COIL_A_2_PIN, sequence[j][1]);
                gpio_set_value(COIL_B_1_PIN, sequence[j][2]);
                gpio_set_value(COIL_B_2_PIN, sequence[j][3]);
                fsleep(delay);
            }
        }
        else
        {
            for (j = 7; j >= 0; --j)
            {
                gpio_set_value(COIL_A_1_PIN, sequence[j][0]);
                gpio_set_value(COIL_A_2_PIN, sequence[j][1]);
                gpio_set_value(COIL_B_1_PIN, sequence[j][2]);
                gpio_set_value(COIL_B_2_PIN, sequence[j][3]);
                fsleep(delay);
            }
        }
    }
}

// Initialize the stepper motor driver
static int __init stepper_init(void)
{
    // Request and configure the GPIO pins
    gpio_request(COIL_A_1_PIN, "Stepper Motor Coil A 1");
    gpio_direction_output(COIL_A_1_PIN, 0);

    gpio_request(COIL_A_2_PIN, "Stepper Motor Coil A 2");
    gpio_direction_output(COIL_A_2_PIN, 0);

    gpio_request(COIL_B_1_PIN, "Stepper Motor Coil B 1");
    gpio_direction_output(COIL_B_1_PIN, 0);

    gpio_request(COIL_B_2_PIN, "Stepper Motor Coil B 2");
    gpio_direction_output(COIL_B_2_PIN, 0);

    int cnt;
    cnt = 0;
    // Drive the stepper motor
    while (cnt < 30)
    {
        drive_stepper(400, 400, 1); // 2ms delay, 100 steps, clockwise

        ssleep(1);

        drive_stepper(400, 400, 0);
        ssleep(1);
        cnt++;
    }

    return 0;
}

// Cleanup the stepper motor driver
static void __exit stepper_exit(void)
{
    // Stop the stepper motor
    gpio_set_value(COIL_A_1_PIN, 0);
    gpio_set_value(COIL_A_2_PIN, 0);
    gpio_set_value(COIL_B_1_PIN, 0);
    gpio_set_value(COIL_B_2_PIN, 0);

    // Free the GPIO pins
    gpio_free(COIL_A_1_PIN);
    gpio_free(COIL_A_2_PIN);
    gpio_free(COIL_B_1_PIN);
    gpio_free(COIL_B_2_PIN);
}

// Module initialization and cleanup functions
module_init(stepper_init);
module_exit(stepper_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ngo Van Quang");
MODULE_DESCRIPTION("Stepper Motor Driver");
