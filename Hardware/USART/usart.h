#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "sys.h" 
#include <stdio.h>

#define USART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		//使能（1）/禁止（0）串口1接收

#define EN_USART2_RX 			1		//使能（1）/禁止（0）串口2接收

#define GPS_Buffer_Length 80
#define UTCTime_Length 11
#define latitude_Length 11
#define N_S_Length 2
#define longitude_Length 12
#define E_W_Length 2 

#define false 0
#define true 1

typedef struct SaveData 
{
	char GPS_Buffer[GPS_Buffer_Length];
	char isGetData;		//是否获取到GPS数据
	char isParseData;	//是否解析完成
	char UTCTime[UTCTime_Length];		//UTC时间
	char latitude[latitude_Length];		//纬度
	char N_S[N_S_Length];		//N/S
	char longitude[longitude_Length];		//经度
	char E_W[E_W_Length];		//E/W
	char isUsefull;		//定位信息是否有效
} _SaveData;


extern uint8_t Serial_TxPacket[];
extern uint8_t Serial_RxPacket[];

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t Number, uint8_t Length);
void Serial_Printf(char *format, ...);
void Serial_SendPacket(void);
uint8_t Serial_GetRxFlag(void); 	

extern char rxdatabufer;
extern u16 point1;
extern _SaveData Save_Data;

void CLR_Buf(void);

u8 Hand(char *a);
void clrStruct(void);

extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
extern u16 USART_RX_STA;         		//接收状态标记	

extern u8  USART2_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
extern u16 USART2_RX_STA;         		//接收状态标记	
//如果想串口中断接收，请不要注释以下宏定义
void Usart1_Init(u32 bound);
void Usart2_Init(u32 bound);
void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len);

/* lightweight formatted output - replaces sprintf(), supports %d %u %x %X %c %s %% with width / zero-pad / l modifier */
int xsnprintf(char *s, const char *fmt, ...);
#endif


