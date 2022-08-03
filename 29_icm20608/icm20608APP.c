#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>
#include <linux/input.h>

int main(int argc,char *argv[])
{
    int fd = 0;
    int ret = 0;

    short data[7];

	short gyro_x_adc, gyro_y_adc, gyro_z_adc;
	short accel_x_adc, accel_y_adc, accel_z_adc;
	short temp_adc;

	float gyro_x_act, gyro_y_act, gyro_z_act;
	float accel_x_act, accel_y_act, accel_z_act;
	float temp_act;

    if(argc != 2)
    {
        printf("usage error!\n");
        exit(-1);
    }

    fd = open(argv[1],O_RDWR);
    if(fd < 0)
    {
        printf("open file %s error!\n",argv[1]);
        exit(-1);
    }

    while (1) {
		ret = read(fd, data, sizeof(data));
		if(ret == 0) { 			/* 数据读取成功 */
            accel_x_adc = data[0];
			accel_y_adc = data[1];
			accel_z_adc = data[2];
			gyro_x_adc = data[3];
			gyro_y_adc = data[4];
			gyro_z_adc = data[5];
			temp_adc = data[6];

			/* 计算实际值 */
			gyro_x_act = (float)(gyro_x_adc)  / 16.4;
			gyro_y_act = (float)(gyro_y_adc)  / 16.4;
			gyro_z_act = (float)(gyro_z_adc)  / 16.4;
			accel_x_act = (float)(accel_x_adc) / 2048;
			accel_y_act = (float)(accel_y_adc) / 2048;
			accel_z_act = (float)(accel_z_adc) / 2048;
			temp_act = ((float)(temp_adc) - 25 ) / 326.8 + 25;

			printf("\n原始值:\n");
			printf("adc gx = %d, gy = %d, gz = %d!\n", gyro_x_adc, gyro_y_adc, gyro_z_adc);
			printf("adc ax = %d, ay = %d, az = %d!\n", accel_x_adc, accel_y_adc, accel_z_adc);
			printf("adc temp = %d!\n", temp_adc);
			printf("实际值:\n");
			printf("act gx = %.2f°/S, gy = %.2f°/S, gz = %.2f°/S!\n", gyro_x_act, gyro_y_act, gyro_z_act);
			printf("act ax = %.2fg, ay = %.2fg, az = %.2fg!\n", accel_x_act, accel_y_act, accel_z_act);
			printf("act temp = %.2f°C!\n", temp_act);
		}
		usleep(200000); /*200ms */
	}

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s error!\n",argv[1]);
        exit(-1);
    }
    return 0;
}