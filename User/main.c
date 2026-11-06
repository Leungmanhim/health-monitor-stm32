/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "stm32f10x.h"
#include "systick.h"
#include "usart.h"
#include "oled.h"
#include "dht11.h"
#include "esp8266.h"
#include "string.h"
#include "mq4.h" //MQ4天然气传感器驱动库
#include "led.h"
#include "key.h"
#include "gps.h"
#include "mpu6050.h"
#include "dma.h"
#include "adc.h"
#include "timer.h"
#include "exti.h"
#include "ds18b20.h"
#include "max30102.h" 
#include "myiic.h"
#include "algorithm.h"
#include "Store.h"
#include <MyRTC.h>
#include "secrets.h"		//WiFi / 巴法云 uid（本文件不提交到仓库）
//声明
extern uint8_t Alarm_Hour,Alarm_Min;
u16 Sedentarysit_Sec=1800;
u8 sedentary=0;
u8 sedentary_alarm=0;		//久坐已提醒（粘滞标志，直到走动或按键才清零，用于上报APP）
/************心率参数*****************/
void OLED_Show(void);
void Finger_off(void);
#define PPG_WAVE_COUNT 	4
#define PPG_MIN_COUNT 	0X3FFFF
uint32_t aun_ir_buffer[500]; 		//IR数组,红外光原始数据缓存区
uint32_t aun_red_buffer[500];  	//Red数组,红光原始数据缓存区
int32_t n_ir_buffer_length;    	//数据长度
	
int32_t n_sp02; 						//血氧值
int32_t n_heart_rate;   		//心率值

int8_t ch_spo2_valid;   		//算法执行成功标志位
int8_t ch_hr_valid;    			
uint8_t SPO2_Ststus = Spo2_WorkMode;
u8 Packet[8];
u8 hrH=60,spo2H=95,mode,KeyNum;

uint32_t un_min, un_max; 	//原始数据的最大/最小值
uint32_t Wave_Range;		//用于显示计算倍率
u16 i;	
u8 buf_flag=0;

s8 Wave_sum;				//OLED波形大小
u8 temp[6];					//拼字节
u8 str[30];					//字符串显示变量
u8 dis_hr=0,dis_spo2=0,Count_hr_UpLim=200,Count_hr_LowLim=50,Count_spo2_UpLim=100,Count_spo2_LowLim=90;
u8 wave;
/************心率参数*****************/
void Title(void);
void process_commands(void);
void Page2_HeartRate();
short   temperature;//温度
#define TEMP_BASE_TENTHS  260		/* 上电起始显示温度：26.0℃（0.1℃单位） */
short   temp_raw;					/* DS18B20实测温度，0.1℃ */
short   temp_disp = TEMP_BASE_TENTHS;	/* 平滑后的显示温度，0.1℃ */
u8      Temp_Init_Flag = 0;		/* 上电首次显示初始化标志 */
void WIFIChoose_Page(void);
void Page_3(void);
void Page_4(void);
u16 Level_Check(void);

extern double lon;//经度
extern double lat;//纬度

//dht11添加变量
DHT11_Data_TypeDef DHT11_Data;
extern  DHT11_Data_TypeDef DHT11_Data;
u8 keynum;//按键值

extern		uint16_t ppm;
extern		uint16_t xx;
char data[192];
u8 times=0;		

 float Level;//水位
 u8 QX_Flag;
extern float pitch,roll,yaw; 		//欧拉角
extern uint16_t time_count;//更新中断计数
u8 MPU_flag=0,ZD_flag=0;
u16 Level_flag=50;
u8 SD_Time,SD_Flag;
u8 step=0,flag_step;//步数
u8 count_Temp=36;
u8 HeartRate_Flag=0;
u8 count_HeartRate=150;
extern volatile uint8_t alarm_active;
extern volatile unsigned char esp_status, esp_tx_count;
extern volatile u8 Step_Flag;
extern volatile u8 Page_Req;		//手机远程切页请求，0xFF=无
extern volatile u16 Sed_Req;		//手机直接设定久坐秒数，0=无
extern volatile u8 Fall_Candidate;	//跌倒预判：检测到冲击后8秒内有效
u8 Alarm_Mute=0;		//按键消音标志：1=本次报警已静默，条件恢复后自动清零
u16 Send_Tick=0;		//最近一次数据上报时刻(ms)
u16 Heart_Tick=0;		//最近一次心率异常上报时刻(ms)
int main(void)  
{		
	static u16 last_temp_tick = 0;		/* 体温显示的刷新节拍(ms) */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	//中断控制器分组设置
	Usart1_Init(9600);							//串口1，打印信息用
	Usart2_Init(115200);						//串口2，驱动ESP8266用
	USART3_Init(9600);							//串口3，GPS(ATGM336H)用
	Store_Init();				//参数存储模块初始化，在上电的时候将闪存的数据加载回Store_Data，实现掉电不丢失
	count_Temp=Store_Data[1];
	Sedentarysit_Sec=Store_Data[2];
	Alarm_Hour=Store_Data[3];   
	Alarm_Min=Store_Data[4];
	Count_hr_UpLim=Store_Data[5];
	Count_hr_LowLim=Store_Data[6];
	Count_spo2_UpLim=Store_Data[7];
	Count_spo2_LowLim=Store_Data[8];
	/* 首次上电时Flash内数据为0，会导致报警阈值非法（蜂鸣器常响、久坐秒触发），
	   这里做一次合法性检查，非法则恢复默认值 */
	if(count_Temp<20||count_Temp>60)               count_Temp=38;
	if(Sedentarysit_Sec<10||Sedentarysit_Sec>7200)   Sedentarysit_Sec=1800;	//久坐提醒默认30分钟，最小10秒便于演示
	if(Alarm_Hour>23)                              Alarm_Hour=0;
	if(Alarm_Min>59)                               Alarm_Min=0;
	if(Count_hr_UpLim<60||Count_hr_UpLim>250)       Count_hr_UpLim=200;
	if(Count_hr_LowLim<20||Count_hr_LowLim>120)     Count_hr_LowLim=30;
	if(Count_spo2_UpLim<80||Count_spo2_UpLim>100)   Count_spo2_UpLim=100;
	if(Count_spo2_LowLim<50||Count_spo2_LowLim>99)  Count_spo2_LowLim=90;
	BEEP_Init();
	LED_Init();			//报警指示灯(PB0)，默认熄灭
	//  BEEP=0;//开机关闭蜂鸣器
	//  LED=0; //开机关闭灯光
	Key_Init();
	//IIC_Init();
	//  DHT11_Init();           //DHT11初始化 

	TIM2_Init(10-1,7200-1);//编码器+按键扫描
	max30102_init();
	n_ir_buffer_length=500; 	//更新数组长度
	Adc_Init();
	DS18B20_Init();       //温度传感器初始化 
	MPU6050_Init();//陀螺仪初始化
	MPU6050_EXTI_Init();
	MyRTC_Init();
	OLED_Init();
	OLED_Clear(0);
 do {
			keynum=KEY_Scan(0);
			WIFIChoose_Page();
			if(keynum==1)//按键1
			{
				// 执行联网操作
				OLED_Clear(0); 
				Title();
				ESP8266_Init();
				Delay_ms(1500);		//让联网结果显示1.5秒
				OLED_Clear(0);		//清掉状态文字，避免残留在主界面
			//	WIFIUpdateFlag=1;//允许ESP8266数据上传标志
				break; // 退出循环
			} 
			else if (keynum ==2)//按键2
			{
				OLED_Clear(0); // 不执行任何操作，直接退出循环
				break; // 退出循环
			}
			else 
			{
			}
		} while (1); // 一直循环直到用户选择了1或2


	while(1)
	{		
		MyRTC_ReadTime();							//RTC读取时间，最新的时间存储到MyRTC_Time数组中
		display_lon_lat();
		if(sedentary==1)
		{
			StartAlarm(5); 
	//    /*******************向巴法云发送数据***************************************/
		xsnprintf(data,"cmd=2&uid=" BEMFA_UID "&topic=" BEMFA_TOPIC "&msg=remind\r\n");
		ESP8266_SendData((unsigned char *)data);
		sedentary=0;//清除标志
		}

		temp_raw = (short)DS18B20_Get_Temp();
		/* 体温显示：上电先显示26.0℃(室温)，之后直接跟随DS18B20实测值——每250ms刷新一次，
		   握住探头时跟着体温快速上升（走剩余差值的一半，最多0.3℃）；
		   松开后按环境温度缓慢回落（每次最多0.2℃），像真实体温计自然降到室温 */
		if(Temp_Init_Flag == 0)
		{
			temp_disp     = TEMP_BASE_TENTHS;
			Temp_Init_Flag = 1;
			last_temp_tick = time_count;
		}
		if(temp_raw > 0 && (uint16_t)(time_count - last_temp_tick) >= 250)
		{
			short target = temp_raw;					/* 显示跟随实测温度 */
			last_temp_tick = time_count;				/* 传感器无应答时(temp_raw<=0)保持原显示，不会掉到0℃ */

			if(target > temp_disp)
			{
				short d = target - temp_disp;
				short st = d/2;
				if(st < 1)  st = 1;
				if(st > 3)  st = 3;
				temp_disp += st;
			}
			else if(target < temp_disp)
			{
				short d = temp_disp - target;
				short st = d/50;
				if(st < 1) st = 1;
				if(st > 2) st = 2;
				temp_disp -= st;
			}
		}
		temperature = temp_disp;

		if(MPU_flag==1)//MPU中断标志位
		{
			MPU_Data();
			MPU_flag=0;
		}

		/* 计步统一在主循环处理：以前放在主界面显示函数里，
		   一换到其他页面（GPS/阈值/时间）就不再累加步数了 */
		if(Step_Flag)
		{
			Step_Flag=0;
			step++;
		}
			process_commands();
		
		/* 跌倒或超温：交给StartAlarm统一驱动蜂鸣器节奏。
		   原来这里直接BEEP_ON/BEEP_OFF，会和定时器里的报警节奏互相覆盖，导致报警没声音 */
		if(SD_Flag==1||(uint16_t)temperature/10>=count_Temp)
		{
			if(!alarm_active && !Alarm_Mute) StartAlarm(5);	//报警结束后条件仍在则继续响，除非被按键消音
		}
		else if(!alarm_active)
		{
			ALARM_OFF();
		}
		if ((dis_hr > Count_hr_UpLim || dis_hr < Count_hr_LowLim ||
			dis_spo2 > Count_spo2_UpLim || dis_spo2 < Count_spo2_LowLim) &&
			dis_hr != 0 && dis_spo2 != 0)
		{
			if(!alarm_active && !Alarm_Mute) StartAlarm(5);	//报警期间不重复触发，除非被按键消音
			if((uint16_t)(time_count-Heart_Tick)>=10000)	//10秒内最多上报一次，避免阻塞主循环
			{
				Heart_Tick=time_count;
				xsnprintf(data,"cmd=2&uid=" BEMFA_UID "&topic=" BEMFA_TOPIC "&msg=heart\r\n");
				ESP8266_SendData((unsigned char *)data);
			}
		}
		keynum=KEY_Scan(0);

		/* 报警响的时候按任意键立即消音并进入静默；等跌倒/超温/心率血氧条件恢复后自动解除静默 */
		if(keynum!=0 && alarm_active)
		{
			alarm_active = 0;
			ALARM_OFF();
			Alarm_Mute = 1;
		}
		if(Alarm_Mute && SD_Flag==0 && ((uint16_t)temperature/10)<count_Temp &&
		   !(dis_hr && dis_spo2 &&
		     (dis_hr>Count_hr_UpLim || dis_hr<Count_hr_LowLim ||
		      dis_spo2>Count_spo2_UpLim || dis_spo2<Count_spo2_LowLim)))
		{
			Alarm_Mute = 0;			/* 条件都恢复了，下次再触发可以重新报警 */
		}

		/* 手机APP远程切换装置屏幕：收到 PAGE 命令后直接跳到对应页面 */
		if(Page_Req != 0xFF)
		{
			times = (Page_Req <= 4) ? Page_Req : 0;
			Page_Req = 0xFF;
			OLED_Clear(0);
		}

		/* 手机直接设定久坐阈值（演示用，可设成10秒） */
		if(Sed_Req != 0)
		{
			Sedentarysit_Sec = Sed_Req;
			Store_Data[2] = Sedentarysit_Sec;
			Store_Save();
			Sed_Req = 0;
		}

		if(keynum==1)//界面切换按键
		{  
		
			OLED_Clear(0);
			times++; //界面切换标志位                                           
			if(times==5){ times=0;}
		}
		if(times==0)
		{

			Page_1();
		
		}
		if(times==1)
		{

			Page2_HeartRate();

		}
		if(times==2)
		{
			Page_2();//GPS
		}
		if(times==3)
		{
			Page_3();//时间
		}
		
		if(times==4)
		{
			Page_4();//阈值设置
		}
		if(MyRTC_Time[3]==Alarm_Hour&&MyRTC_Time[4]==Alarm_Min&&MyRTC_Time[5]==0)//闹钟到达
		{
						StartAlarm(10);
		}

		 

		 
	//    /*******************向巴法云发送数据***************************************/
		int lon_i=(int)lon;
long lon_f=(long)((lon-lon_i)*1000000.0+0.5);
int lat_i=(int)lat;
long lat_f=(long)((lat-lat_i)*1000000.0+0.5);
if(lon_f>=1000000){lon_f=0;lon_i++;}
if(lat_f>=1000000){lat_f=0;lat_i++;}
xsnprintf(data,"cmd=2&uid=" BEMFA_UID "&topic=" BEMFA_TOPIC "&msg=#%d#%d#%d#%d.%d#%d#%d#%d#%d#%d#%d#%d#%d.%06ld#%d.%06ld#%d#\r\n",
		step,dis_hr,dis_spo2,temperature/10,temperature%10,SD_Flag,count_Temp,Sedentarysit_Sec,Count_hr_UpLim,Count_hr_LowLim,Count_spo2_UpLim,Count_spo2_LowLim,lon_i,lon_f,lat_i,lat_f,sedentary_alarm);
		ESP8266_SendData((unsigned char *)data);

	}
}
extern volatile uint8_t alarm_active;
extern volatile uint16_t alarm_duration;
extern volatile uint32_t alarm_timer;
extern volatile uint8_t beep_counter;
extern volatile uint8_t beep_pattern;
//TIM2中断函数
void TIM2_IRQHandler(void)   //TIM4中断
{
		static uint32_t sedentary_counter = 0;
		static uint32_t last_step_count = 0;
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) //检查指定的TIM中断发生与否:TIM 中断源 
		{
			time_count++;

			/* 每20ms触发一次MPU6050读取：INT引脚没接到PB5时也能正常计步和测跌倒 */
			if(time_count % 20 == 0) MPU_flag = 1;
			
			
			if (step == last_step_count)
        {
            sedentary_counter++;
            if (sedentary_counter >= Sedentarysit_Sec*1000) // 30秒 (假设定时器中断每1ms触发一次)
            {
                // 久坐提醒
								sedentary=1;
                sedentary_alarm=1;		//上报给APP的久坐标志，走动或按键后自动清零
                sedentary_counter = 0; // 重置计数器
            }
        }
        else
        {
            sedentary_counter = 0; // 如果步数增加，重置计数器
            last_step_count = step;
            sedentary_alarm = 0;   // 有走动就不再算久坐
        }
			if(keynum==1||keynum==2||keynum==3||keynum==4)
				{
					alarm_active=0;
					sedentary_counter = 0; // 如果步数增加，重置计数器
					last_step_count = step;
					sedentary_alarm = 0;
					ALARM_OFF();
				}
			
			if (alarm_active)
        {
						alarm_timer++;
           beep_counter++;
            
            // 创建一个更慢的、有节奏的蜂鸣模式
            switch (beep_pattern)
            {
                case 0:  // 短鸣
                    if (beep_counter <= 200) ALARM_ON();
                    else if (beep_counter <= 400) ALARM_OFF();
                    else {
                        beep_counter = 0;
                        beep_pattern = 1;
                    }
                    break;
                
                case 1:  // 短鸣
                    if (beep_counter <= 200) ALARM_ON();
                    else if (beep_counter <= 400) ALARM_OFF();
                    else {
                        beep_counter = 0;
                        beep_pattern = 2;
                    }
                    break;
                
                case 2:  // 长鸣
                    if (beep_counter <= 600) ALARM_ON();
                    else if (beep_counter <= 1000) ALARM_OFF();
                    else {
                        beep_counter = 0;
                        beep_pattern = 0;
                    }
                    break;
            }

   // 检查是否应该停止报警
            if (alarm_timer >= alarm_duration)
            {
                alarm_active = 0;
                ALARM_OFF();
            }
        }
			if(time_count%1000==0)		//每秒判断一次
			{
				/* 跌倒判据：先检测到冲击(Fall_Candidate)，之后持续倾倒6秒才算跌倒。
				   普通拿放不会产生冲击，因此不会误报 */
				if(QX_Flag==1 && Fall_Candidate)
				{
					SD_Time++;
				}
				else
				{
					SD_Time=0;			//没冲击或已恢复直立：重新计时
				}
				if(SD_Time>5)
				{
					SD_Flag=1;
					Fall_Candidate=0;
				}
				
			}
		}
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);  //清除TIMx的中断待处理位:TIM 中断源 
}
void EXTI9_5_IRQHandler(void)
{
u8 sta; 
	if(EXTI_GetITStatus(EXTI_Line5) != RESET)
		{
			MPU_flag=1;
          EXTI_ClearITPendingBit(EXTI_Line5);  
		}
		if(EXTI_GetITStatus(EXTI_Line6) != RESET)
		{
          EXTI_ClearITPendingBit(EXTI_Line6);  
		}
}
extern volatile Command_t command_flag;
extern volatile unsigned char command_count;
extern unsigned char esp8266_buf[];
void process_commands(void)
{
	Command_t current_command;
	unsigned char n, i;
	u8 changed = 0;

	/* 中断里可能连按了好几次，这里一次取走“指令 + 次数”，全部应用，
	   避免连击时后到的指令把前一条覆盖掉（这就是之前要连按好几下才动一下的原因） */
	__disable_irq();
	current_command = command_flag;
	n = command_count;
	command_flag = CMD_NONE;
	command_count = 0;
	__enable_irq();

	if(current_command == CMD_NONE || n == 0)
		return;

	for(i = 0; i < n; i++)
	{
		changed = 1;
		switch(current_command)
		{
		case CMD_TEMP_ADD:
				if(count_Temp < 60) count_Temp += 1;
				Store_Data[1] = count_Temp;
				break;
		case CMD_TEMP_SUB:
				if(count_Temp > 20) count_Temp -= 1;
				Store_Data[1] = count_Temp;
				break;
		case CMD_SED_ADD:			/* 久坐阈值：大范围按1分钟调，低于2分钟后按10秒细调 */
				Sedentarysit_Sec += (Sedentarysit_Sec < 120) ? 10 : 60;
				if(Sedentarysit_Sec > 7200) Sedentarysit_Sec = 7200;
				Store_Data[2] = Sedentarysit_Sec;
				break;
		case CMD_SED_SUB:
				Sedentarysit_Sec -= (Sedentarysit_Sec > 120) ? 60 : 10;
				if(Sedentarysit_Sec < 10) Sedentarysit_Sec = 10;
				Store_Data[2] = Sedentarysit_Sec;
				break;
		case CMD_SET_ALARM:
				Store_Data[3] = Alarm_Hour;
				Store_Data[4] = Alarm_Min;
				StartAlarm(1);		/* 收到闹钟设置后短鸣一声，方便确认已同步 */
				break;
		case CMD_HRUPlim_ADD:
				Count_hr_UpLim+=5;
				Store_Data[5] = Count_hr_UpLim;
				break;
		case CMD_HRUPlim_SUB:
				Count_hr_UpLim-=5;
				Store_Data[5] = Count_hr_UpLim;
				break;
		case CMD_HRLOWlim_ADD:
				Count_hr_LowLim+=5;
				Store_Data[6] = Count_hr_LowLim;
				break;
		case CMD_HRLOWlim_SUB:
				Count_hr_LowLim-=5;
				Store_Data[6] = Count_hr_LowLim;
				break;
		case CMD_SPO2UPlim_ADD:
				Count_spo2_UpLim++;
				Store_Data[7] = Count_spo2_UpLim;
				break;
		case CMD_SPO2UPlim_SUB:
				Count_spo2_UpLim--;
				Store_Data[7] = Count_spo2_UpLim;
				break;
		case CMD_SPO2LOWlim_ADD:
				Count_spo2_LowLim++;
				Store_Data[8] = Count_spo2_LowLim;
				break;
		case CMD_SPO2LOWlim_SUB:
				Count_spo2_LowLim--;
				Store_Data[8] = Count_spo2_LowLim;
				break;
		case CMD_REST_ALL:			//恢复出厂设置
				count_Temp=Store_Data[1]=38;
				Sedentarysit_Sec=Store_Data[2]=1800;
				Alarm_Hour=Store_Data[3]=00;
				Alarm_Min=Store_Data[4]=00;
				Count_hr_UpLim=Store_Data[5]=200;
				Count_hr_LowLim=Store_Data[6]=30;
				Count_spo2_UpLim=Store_Data[7]=99;
				Count_spo2_LowLim=Store_Data[8]=90;
				break;
		default:
				return;
		}
	}

	/* 只在有改动时写一次Flash，避免连击时反复擦写 */
	if(changed) Store_Save();
}

void OLED_Show(void)
{
	switch(mode)
	{
		case 0:
//				OLED_ShowChinese(16,0,0); //显示中文<心率 血氧>
//				OLED_ShowChinese(32,0,1);
//				OLED_ShowChinese(64,0,2);
//				OLED_ShowChinese(80,0,3);
			  break;
		
	  case 1:
//			  OLED_ShowChinese(0,0,0);
//		    OLED_ShowChinese(16,0,1);
		    OLED_ShowString(32,0,"hrH:",16);
		    OLED_ShowNum(64,0,hrH,3,16);   //心率hrH: 60
//			  OLED_ShowChinese(0,6,5);
//		    OLED_ShowChinese(16,6,6);
//		    OLED_ShowChinese(32,6,7);
//		    OLED_ShowChinese(48,6,8);      //阈值设置
			  break;
		
		case 2:
//			  OLED_ShowChinese(0,0,2);
//		    OLED_ShowChinese(16,0,3);
		    OLED_ShowString(32,0,"spo2H:",16);
		    OLED_ShowNum(80,0,spo2H,3,16);   //血氧spo2H: 95
		    
//			  OLED_ShowChinese(0,6,5);
//		    OLED_ShowChinese(16,6,6);
//		    OLED_ShowChinese(32,6,7);
//		    OLED_ShowChinese(48,6,8);      //阈值设置
			break;
	
	}
}
void Finger_off(void)
{
	if(aun_ir_buffer[499] > 5000 && SPO2_Ststus == Proximity)
	{	
		maxim_max30102_write_reg(REG_LED1_PA,0x30);   //Choose value for ~ 7mA for LED1  LED 0C
		maxim_max30102_write_reg(REG_LED2_PA,0x30);   // Choose value for ~ 7mA for LED2  IR 0D
		maxim_max30102_write_reg(REG_FIFO_CONFIG,0x0f);  //sample avg = 1, fifo rollover=-1, fifo almost full = 17 08
		maxim_max30102_write_reg(REG_SPO2_CONFIG,0x27);// SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS) 0A
		SPO2_Ststus = Spo2_WorkMode;
	}
	else if(aun_ir_buffer[499] < 2000 && SPO2_Ststus == Spo2_WorkMode)
	{
		maxim_max30102_write_reg(REG_LED1_PA,0x00);   //Choose value for ~ 7mA for LED1  LED 0C
		maxim_max30102_write_reg(REG_LED2_PA,0x19);   // Choose value for ~ 7mA for LED2  IR 0D
		maxim_max30102_write_reg(REG_SPO2_CONFIG,0x27);// SPO2_ADC range = 4096nA, SPO2 sample rate (100 Hz), LED pulseWidth (400uS) 0A
		maxim_max30102_write_reg(REG_FIFO_WR_PTR,0x00); //FIFO_WR_PTR[4:0] 04
		maxim_max30102_write_reg(REG_OVF_COUNTER,0x00);  //OVF_COUNTER[4:0]05
		maxim_max30102_write_reg(REG_FIFO_RD_PTR,0x00); //FIFO_RD_PTR[4:0] 06
		maxim_max30102_write_reg(REG_FIFO_CONFIG,0x00);  //sample avg = 1, fifo rollover=-1, fifo almost full = 17 08
		SPO2_Ststus = Proximity;
	}	
}
void Title(void)
{
	OLED_ShowCHinese(0,  0,110);//人
	OLED_ShowCHinese(16, 0,69); //体
	OLED_ShowCHinese(32, 0,111);//健
	OLED_ShowCHinese(48, 0,112);//康
	OLED_ShowCHinese(64, 0,56); //监
	OLED_ShowCHinese(80, 0,57); //测
	OLED_ShowCHinese(96, 0,19); //系
	OLED_ShowCHinese(112,0,20); //统
}
void WIFIChoose_Page(void)
{

	Title();
	OLED_ShowCHinese(8-8,  	3,72);//请选择是否联网
	OLED_ShowCHinese(26-8, 	3,73);//
	OLED_ShowCHinese(44-8, 	3,74);//
	OLED_ShowCHinese(62-8, 	3,75);//
	OLED_ShowCHinese(80-8, 	3,76);//
	OLED_ShowCHinese(98-8, 	3,77);//
	OLED_ShowCHinese(114-8, 3,78);//

	OLED_ShowString(16,		6,(u8*)"1.",16);
	OLED_ShowCHinese(32, 	6,75);//
	OLED_ShowString(64,		6,(u8*)"2.",16);
	OLED_ShowCHinese(80,	6,76);//

}

/* 画“℃”：16号ASCII字符C + 左上角4×4小圆圈。
   原来直接用汉字字库里的℃，字号和温度数字不统一、看着偏小，改用这个组合保证同高 */
static void OLED_ShowDegreeC(u8 x, u8 y)
{
	OLED_Set_Pos(x,   y);	OLED_WR_Byte(0x30, OLED_DATA);
	OLED_Set_Pos(x+1, y);	OLED_WR_Byte(0x78, OLED_DATA);
	OLED_Set_Pos(x+2, y);	OLED_WR_Byte(0x78, OLED_DATA);
	OLED_Set_Pos(x+3, y);	OLED_WR_Byte(0x30, OLED_DATA);
	OLED_ShowChar(x+4, y, 'C', 16);
}

void Page_1(void)
{
		/*******************OLED显示数据***************************************/

		Title();
		OLED_ShowCHinese(0, 4, 69);//体温
		OLED_ShowCHinese(16,4, 70);//
		OLED_ShowString(32,		4,(u8*)":",16);
		OLED_ShowNum(40,			4,temperature/10,2,16);  			//显示温度
		OLED_ShowString(56,		4,(u8*)".",16);
		OLED_ShowNum(64,			4,temperature%10,1,16);  			//显示温度
		OLED_ShowDegreeC(74,	4);								//℃符号

		OLED_ShowCHinese(0,	6,35);//状态
		OLED_ShowCHinese(16,6,36);//
		OLED_ShowString(32,	6,":",16); 
		if(SD_Flag==1)
		{
		OLED_ShowCHinese(40,6,81);//跌倒
		OLED_ShowCHinese(56,6,82);//
		}
		else
		{
		OLED_ShowCHinese(40,6,83);//跌倒
		OLED_ShowCHinese(56,6,84);//
		}

		/* 步数的累加已经放在主循环里（见 Step_Flag 处理），这里只负责显示 */

		OLED_ShowCHinese(0,	2,100);//步数
		OLED_ShowCHinese(16,2,101);//
		OLED_ShowChar(32,	2,':',16);
		OLED_ShowNum(42,	2,step,3,16);

		/* 联网状态指示：C:xx 表示云端已连并已发出xx个数据包，-:xx 表示尚未连上云端 */
		xsnprintf((char *)str, "%c%02d", esp_status==2?'C':'-', esp_tx_count%100);
		OLED_ShowString(96, 2, (u8 *)str, 8);
				 

        
}

void Page2_HeartRate(void)
{
		
    OLED_ShowCHinese(16,  0,6);//心率
    OLED_ShowCHinese(32, 0,7);//

    OLED_ShowCHinese(64, 0,8);//血氧
    OLED_ShowCHinese(80, 0,9);//
		OLED_Show();
		if(mode==0)    //整个程序执行一次1S,所以设置模式时禁用主程序,保证按键检测灵敏,但第一次按键要长按1S左右
		{
		i=0;					//重置初始值
		un_max=0;
        un_min=PPG_MIN_COUNT;
        
        for(i=100;i<500;i++)	//数组往前移100->目的是丢弃500个数组中的前面100个数组,并更新100个新采集的值
        {
            aun_red_buffer[i-100]=aun_red_buffer[i];
            aun_ir_buffer[i-100]=aun_ir_buffer[i];
            
			if(i > 400)
			{
				if(un_min>aun_ir_buffer[i])  
					un_min=aun_ir_buffer[i];	//找到最小值
				if(un_max<aun_ir_buffer[i])
					un_max=aun_ir_buffer[i];	//找到最大值
			}
			
    }

		Wave_Range = un_max-un_min; //计算波峰波谷的差值
		Wave_Range = Wave_Range/14; //计算倍率

			for(i=400;i<500;i++)  	//填充100个新数据到数组
			{
			u32 wait_cnt=0;
			wave++;				//减慢OLED刷新速度
			while(MAX30102_INT==1)
			{
				if(++wait_cnt>=50000) break;	//约5ms超时：没接MAX30102时不会死等卡住
			}
			if(wait_cnt>=50000) break;			//传感器无响应，退出本次采集
			max30102_FIFO_ReadBytes(REG_FIFO_DATA,temp);//aun_ir_buffer[i]
			aun_red_buffer[i] =  (long)((long)((long)temp[0]&0x03)<<16) | (long)temp[1]<<8 | (long)temp[2]; //采集和合并得到原始数据 
			aun_ir_buffer[i] = (long)((long)((long)temp[3] & 0x03)<<16) |(long)temp[4]<<8 | (long)temp[5];  

			if(aun_ir_buffer[i] > 5000) //手指放开时一般都大于5000
			{
				if(aun_ir_buffer[i] > un_max) un_max = (aun_ir_buffer[i] + 100); //刷新倍率
				else if(aun_ir_buffer[i] < un_min)
				{
					un_min = aun_ir_buffer[i] - 200;
					Wave_Range = un_max-un_min;
					Wave_Range = Wave_Range/14;
					
				}
				
				Wave_sum = (un_max - aun_ir_buffer[i])/Wave_Range; //得到OLED显示的波形值
				buf_flag=1; //标志位,执行算法用,主要目的是过滤掉手指放开时的自然光影响
			}
			else {
				Wave_sum=1;
				buf_flag=0;
			}
	
			if(Wave_sum>16) Wave_sum = 16; 							//数值过大,纠正
//			printf("%d, %d\r\n",aun_ir_buffer[i],aun_red_buffer[i],dis_hr);//输出脉搏波形到上位机显示
			//printf("%d, %d, %d, %d \r\n",aun_ir_buffer[i],un_max,un_min,Wave_Range);
			if(wave==PPG_WAVE_COUNT)
			{
				if(mode==0)
				OLED_wave(Wave_sum);  								//显示波形
				wave=0;
			}
			keynum=KEY_Scan(0);
			if(keynum==1)
			{
				times++;
				OLED_Clear(0);
				break;
				
			}
		}
		//执行算法,去直流,滤波,计算波形幅值等
		if(buf_flag)//防止误判
		{
			maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer, &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
		}
		/* 只采纳算法判定为有效、且落在生理范围内的结果，再对最近5次做滑动平均，
		   抑制血氧/心率忽高忽低（原来没检查valid标志，也没做平均） */
		if(buf_flag && ch_spo2_valid && ch_hr_valid &&
		   n_sp02>70 && n_sp02<=100 && n_heart_rate>30 && n_heart_rate<180)
		{
			static u8 hr_buf[5], sp_buf[5];
			static u8 buf_idx=0, buf_cnt=0;
			u16 hr_sum=0, sp_sum=0;
			u8 k;
			hr_buf[buf_idx] = (u8)n_heart_rate;
			sp_buf[buf_idx] = (u8)n_sp02;
			buf_idx++;
			if(buf_idx>=5) buf_idx=0;
			if(buf_cnt<5)  buf_cnt++;
			for(k=0;k<buf_cnt;k++)
			{
				hr_sum += hr_buf[k];
				sp_sum += sp_buf[k];
			}
			dis_hr   = hr_sum/buf_cnt;
			dis_spo2 = sp_sum/buf_cnt;
		}
		else if(aun_ir_buffer[499] < 2000)	//手指移开才清零，否则保持上一次的稳定值
		{
			dis_hr = 00;
			dis_spo2 = 00;
		}
		
//    if(dis_hr>=hrH||(dis_spo2<=spo2H&&dis_spo2>=90))   //超过或低于阈值蜂鸣器响
//		{
//			 Beep_turn();
//		 }
		//else Beep_off();
		 
		if(dis_hr == 0 || dis_spo2 == 0)  
		{
			if(mode==0)
			{
				xsnprintf((char *)str,"            ");
				OLED_ShowString(0,2,str,16);
				xsnprintf((char *)str,"  --      -- ");
				OLED_ShowString(0,4,str,16);
			 }
		}
		else{
			Packet[0] = dis_hr;
			Packet[1] = dis_spo2;
		//	Uart1_SendPacket(Packet,2);
			if(mode==0)
		  {
			  OLED_ShowNum_Heart(0,2,dis_hr,3,32);
			  OLED_ShowNum_Heart(66,2,dis_spo2,2,32);
			  OLED_ShowChar(106,4,'%',16);
				
			 }
		}
		Finger_off(); //手指靠近识别
	 }



	
}
void Page_3(void)
{
		Title();
		Time_Disp();

			OLED_ShowCHinese(0, 	6, 102);//开灯时间
			OLED_ShowCHinese(16,	6, 103);//	
			OLED_ShowCHinese(32, 	6, 98);//
			OLED_ShowCHinese(48,	6, 99);//
			OLED_ShowString(64,		6,":",16);		 
			OLED_ShowNum(72,		6,Alarm_Hour,2,16);
			OLED_ShowString(88,		6,":",16);	
			OLED_ShowNum(96,		6,Alarm_Min,2,16);
		if(keynum==3)
		{
			Alarm_Hour++;
				if(Alarm_Hour>23)
				{
					Alarm_Hour=0;
				}
			command_flag = CMD_SET_ALARM;
			command_count++;		//按键修改也要计数，否则process_commands不会执行
		}
		if(keynum==4)
		{
			Alarm_Min++;
					if(Alarm_Min>59)
				{
					Alarm_Min=0;
				}
			command_flag = CMD_SET_ALARM;
			command_count++;
		}
}
u8 row=0;//阈值设置的光标Y坐标
u8 PageNum=0; // 新增子页面变量
#define MAX_PAGES 2
#define ROWS_PER_PAGE 3
void Page_4(void)
{
    OLED_ShowCHinese(26, 0, 96);  // 阈值设置
    OLED_ShowCHinese(44, 0, 97);
    OLED_ShowCHinese(62, 0, 98);
    OLED_ShowCHinese(80, 0, 99);

    // 显示当前页面的选项
    for (int i = 0; i < ROWS_PER_PAGE; i++) {
        int optionIndex = PageNum * ROWS_PER_PAGE + i;
        switch (optionIndex) {
            case 0:
                OLED_ShowCHinese(0, 2 + i * 2, 69);  // 体温
                OLED_ShowCHinese(16, 2 + i * 2, 70);
                OLED_ShowCHinese(32, 2 + i * 2, 98);  // 阈值
                OLED_ShowCHinese(48, 2 + i * 2, 99);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, count_Temp, 2, 16);
                OLED_ShowDegreeC(88, 2 + i * 2);		//℃用和数字同高的符号，避免字库里的℃偏小
                break;
            case 1:
                OLED_ShowCHinese(0, 2 + i * 2, 104);  // 时间间隔
                OLED_ShowCHinese(16, 2 + i * 2, 105);
                OLED_ShowCHinese(32, 2 + i * 2, 105);
                OLED_ShowCHinese(48, 2 + i * 2, 106);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, Sedentarysit_Sec, 3, 16);
                OLED_ShowString(96, 2 + i * 2, (u8*)"s", 16);
                break;
            case 2:
                OLED_ShowCHinese(0, 2 + i * 2, 6);  // 心率上限
                OLED_ShowCHinese(16, 2 + i * 2, 7);
                OLED_ShowCHinese(32, 2 + i * 2, 107);
                OLED_ShowCHinese(48, 2 + i * 2, 109);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, Count_hr_UpLim, 3, 16);
                break;
            case 3:
                OLED_ShowCHinese(0, 2 + i * 2, 6);  // 心率下限
                OLED_ShowCHinese(16, 2 + i * 2, 7);
                OLED_ShowCHinese(32, 2 + i * 2, 108);
                OLED_ShowCHinese(48, 2 + i * 2, 109);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, Count_hr_LowLim, 3, 16);
                break;
            case 4:
                OLED_ShowCHinese(0, 2 + i * 2, 8);  // 血氧上限
                OLED_ShowCHinese(16, 2 + i * 2, 9);
                OLED_ShowCHinese(32, 2 + i * 2, 107);
                OLED_ShowCHinese(48, 2 + i * 2, 109);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, Count_spo2_UpLim, 3, 16);
                break;
            case 5:
                OLED_ShowCHinese(0, 2 + i * 2, 8);  // 血氧下限
                OLED_ShowCHinese(16, 2 + i * 2, 9);
                OLED_ShowCHinese(32, 2 + i * 2, 108);
                OLED_ShowCHinese(48, 2 + i * 2, 109);
                OLED_ShowString(64, 2 + i * 2, (u8*)":", 16);
                OLED_ShowNum(72, 2 + i * 2, Count_spo2_LowLim, 3, 16);
                break;
        }
    }

    OLED_ShowString(110, 2 + row * 2, (u8*)"<", 16);

    if (keynum == 2)  // 按键2：移动光标
    {
        OLED_ShowString(110, 2 + row * 2, (u8*)" ", 16);
        row++;
        if (row >= ROWS_PER_PAGE) {
            row = 0;
            PageNum = (PageNum + 1) % MAX_PAGES;
            OLED_Clear(0);
        }
    }

    // 根据当前选中的选项处理按键3和按键4
    int currentOption = PageNum * ROWS_PER_PAGE + row;
    if (keynum == 3) 
			{
      //  int change = (keynum == 3) ? 1 : -1;
        switch (currentOption)
					{
            case 0://体温设置
							command_flag = CMD_TEMP_ADD;break;
            case 1://时间间隔
							command_flag = CMD_SED_ADD;break;
            case 2://心率上限
							command_flag = CMD_HRUPlim_ADD;break;
            case 3://心率下限
								command_flag = CMD_HRLOWlim_ADD;break;
            case 4://血氧上限
								command_flag = CMD_SPO2UPlim_ADD;break;
            case 5://血氧下限
							command_flag = CMD_SPO2LOWlim_ADD;break;
					}
			command_count++;		//按键修改必须计数，process_commands才会真正应用
			}
   if (keynum == 4) 
		{
      //  int change = (keynum == 3) ? 1 : -1;
        switch (currentOption)
				{
            case 0://体温设置
							command_flag = CMD_TEMP_SUB;break;
            case 1://时间间隔
							command_flag = CMD_SED_SUB;break;
            case 2://心率上限
							command_flag = CMD_HRUPlim_SUB;break;
            case 3://心率下限
							command_flag = CMD_HRLOWlim_SUB;break;
            case 4://血氧上限
							command_flag = CMD_SPO2UPlim_SUB;break;
            case 5://血氧下限
							command_flag = CMD_SPO2LOWlim_SUB;break;
        }
			command_count++;
    }

}
