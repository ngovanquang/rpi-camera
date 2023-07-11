#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "../stepper_driver.h"

#define DEVICE_NODE "/dev/motor"

#define MOTOR_STOP		0x1
#define MOTOR_RESET		0x6
#define MOTOR_MOVE		0x3
#define MOTOR_GET_STATUS	0x4
#define MOTOR_SPEED		0x5
// #define MOTOR_GOBACK	0x6
// #define MOTOR_CRUISE	0x7

/*ham kiem tra entry point open cua stepper driver*/

static int open_stepperdev() {
    int fd = open(DEVICE_NODE, O_RDWR);
    if (fd < 0)
    {
        printf("Can't open the device file\n");
        exit(1);
    }
    return fd;
}


static void print_menu()
{
    printf("-------------(MENU OPTIONS)-------------\n");
    printf("\tm (to open the menu options)\n");
    printf("\tS (to stop motor)\n");
    printf("\tg (to get status motor)\n");
    printf("\tr (to reset motor)\n");
    printf("\tv (to set speed of motor)\n");
    printf("\tM (to to Move motor)\n");
    printf("\tw (to move up motor)\n");
    printf("\ts (to move down motor)\n");
    printf("\ta (to move left motor)\n");
    printf("\td (to move right motor)\n");
    printf("\to (to open a device node)\n");
    printf("\tc (to close the device node)\n");
    printf("\tq (to quit the device node)\n");
    printf("----------------------------------------\n");
}
/* ham kiem tra entry point release cua stepper driver */
void close_stepperdev(int fd)
{
    close(fd);
}

void control_stop_motors() {
    int fd = open_stepperdev();
    int ret = ioctl(fd, MOTOR_STOP);
    close_stepperdev(fd);
}

void control_motor_reset()
{
    int fd = open_stepperdev();
    int ret = ioctl(fd, MOTOR_RESET); 
    close_stepperdev(fd);
}

void control_get_status(struct motor_message_t* msg)
{
    int fd = open_stepperdev();
    int ret = ioctl(fd, MOTOR_GET_STATUS, msg);
    if (ret >= 0) {
        printf("+++++++++++++++++++++++++++++\n");
        printf("motor status:\n");
        printf("\tspeed: %d\n", msg->speed);
        printf("\tstatus: %s\n", msg->status == MOTOR_IS_RUNNING ? "Running" : "Stopped");
        printf("\tx: %d\n", msg->x);
        printf("\ty: %d\n", msg->y);
        printf("+++++++++++++++++++++++++++++\n");
    }
    close_stepperdev(fd);
}

void control_motor_move(struct motors_steps_t* position)
{
    int fd = open_stepperdev();
    int ret = ioctl(fd, MOTOR_MOVE, position); 
    close_stepperdev(fd);
}

void control_motor_speed(int* speed)
{
    int fd = open_stepperdev();
    int ret = ioctl(fd, MOTOR_SPEED, speed); 
    close_stepperdev(fd);
}



int main() {
    int ret = 0;
    char option = 'q';
    int fd = -1;
    printf("Select bellow to options:\n");
    print_menu();
    while (1)
    {
        printf("Enter your option: ");
        scanf(" %c", &option);
        switch (option)
        {
        case 'o':
            if (fd < 0)
                fd = open_stepperdev();
            else 
                printf("%s has already opened\n", DEVICE_NODE);
            break;
        case 'c':
            if (fd > -1)
                close_stepperdev(fd);
            else
                printf("%s has not opened yet! Can not close\n", DEVICE_NODE);
            fd = -1;
            break;
        case 'q':
            if(fd > -1)
                close_stepperdev(fd);
            printf("Quit the application. Good bye!\n");
            return 0;
        case 'm':
            print_menu();
            break;
        case 'S':
            control_stop_motors();
            break;

        case 'g':
        {
            struct motor_message_t msg;
            control_get_status(&msg);
            break;
        }
        case 'M':
        {
            struct motors_steps_t dst;
            scanf(" %d", &dst.x);
            scanf(" %d", &dst.y);
            control_motor_move(&dst);
            break;
        }
        case 'w':
        {
            struct motors_steps_t dst;
            dst.x = 0;
            dst.y = 30;
            control_motor_move(&dst);
            break;
        }
        case 's':
        {
            struct motors_steps_t dst;
            dst.x = 0;
            dst.y = -30;
            control_motor_move(&dst);
            break;
        }
        case 'd':
        {
            struct motors_steps_t dst;
            dst.x = -30;
            dst.y = 0;
            control_motor_move(&dst);
            break;
        }
        case 'a':
        {
            struct motors_steps_t dst;
            dst.x = 30;
            dst.y = 0;
            control_motor_move(&dst);
            break;
        }
        case 'r':
            control_motor_reset();
            break;
        case 'v':
        {
            int speed;
            scanf("%d", &speed);
            control_motor_speed(&speed);
            break;
        }
        default:
            printf("Invalid option %c\n", option);
            break;
        }
    }
}
