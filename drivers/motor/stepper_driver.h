#ifndef __STEPPER_DRIVER__
#define __STEPPER_DRIVER__

// Horizontal motor
#define HORIRONTAL_MOTOR_GPIO_LEVEL (0)
#define HORIRONTAL_COIL_A_1_PIN (17)
#define HORIRONTAL_COIL_A_2_PIN (27)
#define HORIRONTAL_COIL_B_1_PIN (22)
#define HORIRONTAL_COIL_B_2_PIN (23)

// Vertical motor
#define VERTICAL_MOTOR_GPIO_LEVEL (0)
#define VERTICAL_COIL_A_1_PIN (12)
#define VERTICAL_COIL_A_2_PIN (13)
#define VERTICAL_COIL_B_1_PIN (5)
#define VERTICAL_COIL_B_2_PIN (6)


#define MAX_SPEED (2500)
#define MIN_SPEED (100)

#define INIT_SPEED (2000)

#define MAX_X_STEP (1000)
#define MIN_X_STEP (0)
#define MAX_Y_STEP (1000)
#define MIN_Y_STEP (0)

#define INIT_X_STEP ((MAX_X_STEP - MIN_X_STEP) / 2)
#define INIT_Y_STEP ((MAX_Y_STEP - MIN_Y_STEP) / 2)

enum motor_status {
	MOTOR_IS_STOP,
	MOTOR_IS_RUNNING,
};

struct motor_message_t{
	int x;
	int y;
	enum motor_status status;
	int speed;
};

struct motors_steps_t{
	int x;
	int y;
};

struct stepper_gpio_pins_t {
    unsigned int motor_coil_a_1_pin;
    unsigned int motor_coil_a_2_pin;
    unsigned int motor_coil_b_1_pin;
    unsigned int motor_coil_b_2_pin;
};

enum motor_type {
    HORIRONTAL_MOTOR,
    VERTICAL_MOTOR,
};

struct stepper_device_t {
    struct stepper_gpio_pins_t* hozirontal_motor_pins;
    struct stepper_gpio_pins_t* vertical_motor_pins;
    unsigned int motor_speed;                     // step per second
    struct motors_steps_t* cur_steps;
    enum motor_status status;
};

#endif
