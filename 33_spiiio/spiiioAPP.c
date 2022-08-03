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

#define GET_SENSOR_INT_DATA(ret,filepath,str,idata);\
    ret = read_file_data(filepath,str);\
    if(ret < 0)\
        return ret;\
    idata = atoi(str);
#define GET_SENSOR_FLOAT_DATA(ret,filepath,str,fdata);\
    ret = read_file_data(filepath,str);\
    if(ret < 0)\
        return ret;\
    fdata = atof(str);

static const float icm20608_gyro_scale[] = {0.007629, 0.015259, 0.030518, 0.061035};
static const double icm20608_accel_scale[] = {0.000061035, 0.000122070, 0.000244141, 0.000488281};

char *file_path[] = {
    "/sys/bus/iio/devices/iio:device1/in_accel_scale",
    "/sys/bus/iio/devices/iio:device1/in_accel_x_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_accel_x_raw",
    "/sys/bus/iio/devices/iio:device1/in_accel_y_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_accel_y_raw",
    "/sys/bus/iio/devices/iio:device1/in_accel_z_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_accel_z_raw",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_scale",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_x_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_x_raw",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_y_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_y_raw",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_z_calibbias",
    "/sys/bus/iio/devices/iio:device1/in_anglvel_z_raw",
    "/sys/bus/iio/devices/iio:device1/in_temp_offset",
    "/sys/bus/iio/devices/iio:device1/in_temp_raw",
    "/sys/bus/iio/devices/iio:device1/in_temp_scale"
};
enum {
    ACCEL_SCALE,
    ACCEL_X_CALIBBIAS,
    ACCEL_X_RAW,
    ACCEL_Y_CALIBBIAS,
    ACCEL_Y_RAW,
    ACCEL_Z_CALIBBIAS,
    ACCEL_Z_RAW,
    ANGLVEL_SCALE,
    ANGLVEL_X_CALIBBIAS,
    ANGLVEL_X_RAW,
    ANGLVEL_Y_CALIBBIAS,
    ANGLVEL_Y_RAW,
    ANGLVEL_Z_CALIBBIAS,
    ANGLVEL_Z_RAW,
    TEMP_OFFSET,
    TEMP_RAW,
    TEMP_SCALE
};

struct icm20608_dev{
    double accel_scale_adc;
    int accel_x_raw_adc,accel_y_raw_adc,accel_z_raw_adc;
    int accel_x_cal_adc,accel_y_cal_adc,accel_z_cal_adc;

    float gyro_scale_adc;
    int gyro_x_raw_adc,gyro_y_raw_adc,gyro_z_raw_adc;
    int gyro_x_cal_adc,gyro_y_cal_adc,gyro_z_cal_adc;

    float temp_scale_adc;
    int temp_offset_adc,temp_raw_adc;

    float accel_x_raw_act,accel_y_raw_act,accel_z_raw_act;
    float gyro_x_raw_act,gyro_y_raw_act,gyro_z_raw_act;
    float temp_raw_act;
};
struct icm20608_dev icm20608;

int read_file_data(char *filepath,char *str)
{
    int ret = 0;
    FILE *fp = fopen(filepath,"r");
    if(NULL == fp)
    {
        printf("fopen %s error!\n",filepath);
        return -1;
    }
    ret = fscanf(fp,"%s",str);
    if(ret != 1)
    {
        printf("fscanf error!\n");
        return -2;
    }
    ret = fclose(fp);
    if(ret != 0)
    {
        printf("fclose %s error!\n",filepath);
        return -3;
    }
    return 0;
}
int write_gyro_file_data(char *filepath,float fdata)
{
    int ret = 0;
    FILE *fp = fopen(filepath,"w");
    if(NULL == fp)
    {
        printf("fopen %s error!\n",filepath);
        return -1;
    }
    ret = fprintf(fp,"%.6f",fdata);
    if(ret < 1)
    {
        printf("fprintf error!\n");
        return -2;
    }
    ret = fclose(fp);
    if(ret != 0)
    {
        printf("fclose %s error!\n",filepath);
        return -3;
    }
    return 0;
}
int write_accel_file_data(char *filepath,double fdata)
{
    int ret = 0;
    FILE *fp = fopen(filepath,"w");
    if(NULL == fp)
    {
        printf("fopen %s error!\n",filepath);
        return -1;
    }
    ret = fprintf(fp,"%.9lf",fdata);
    if(ret <= 0)
    {
        printf("fprintf error!\n");
        return -2;
    }
    ret = fclose(fp);
    if(ret != 0)
    {
        printf("fclose %s error!\n",filepath);
        return -3;
    }
    return 0;
}

int read_sensor(void)
{
    int ret = 0;
    char str[50];

    GET_SENSOR_FLOAT_DATA(ret,file_path[ACCEL_SCALE],str,icm20608.accel_scale_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_X_RAW],str,icm20608.accel_x_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_Y_RAW],str,icm20608.accel_y_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_Z_RAW],str,icm20608.accel_z_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_X_CALIBBIAS],str,icm20608.accel_x_cal_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_Y_CALIBBIAS],str,icm20608.accel_y_cal_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ACCEL_Z_CALIBBIAS],str,icm20608.accel_z_cal_adc);

    GET_SENSOR_FLOAT_DATA(ret,file_path[ANGLVEL_SCALE],str,icm20608.gyro_scale_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_X_RAW],str,icm20608.gyro_x_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_Y_RAW],str,icm20608.gyro_y_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_Z_RAW],str,icm20608.gyro_z_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_X_CALIBBIAS],str,icm20608.gyro_x_cal_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_Y_CALIBBIAS],str,icm20608.gyro_y_cal_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ANGLVEL_Z_CALIBBIAS],str,icm20608.gyro_z_cal_adc);

    GET_SENSOR_FLOAT_DATA(ret,file_path[TEMP_SCALE],str,icm20608.temp_scale_adc);
    GET_SENSOR_INT_DATA(ret,file_path[TEMP_OFFSET],str,icm20608.temp_offset_adc);
    GET_SENSOR_INT_DATA(ret,file_path[TEMP_RAW],str,icm20608.temp_raw_adc);

    icm20608.accel_x_raw_act = icm20608.accel_x_raw_adc * icm20608.accel_scale_adc;
    icm20608.accel_y_raw_act = icm20608.accel_y_raw_adc * icm20608.accel_scale_adc;
    icm20608.accel_z_raw_act = icm20608.accel_z_raw_adc * icm20608.accel_scale_adc;

    icm20608.gyro_x_raw_act = icm20608.gyro_x_raw_adc * icm20608.gyro_scale_adc;
    icm20608.gyro_y_raw_act = icm20608.gyro_y_raw_adc * icm20608.gyro_scale_adc;
    icm20608.gyro_z_raw_act = icm20608.gyro_z_raw_adc * icm20608.gyro_scale_adc;

    icm20608.temp_raw_act = (icm20608.temp_raw_adc - icm20608.temp_offset_adc) / icm20608.temp_scale_adc + 25;

    return 0;
}

int show_sensor_scale(void)
{
    int ret = 0;
    char str[50];

    GET_SENSOR_FLOAT_DATA(ret,file_path[ACCEL_SCALE],str,icm20608.accel_scale_adc);
    GET_SENSOR_FLOAT_DATA(ret,file_path[ANGLVEL_SCALE],str,icm20608.gyro_scale_adc);

    printf("as = %.9lf!\n",icm20608.accel_scale_adc);
    printf("gs = %.6f!\n",icm20608.gyro_scale_adc);
    return 0;
}
int write_sensor_scale(void)
{
    int ret = 0;

    ret = write_accel_file_data(file_path[ACCEL_SCALE],icm20608_accel_scale[2]);
    if(ret < 0)
        return ret;
    
    ret = write_gyro_file_data(file_path[ANGLVEL_SCALE],icm20608_gyro_scale[2]);
    if(ret < 0)
        return ret;
    
    return 0;
}

int main(int argc,char *argv[])
{
    int ret = 0;
    if(argc != 1)
    {
        printf("usage error!\n");
        exit(-1);
    }

    printf("初始分辨率:\n");
    ret = show_sensor_scale();
    if(ret < 0)
        exit(-3);
    printf("写入分辨率中...\n");
    ret = write_sensor_scale();
    if(ret < 0)
        exit(-4);
    printf("初始后分辨率:\n");
    ret = show_sensor_scale();
    if(ret < 0)
        exit(-5);
    //printf("校准值:\n");
    //printf("cal gx = %d, gy = %d, gz = %d!\n",icm20608.gyro_x_cal_adc,icm20608.gyro_y_cal_adc,icm20608.gyro_z_cal_adc);
    //printf("cal ax = %d, ay = %d, az = %d!\n",icm20608.accel_x_cal_adc,icm20608.accel_y_cal_adc,icm20608.accel_z_cal_adc);

    while(1)
    {
        ret = read_sensor();
        if(ret < 0)
            exit(-2);
        else if(ret == 0)
        {
            printf("\n原始值:\n");
            printf("adc gx = %d, gy = %d, gz = %d!\n",icm20608.gyro_x_raw_adc,icm20608.gyro_y_raw_adc,icm20608.gyro_z_raw_adc);
            printf("adc ax = %d, ay = %d, az = %d!\n",icm20608.accel_x_raw_adc,icm20608.accel_y_raw_adc,icm20608.accel_z_raw_adc);
            printf("adc temp = %d!\n",icm20608.temp_raw_adc);
            printf("实际值:\n");
            printf("act gx = %.2f°/S, gy = %.2f°/S, gz = %.2f°/S!\n",icm20608.gyro_x_raw_act,icm20608.gyro_y_raw_act,icm20608.gyro_z_raw_act);
            printf("act ax = %.2fg, ay = %.2fg, az = %.2fg!\n",icm20608.accel_x_raw_act,icm20608.accel_y_raw_act,icm20608.accel_z_raw_act);
            printf("act temp = %.2f°C!\n",icm20608.temp_raw_act);
        }
        usleep(200000);
    }
    return 0;
}