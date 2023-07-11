#ifndef __PTZ_CONTROL__
#define __PTZ_CONTROL__

#define DEVICE_NODE "/dev/motor"

struct motors_steps_t{
	int x;
	int y;
};

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


#define MOTOR_STOP		    0x1
#define MOTOR_MOVE		    0x3
#define MOTOR_GET_STATUS	0x4
#define MOTOR_SPEED		    0x5
#define MOTOR_RESET		    0x6

static char MAGIC_NUMBER[] = {0x11, 0x12, 0x13, 0x14};

void control_ptz_stop(void);

void control_ptz_reset(void);

void control_ptz_get_status(struct motor_message_t* msg);

void control_ptz_move(struct motors_steps_t* relative_position);

void control_ptz_speed(int* speed);

int handle_ptz_control_message(const char* cmd, int len);


#endif