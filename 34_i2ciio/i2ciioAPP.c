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

struct ap3216c_dev{
    int ir_raw_adc,als_raw_adc,ps_raw_adc;
    float als_scale_adc;
    float als_raw_act;
};
struct ap3216c_dev ap3216c;

static const float ap3216c_als_scale[] = {0.315262,0.078766,0.019699,0.004929};

char *file_path[] = {
    "/sys/bus/iio/devices/iio:device1/in_intensity_ir_raw",
    "/sys/bus/iio/devices/iio:device1/in_intensity_both_raw",
    "/sys/bus/iio/devices/iio:device1/in_proximity_raw",
    "/sys/bus/iio/devices/iio:device1/in_intensity_both_scale",
};
enum {
    IR_RAW,
    ALS_RAW,
    PS_RAW,
    ALS_SCALE
};

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
int write_file_data(char *filepath,float fdata)
{
    int ret = 0;
    FILE *fp = fopen(filepath,"w");
    if(NULL == fp)
    {
        printf("fopen %s error!\n",filepath);
        return -1;
    }
    ret = fprintf(fp,"%.6f",fdata);
    if(ret <= 0)
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

int read_sensor(void)
{
    int ret = 0;
    char str[50];

    GET_SENSOR_INT_DATA(ret,file_path[IR_RAW],str,ap3216c.ir_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[ALS_RAW],str,ap3216c.als_raw_adc);
    GET_SENSOR_INT_DATA(ret,file_path[PS_RAW],str,ap3216c.ps_raw_adc);
    GET_SENSOR_FLOAT_DATA(ret,file_path[ALS_SCALE],str,ap3216c.als_scale_adc);

    ap3216c.als_raw_act = ap3216c.als_raw_adc * ap3216c.als_scale_adc;

    return 0;
}

int show_als_scale(void)
{
    int ret = 0;
    char str[50];

    GET_SENSOR_FLOAT_DATA(ret,file_path[ALS_SCALE],str,ap3216c.als_scale_adc);
    printf("als scale = %.6f!\n",ap3216c.als_scale_adc);

    return 0;
}
int write_als_scale(void)
{
    int ret = 0;

    ret = write_file_data(file_path[ALS_SCALE],ap3216c_als_scale[1]);
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
    ret = show_als_scale();
    if(ret < 0)
        exit(-3);
    printf("写入分辨率中...\n");
    ret = write_als_scale();
    if(ret < 0)
        exit(-4);
    printf("初始后分辨率:\n");
    ret = show_als_scale();
    if(ret < 0)
        exit(-5);

    while(1)
    {
        ret = read_sensor();
        if(ret < 0)
        {
            printf("read_sensor error!\n");
            exit(-2);
        }
        else if(ret == 0)
        {
            printf("\n原始值:\n");
            printf("adc ir = %d,als = %d,ps = %d!\n",ap3216c.ir_raw_adc,ap3216c.als_raw_adc,ap3216c.ps_raw_adc);
            printf("实际值:\n");
			printf("act als = %.2flux!\n", ap3216c.als_raw_act);
        }
        usleep(200000);
    }

    return 0;
}