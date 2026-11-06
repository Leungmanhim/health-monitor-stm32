#ifndef _GPS_H
#define _GPS_H
#include "sys.h"
//#include "delay.h"


#define SBUFF 73
#define DATA 50
extern double lon;//经度
extern double lat;//纬度
extern u8 gps_fix;		//1=已定位(经纬度有效)
extern u8 gps_sats;		//卫星数量
extern u8 gps_quality;	//GGA定位质量 0=未定位
extern u8 gps_alive;	//是否收到过GPS语句
extern u8 gps_demo;		//1=演示定位（APP下发坐标）
void GPS_SetDemo(double lo, double la);	//设置演示坐标
void GPS_ClearDemo(void);				//清除演示坐标
void USART3_Init(u32 bound);
void my_printf3(char *fmt,...);
int my_read3(char *temp,int time);
void display_lon_lat(void);
int data_processing(char a);
void time_data(char *data);
void lon_data(char *data);
void lat_data(char *data);
void satellite_nums(char *data);
void Page_2(void);
#endif



