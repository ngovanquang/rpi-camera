#include "ptz_control.h"

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "logger.h"

static int open_ptzdevice(void)
{
    int fd = open(DEVICE_NODE, O_RDWR);
    if (fd < 0)
    {
        LOG_ERROR("Can't open the device file: %s", DEVICE_NODE);
    }
    return fd;
}

static void close_ptzdevice(int fd)
{
    close(fd);
}

void control_ptz_stop(void)
{
    int fd = open_ptzdevice();
    int ret = ioctl(fd, MOTOR_STOP);
    close_ptzdevice(fd);
}

void control_ptz_reset(void)
{
    int fd = open_ptzdevice();
    if (fd < 0) return;
    int ret = ioctl(fd, MOTOR_RESET);
    close_ptzdevice(fd);
}

void control_ptz_get_status(struct motor_message_t *msg)
{
    int fd = open_ptzdevice();
    if (fd < 0) return;
    int ret = ioctl(fd, MOTOR_GET_STATUS, msg);
    if (ret >= 0)
    {
        printf("+++++++++++++++++++++++++++++\n");
        printf("motor status:\n");
        printf("\tspeed: %d\n", msg->speed);
        printf("\tstatus: %s\n", msg->status == MOTOR_IS_RUNNING ? "Running" : "Stopped");
        printf("\tx: %d\n", msg->x);
        printf("\ty: %d\n", msg->y);
        printf("+++++++++++++++++++++++++++++\n");
    }
    close_ptzdevice(fd);
}

void control_ptz_move(struct motors_steps_t *relative_position)
{
    int fd = open_ptzdevice();
    if (fd < 0) return;
    int ret = ioctl(fd, MOTOR_MOVE, relative_position);
    close_ptzdevice(fd);
}

void control_ptz_speed(int *speed)
{
    int fd = open_ptzdevice();
    if (fd < 0) return;
    int ret = ioctl(fd, MOTOR_SPEED, speed);
    close_ptzdevice(fd);
}

static bool validate_control_string(const char *cmd_string, int len)
{
    if (len < (sizeof(MAGIC_NUMBER) + 1))
    {
        printf("LEN FAILED\n");
        return false;
    }

    if (strncmp(cmd_string, MAGIC_NUMBER, 4))
    {
        printf("MAGIC FAILED\n");
        return false;
    }

    if (cmd_string[4] < MOTOR_STOP || cmd_string[4] > MOTOR_RESET)
    {
        printf("COMMAND FAILED\n");
        return false;

    }

    return true;
}

int handle_ptz_control_message(const char *cmd, int len)
{
    if (!validate_control_string(cmd, len))
    {
        LOG_INFOR("ptz cmd is invalid!");
        return -1;
    }
    switch (cmd[4])
    {
    case MOTOR_MOVE:
    {
        int relative_x;
        int relative_y;

        sscanf(cmd + 5, "%s %s", &relative_x, &relative_y);
        relative_x = atoi((char *)&relative_x);
        relative_y = atoi((char *)&relative_y);
        motors_steps_t dst = {
            .x = relative_x,
            .y = relative_y,
        };
        control_ptz_move(&dst);
        break;
    }
    case MOTOR_RESET:
    {
        control_ptz_reset();
    }

    default:
        break;
    }
    // LOG_INFOR("ptz cmd is valid");
}