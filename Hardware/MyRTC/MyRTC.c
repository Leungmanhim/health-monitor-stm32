/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "stm32f10x.h"                  // Device header
#include <time.h>
#include "systick.h"
#include "sys.h"
#include <MyRTC.h>
#include "oled.h"
uint16_t MyRTC_Time[] = {2024, 10, 28, 12, 52, 40};	//定义全局的时间数组，数组内容分别为年、月、日、时、分、秒

void MyRTC_SetTime(void);				//函数声明

/**
  * 函    数：RTC初始化
  * 参    数：无
  * 返 回 值：无
  */
void MyRTC_Init(void)
{
	uint16_t timeout;
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);		//开启PWR的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);		//开启BKP的时钟
	
	/*备份寄存器访问使能*/
	PWR_BackupAccessCmd(ENABLE);							//使用PWR开启对备份寄存器的访问
	
	if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)			//通过写入备份寄存器的标志位，判断RTC是否是第一次配置
															//if成立则执行第一次的RTC配置
	{
		RCC_LSEConfig(RCC_LSE_ON);							//开启LSE时钟
		/* 原来这里是死等LSE起振，没有晶振或起振失败会一直卡在开机，
		   这里加约2秒超时，超时后自动改用LSI，保证系统能正常启动 */
		timeout = 0;
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != SET && ++timeout < 2000)
			Delay_ms(1);

		if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == SET)		//LSE起振成功，走原流程
		{
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);			//选择RTCCLK来源为LSE
			RCC_RTCCLKCmd(ENABLE);							//RTCCLK使能
			
			RTC_WaitForSynchro();							//等待同步
			RTC_WaitForLastTask();							//等待上一次操作完成
			
			RTC_SetPrescaler(32768 - 1);					//设置RTC预分频器，预分频后的计数频率为1Hz
			RTC_WaitForLastTask();							//等待上一次操作完成
		}
		else												//LSE起振失败，改用内部LSI
		{
			RCC_LSEConfig(RCC_LSE_OFF);
			RCC_LSICmd(ENABLE);
			timeout = 0;
			while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET && ++timeout < 1000)
				Delay_ms(1);

			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
			RCC_RTCCLKCmd(ENABLE);
			
			RTC_WaitForSynchro();
			RTC_WaitForLastTask();
			
			RTC_SetPrescaler(40000 - 1);					//LSI约40kHz
			RTC_WaitForLastTask();
		}
		
		MyRTC_SetTime();									//设置时间，调用此函数，全局数组里时间值刷新到RTC硬件电路
		
		BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);			//在备份寄存器写入自己规定的标志位，用于判断RTC是不是第一次执行配置
	}
	else													//RTC不是第一次配置
	{
		
		RTC_WaitForSynchro();								//等待同步
		RTC_WaitForLastTask();								//等待上一次操作完成
	}
}

//如果LSE无法起振导致程序卡死在初始化函数中
//可将初始化函数替换为下述代码，使用LSI当作RTCCLK
//LSI无法由备用电源供电，故主电源掉电时，RTC走时会暂停
/* 
void MyRTC_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);
	
	PWR_BackupAccessCmd(ENABLE);
	
	if (BKP_ReadBackupRegister(BKP_DR1) != 0xA5A5)
	{
		RCC_LSICmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET);
		
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);
		
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
		
		RTC_SetPrescaler(40000 - 1);
		RTC_WaitForLastTask();
		
		MyRTC_SetTime();
		
		BKP_WriteBackupRegister(BKP_DR1, 0xA5A5);
	}
	else
	{
		RCC_LSICmd(ENABLE);				//即使不是第一次配置，也需要再次开启LSI时钟
		while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET);
		
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		RCC_RTCCLKCmd(ENABLE);
		
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
	}
}*/

/**
  * 函    数：RTC设置时间
  * 参    数：无
  * 返 回 值：无
  * 说    明：调用此函数后，全局数组里时间值将刷新到RTC硬件电路
  */
/* lightweight calendar helpers - replace mktime()/localtime() to save flash */
static long my_days_from_civil(long y, unsigned m, unsigned d)
{
    long era; unsigned yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2 ? 3u : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097L + (long)doe - 719468L;
}
static void my_civil_from_days(long z, long *year, unsigned *mon, unsigned *day)
{
    long era; unsigned yoe, doy, mp; long y;
    z += 719468L;
    era = (z >= 0 ? z : z - 146096L) / 146097L;
    doy = (unsigned)(z - era * 146097L);
    yoe = (doy - doy / 1460u + doy / 36524u - doy / 146096u) / 365u;
    y = (long)yoe + era * 400L;
    doy = doy - (365u * yoe + yoe / 4u - yoe / 100u);
    mp = (5u * doy + 2u) / 153u;
    *day = doy - (153u * mp + 2u) / 5u + 1u;
    *mon = mp < 10 ? mp + 3 : mp - 9;
    *year = y + (*mon <= 2);
}
static long my_mktime(const struct tm *t)
{
    long days = my_days_from_civil((long)t->tm_year + 1900L, (unsigned)t->tm_mon + 1u, (unsigned)t->tm_mday);
    return days * 86400L + (long)t->tm_hour * 3600L + (long)t->tm_min * 60L + (long)t->tm_sec;
}
static void my_localtime(time_t t, struct tm *out)
{
    long days = t / 86400L;
    long rem = t - days * 86400L;
    long year; unsigned mon, day;
    my_civil_from_days(days, &year, &mon, &day);
    out->tm_year = (int)year - 1900;
    out->tm_mon = (int)mon - 1;
    out->tm_mday = (int)day;
    out->tm_hour = (int)(rem / 3600L);
    out->tm_min = (int)((rem % 3600L) / 60L);
    out->tm_sec = (int)(rem % 60L);
}

void MyRTC_SetTime(void)
{
	time_t time_cnt;		//定义秒计数器数据类型
	struct tm time_date;	//定义日期时间数据类型
	
	time_date.tm_year = MyRTC_Time[0] - 1900;		//将数组的时间赋值给日期时间结构体
	time_date.tm_mon = MyRTC_Time[1] - 1;
	time_date.tm_mday = MyRTC_Time[2];
	time_date.tm_hour = MyRTC_Time[3];
	time_date.tm_min = MyRTC_Time[4];
	time_date.tm_sec = MyRTC_Time[5];
	
	time_cnt = my_mktime(&time_date) - 8 * 60 * 60;	//调用mktime函数，将日期时间转换为秒计数器格式
													//- 8 * 60 * 60为东八区的时区调整
	
	RTC_SetCounter(time_cnt);						//将秒计数器写入到RTC的CNT中
	RTC_WaitForLastTask();							//等待上一次操作完成
}

/**
  * 函    数：RTC读取时间
  * 参    数：无
  * 返 回 值：无
  * 说    明：调用此函数后，RTC硬件电路里时间值将刷新到全局数组
  */
void MyRTC_ReadTime(void)
{
	time_t time_cnt;		//定义秒计数器数据类型
	struct tm time_date;	//定义日期时间数据类型
	
	time_cnt = RTC_GetCounter() + 8 * 60 * 60;		//读取RTC的CNT，获取当前的秒计数器
													//+ 8 * 60 * 60为东八区的时区调整
	
	my_localtime(time_cnt, &time_date);				//使用localtime函数，将秒计数器转换为日期时间格式
	
	MyRTC_Time[0] = time_date.tm_year + 1900;		//将日期时间结构体赋值给数组的时间
	MyRTC_Time[1] = time_date.tm_mon + 1;
	MyRTC_Time[2] = time_date.tm_mday;
	MyRTC_Time[3] = time_date.tm_hour;
	MyRTC_Time[4] = time_date.tm_min;
	MyRTC_Time[5] = time_date.tm_sec;
}
u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec)
{
	time_t time_cnt;		//定义秒计数器数据类型
	struct tm time_date;	//定义日期时间数据类型
	
	time_date.tm_year = syear - 1900;		//将数组的时间赋值给日期时间结构体
	time_date.tm_mon = smon - 1;
	time_date.tm_mday = sday;
	time_date.tm_hour = hour;
	time_date.tm_min = min;
	time_date.tm_sec = sec;
	
	time_cnt = my_mktime(&time_date) - 8 * 60 * 60;	//调用mktime函数，将日期时间转换为秒计数器格式
													//- 8 * 60 * 60为东八区的时区调整
	
	RTC_SetCounter(time_cnt);						//将秒计数器写入到RTC的CNT中
	RTC_WaitForLastTask();							//等待上一次操作完成
	return 0;	    
}
void Time_Disp(void)
{
		OLED_ShowNum(0,2,MyRTC_Time[0],4,16);
//		OLED_ShowCHinese(32,0,5);
		OLED_ShowString(32,2,(u8*)"-",16);
		OLED_ShowNum(40,2,MyRTC_Time[1],2,16);
		OLED_ShowString(56,2,(u8*)"-",16);
		OLED_ShowNum(64,2,MyRTC_Time[2],2,16);
//		OLED_ShowString(96,0," ",16);
		OLED_ShowNum(0,4,MyRTC_Time[3],2,16);
		OLED_ShowString(16,4,(u8*)":",16);
		OLED_ShowNum(24,4,MyRTC_Time[4],2,16);
		OLED_ShowString(40,4,(u8*)":",16);
		OLED_ShowNum(48,4,MyRTC_Time[5],2,16);
}