/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "gps.h"
#include "stdlib.h"
#include "stdio.h"    
#include "stdarg.h"		
#include "string.h" 
#include "stm32f10x_usart.h"
#include "usart.h"
#include "oled.h"
#include "systick.h"
	char str_gps_time[2];
char sbuff_code[DATA] = {0};
char str_gps[DATA] = {0};
char str_gps11[DATA] = {0};
char str_gps22[DATA] = {0};
char str_gps33[DATA] = {0};
char flag_updata[2] = {0};
double lon;//经度
double lat;//纬度
/*
	串口三配置
*/
void USART3_Init(u32 bound)
{  
 
	
	NVIC_InitTypeDef NVIC_Initstr_gpsucture;
	GPIO_InitTypeDef GPIO_Initstr_gpsucture;
	USART_InitTypeDef USART_Initstr_gpsucture;
 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	// GPIOB时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE); //串口3时钟使能
 
 	USART_DeInit(USART3);  //复位串口3
		 //USART3_TX   PB10
  GPIO_Initstr_gpsucture.GPIO_Pin = GPIO_Pin_10; //PB10
  GPIO_Initstr_gpsucture.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Initstr_gpsucture.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
  GPIO_Init(GPIOB, &GPIO_Initstr_gpsucture); //初始化PB10
   
    //USART3_RX	  PB11
  GPIO_Initstr_gpsucture.GPIO_Pin = GPIO_Pin_11;
  GPIO_Initstr_gpsucture.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
  GPIO_Init(GPIOB, &GPIO_Initstr_gpsucture);  //初始化PB11
	
	USART_Initstr_gpsucture.USART_BaudRate = bound;//波特率一般设置为9600;
	USART_Initstr_gpsucture.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_Initstr_gpsucture.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_Initstr_gpsucture.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_Initstr_gpsucture.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_Initstr_gpsucture.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
  
	USART_Init(USART3, &USART_Initstr_gpsucture); //初始化串口	3
  
	//使能接收中断
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);//开启中断   
	
	//设置中断优先级
	NVIC_Initstr_gpsucture.NVIC_IRQChannel = USART3_IRQn;
	NVIC_Initstr_gpsucture.NVIC_IRQChannelPreemptionPriority=1 ;//抢占优先级3
	NVIC_Initstr_gpsucture.NVIC_IRQChannelSubPriority = 1;		//子优先级3
	NVIC_Initstr_gpsucture.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_Initstr_gpsucture);	//根据指定的参数初始化VIC寄存器
	
	
	USART_Cmd(USART3, ENABLE);                    //使能串口 

}


 
static char USART3_TxBuff[SBUFF];        //写缓冲区
static char USART3_TxBuff_read[SBUFF];   //接收缓冲区
static int i_RT3 = 1;                          // 指向接收缓冲区位置指针 
char UT_flag;

//void USART3_IRQHandler(void)
//{
//	unsigned char r;
//	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  //标志位被置一
//	{
//		r = USART_ReceiveData(USART3);       //读取接收到的数据
//		if (r != 0 )
//		{
//				USART3_TxBuff_read[i_RT3] = r;
//				i_RT3++;
//				if (i_RT3 >= 45)
//				i_RT3 = 45;   //防止数据越界
//		}
//	} 
//	USART_ClearFlag(USART3,USART_FLAG_TC); 				 	 //清除中断标志位置										 
//} 

void USART3_IRQHandler(void)
{
	unsigned char r;
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)  //标志位被置1
	{
		r = USART_ReceiveData(USART3);       //读取接收到的数据
		
		if(r == '$')		//读到$符号说明新语句正在发送，将保存字符数组清零，并将$赋值给数组的第一个元素
		{
			memset(USART3_TxBuff_read,0,SBUFF);
			USART3_TxBuff_read[0] = r;
			i_RT3 = 1;
		}
		
		if (r != '$' && USART3_TxBuff_read[0] == '$')		//如果字符不等于$并且数组中第一个元素为$时，从第第二个元素开始，依次将字符保存至数组中
		{
				USART3_TxBuff_read[i_RT3] = r;
				i_RT3++;
				if (i_RT3 >= SBUFF)
				i_RT3 = SBUFF - 1;   //防止数据越界
		}
		
		if(i_RT3 >= 6)		//当保存到第七个字符时，只保留GGA/RMC语句，其余清空
		{
			/* $xxGGA 用于取卫星数，$xxRMC 用于取定位状态和经纬度。
			   原来只认GGA、却按RMC的字段位置解析经纬度，导致永远定位失败 */
			if(!((USART3_TxBuff_read[3]=='G' && USART3_TxBuff_read[4]=='G' && USART3_TxBuff_read[5]=='A') ||
			     (USART3_TxBuff_read[3]=='R' && USART3_TxBuff_read[4]=='M' && USART3_TxBuff_read[5]=='C')))
			{
				memset(USART3_TxBuff_read,0,SBUFF);
				i_RT3 = 1;
			}
	  }
		
		if(((USART3_TxBuff_read[3]=='G' && USART3_TxBuff_read[4]=='G' && USART3_TxBuff_read[5]=='A') ||
		    (USART3_TxBuff_read[3]=='R' && USART3_TxBuff_read[4]=='M' && USART3_TxBuff_read[5]=='C')) &&
		   r == '*')		//语句结束标志，置数据处理标志并暂停接收，防止解析时被新数据覆盖
		{
			UT_flag = 1;
			USART_Cmd(USART3, DISABLE);
		}
	} 
	USART_ClearFlag(USART3,USART_FLAG_TC); 				 	 //清除中断标志位置										 
} 

int i_printf3 = 0;
void my_printf3(char *fmt,...)
{
		va_list ap;                          //申请一va_list     变量         
		va_start(ap,fmt); 
		vsprintf(USART3_TxBuff,fmt,ap);      //将发送目标整合加入数组 ，准备发送
		va_end(ap);	                         //释放一个va_list  变量
		
		for (i_printf3 = 0; USART3_TxBuff[i_printf3] != 0; i_printf3++)
		{
				USART_SendData(USART3,USART3_TxBuff[i_printf3]);   
				while(USART_GetFlagStatus(USART3,USART_FLAG_TC) != SET);
		}
		
		memset(USART3_TxBuff,0,SBUFF);      //清除
}

int my_read3(char *temp,int time)
{
	if(UT_flag == 1)
	{
		if (USART3_TxBuff_read[0] != 0)
		{
				Delay_ms(time);
				memset(temp,0,DATA);
				strncpy(temp,USART3_TxBuff_read,DATA-1);	/* 留一个字节给'\0'，防止语句超长时越界 */
				temp[DATA-1] = 0;
			
				/* 清空读缓冲区*/
				i_RT3 = 1;                      //使缓冲区中的位置指针归零
				memset(USART3_TxBuff_read,0,SBUFF); 
			  UT_flag = 0;
				USART_Cmd(USART3, ENABLE); 
				return 1;
		}
		else
		{
				USART_Cmd(USART3, ENABLE); 
			  UT_flag = 0;
				return 0;
		}
	}	
	else
	{
			return 0;
	}	
}

u8 gps_fix = 0;			/* 1=RMC里定位状态为A，经纬度有效 */
u8 gps_sats = 0;		/* GGA里的卫星数量 */
u8 gps_quality = 0;		/* GGA里的定位质量：0=未定位 1=GPS 2=差分 */
u8 gps_alive = 0;		/* 是否收到过GPS语句：0=模块没数据(接线/供电/波特率) */
u8 gps_demo = 0;		/* 1=演示定位：APP下发POS坐标后，用给定坐标充当定位结果 */

/* APP演示定位：直接把给定坐标当成一次成功定位，屏幕与上传数据都和真实定位一致 */
void GPS_SetDemo(double lo, double la)
{
	lon = lo;
	lat = la;
	gps_demo = 1;
	gps_fix = 1;
	gps_quality = 1;
	gps_sats = 8;
}

void GPS_ClearDemo(void)
{
	gps_demo = 0;
	lon = 0.0;
	lat = 0.0;
	gps_fix = 0;
	gps_quality = 0;
	gps_sats = 0;
}

/* 取NMEA语句的第idx个逗号字段(从'$'开始算第0个)，找不到返回0 */
static char *gps_field(char *s, u8 idx)
{
	u8 n = 0;
	if(s[0] != '$') return 0;
	while(*s && *s != '*' && *s != '\r' && *s != '\n')
	{
		if(n == idx) return s;
		if(*s == ',') n++;
		s++;
	}
	return 0;
}

/* ddmm.mmmm / dddmm.mmmm 转十进制度 */
static double gps_ddmm(char *f)
{
	long v = 0, frac = 0, div = 1;
	if(f == 0) return 0.0;
	while(*f >= '0' && *f <= '9') { v = v*10 + (*f - '0'); f++; }
	if(*f == '.')
	{
		f++;
		while(*f >= '0' && *f <= '9') { frac = frac*10 + (*f - '0'); div *= 10; f++; }
	}
	return (double)(v/100) + ((double)(v%100) + (double)frac/(double)div)/60.0;
}

void display_lon_lat(void)
{
	char *st, *la, *ns, *lo, *ew, *q, *sat;

	if(!my_read3(sbuff_code,10)) return;		/* 没收到完整语句 */

	gps_alive = 1;

	if(sbuff_code[3]=='R' && sbuff_code[4]=='M' && sbuff_code[5]=='C')	/* $xxRMC：定位状态+经纬度 */
	{
		st = gps_field(sbuff_code, 2);		/* 状态：A有效 V无效 */
		la = gps_field(sbuff_code, 3);		/* 纬度 ddmm.mmmm */
		ns = gps_field(sbuff_code, 4);		/* N/S */
		lo = gps_field(sbuff_code, 5);		/* 经度 dddmm.mmmm */
		ew = gps_field(sbuff_code, 6);		/* E/W */
		if(st && st[0]=='A' && la && lo)
		{
			lon = gps_ddmm(lo); if(ew && ew[0]=='W') lon = -lon;
			lat = gps_ddmm(la); if(ns && ns[0]=='S') lat = -lat;
			gps_fix = 1;
			gps_demo = 0;			/* 收到真实定位，退出演示坐标 */
		}
		else
		{
			if(!gps_demo)			/* 演示定位期间不因失锁清零 */
			{
				lon = 0.0;
				lat = 0.0;
				gps_fix = 0;
			}
		}
	}
	else if(sbuff_code[3]=='G' && sbuff_code[4]=='G' && sbuff_code[5]=='A')	/* $xxGGA：卫星数、定位质量、经纬度 */
	{
		q   = gps_field(sbuff_code, 6);
		sat = gps_field(sbuff_code, 7);
		if(q && q[0] >= '0' && q[0] <= '9') gps_quality = q[0] - '0';
		if(sat && sat[0] >= '0' && sat[0] <= '9')
			gps_sats = (u8)((sat[0]-'0')*10 + ((sat[1] >= '0' && sat[1] <= '9') ? (sat[1]-'0') : 0));

		/* 兼容只输出GGA、不输出RMC的配置：定位质量>0时直接从GGA取经纬度 */
		la = gps_field(sbuff_code, 2);
		ns = gps_field(sbuff_code, 3);
		lo = gps_field(sbuff_code, 4);
		ew = gps_field(sbuff_code, 5);
		if(gps_quality > 0 && gps_fix == 0 &&
		   la && la[0]>='0' && la[0]<='9' && lo && lo[0]>='0' && lo[0]<='9')
		{
			lon = gps_ddmm(lo); if(ew && ew[0]=='W') lon = -lon;
			lat = gps_ddmm(la); if(ns && ns[0]=='S') lat = -lat;
			gps_fix = 1;
		}
	}
}

int data_processing(char numb) //将字符转换为数字
{
	switch(numb)
	{
		case '0':
			return 0;
		case '1':
			return 1;
		case '2':
			return 2;
		case '3':
			return 3;
		case '4':
			return 4;
		case '5':
			return 5;
		case '6':
			return 6;
		case '7':
			return 7;
		case '8':
			return 8;
		case '9':
			return 9;
		default:
			return -1;
	}
}

/***************************************
显示时间
***************************************/
void time_data(char *data)
{
    int time = 0;

    // 解析小时并转换为北京时间
    time = data_processing(data[7]) * 10;
    time += data_processing(data[8]);
    time += 8;  // UTC+8
    if(time >= 24)
    {
        time -= 24;
    }

    // 处理小时显示
    str_gps_time[0] = '0' + (time / 10);    // 十位
    str_gps_time[1] = '0' + (time % 10);    // 个位
    OLED_ShowChar(44, 6, str_gps_time[0], 16);  // 显示小时十位
    OLED_ShowChar(52, 6, str_gps_time[1], 16);  // 显示小时个位
    
    // 显示第一个冒号
    OLED_ShowChar(62, 6, ':', 16);

    // 显示分钟
    OLED_ShowChar(72, 6, data[9], 16);   // 显示分钟十位
    OLED_ShowChar(80, 6, data[10], 16);  // 显示分钟个位
    
    // 显示第二个冒号
    OLED_ShowChar(88, 6, ':', 16);

    // 显示秒钟
    OLED_ShowChar(96, 6, data[11], 16);   // 显示秒钟十位
    OLED_ShowChar(104, 6, data[12], 16);  // 显示秒钟个位
}  
/***************************************
显示经度
***************************************/
void lon_data(char *data)	//经度
{
	char lon_d;
	int lon_i;
	long lon_f;
	double lon_m;
	
	
	lon_m = data_processing(data[34]) * 1000000;
	lon_m += data_processing(data[35]) * 100000;
	lon_m += data_processing(data[37]) * 10000;
	lon_m += data_processing(data[38]) * 1000;
	lon_m += data_processing(data[39]) * 100;
	lon_m += data_processing(data[40]) * 10;
	lon_m += data_processing(data[41]);
	
	lon_m = lon_m / 100000 / 60;
	
	lon_d = data_processing(data[31]) * 100;
	lon_d += data_processing(data[32]) * 10;
	lon_d += data_processing(data[33]);
	
	lon = lon_d + lon_m;
	
	lon_i = (int)lon;
lon_f = (long)((lon - lon_i) * 100000000.0 + 0.5);
if (lon_f >= 100000000) { lon_f = 0; lon_i++; }
xsnprintf(str_gps11,"%d.%08ld",lon_i,lon_f);   //调用Sprintf函数把数	据格式化到字符串数组变量str_gps2中   
//	OLED_Showstr_gpsing(0,2,(u8 *)str_gps11,16);
	
}


/***************************************
显示纬度
***************************************/
void lat_data(char *data)
{
	int lat_d = 0;
	int lat_i = 0;
	long lat_f = 0;
	double lat_m = 0.0;

	
	lat_m = data_processing(data[20]) * 1000000;
	lat_m += data_processing(data[21]) * 100000;
	lat_m += data_processing(data[23]) * 10000;
	lat_m += data_processing(data[24]) * 1000;
	lat_m += data_processing(data[25]) * 100;
	lat_m += data_processing(data[26]) * 10;
	lat_m += data_processing(data[27]);
	
	lat_m = lat_m / 100000 / 60;
	
	lat_d += data_processing(data[18]) * 10;
	lat_d += data_processing(data[19]);
	lat = lat_d + lat_m;

	lat_i = (int)lat;
lat_f = (long)((lat - lat_i) * 1000000000.0 + 0.5);
if (lat_f >= 1000000000) { lat_f = 0; lat_i++; }
xsnprintf(str_gps22,"%d.%09ld",lat_i,lat_f);   //调用Sprintf函数把数	据格式化到字符串数组变量str_gps中   
//	OLED_Showstr_gpsing(0,4,(u8 *)str_gps22,16);		
}

/***************************************
显示卫星数量
***************************************/
void satellite_nums(char *data)
{
	int nums;
	
	nums = data_processing(data[47]) * 10;
	nums += data_processing(data[48]);
	
  xsnprintf(str_gps33,"%d",nums);   //调用Sprintf函数把数	据格式化到字符串数组变量str_gps中   
	OLED_ShowString(84,0,(u8 *)str_gps33,16);
	
}

void Page_2(void)
{
	char s[20];
	int  lon_i, lat_i;
	long lon_f, lat_f;
	char ns, ew;
	static u8 last_state = 0xFF;		/* 0=无数据 1=搜星中 2=已定位 */
	u8 state;

	/* 状态切换时清屏：否则上次"SEARCHING/GO OUTDOOR"的英文字符会残留在数字上 */
	state = (gps_demo) ? 2 : ((gps_alive==0) ? 0 : (gps_fix ? 2 : 1));
	if(state != last_state)
	{
		last_state = state;
		OLED_Clear(0);
	}

	/* 标题：经纬度 */
	OLED_ShowCHinese(8,  0, 31);	//经
	OLED_ShowCHinese(28, 0, 32);	//度
	OLED_ShowCHinese(64, 0, 33);	//纬
	OLED_ShowCHinese(84, 0, 34);	//度

	if(gps_alive == 0 && !gps_demo)
	{
		/* 一条语句都没收到：模块没供电/没接对TX RX/波特率不对 */
		OLED_ShowString(0, 3, (u8 *)"GPS NO DATA", 8);
		OLED_ShowString(0, 5, (u8 *)"CHECK TX/RX", 8);
		OLED_ShowString(0, 6, (u8 *)"9600 8N1", 8);
		return;
	}

	if(gps_fix == 0)
	{
		/* 模块在输出但还没定位：正常现象，需要到室外能看到天空的地方等冷启动 */
		OLED_ShowString(0, 3, (u8 *)"SEARCHING...", 8);
		xsnprintf(s, "SAT %02d", gps_sats);
		OLED_ShowString(0, 5, (u8 *)s, 8);
		OLED_ShowString(0, 6, (u8 *)"GO OUTDOOR", 8);
		return;
	}

	/* 经度：保留4位小数 + 东西经 */
	lon_i = (int)lon;
	lon_f = (long)((lon - lon_i) * 10000.0 + 0.5);
	if(lon_f >= 10000) { lon_f = 0; lon_i++; }
	if(lon_i < 0) lon_i = -lon_i;
	ew = (lon >= 0.0) ? 'E' : 'W';
	xsnprintf(s, "%d.%04ld%c", lon_i, lon_f, ew);
	OLED_ShowString(0, 2, (u8 *)s, 16);

	/* 纬度：保留4位小数 + 南北纬 */
	lat_i = (int)lat;
	lat_f = (long)((lat - lat_i) * 10000.0 + 0.5);
	if(lat_f >= 10000) { lat_f = 0; lat_i++; }
	if(lat_i < 0) lat_i = -lat_i;
	ns = (lat >= 0.0) ? 'N' : 'S';
	xsnprintf(s, "%d.%04ld%c", lat_i, lat_f, ns);
	OLED_ShowString(0, 4, (u8 *)s, 16);

	/* 状态：定位正常 + 卫星数 */
	OLED_ShowCHinese(32, 6, 58);	//定
	OLED_ShowCHinese(52, 6, 59);	//位
	OLED_ShowCHinese(72, 6, 79);	//正
	OLED_ShowCHinese(92, 6, 84);	//常

	xsnprintf(s, "S%02d", gps_sats);
	OLED_ShowString(0, 6, (u8 *)s, 8);
}