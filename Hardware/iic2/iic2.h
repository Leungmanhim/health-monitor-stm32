#ifndef __IIC2_H
#define __IIC2_H

//SDA接GPIO_Pin_11
//SCL接GPIO_Pin_10

#include "stm32f10x.h"

#define SDA_DATA(a)	if (a)	\
							GPIO_SetBits(GPIOB,GPIO_Pin_11);\
							else		\
							GPIO_ResetBits(GPIOB,GPIO_Pin_11)
							
#define SCL_OUT(a)	if (a)	\
							GPIO_SetBits(GPIOB,GPIO_Pin_10);\
							else		\
							GPIO_ResetBits(GPIOB,GPIO_Pin_10)														
							
#define SDA_IO_IN      GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)


//IIC2所有操作函数
void IIC2_Init(void);                //初始化IIC2的IO口	
void SDA_OUT(void);
void SDA_IN(void);			 
void IIC2_Start(void);				//发送IIC2开始信号
void IIC2_Stop(void);	  			//发送IIC2停止信号
void IIC2_Send_Byte(u8 txd);			//IIC2发送一个字节
u16 IIC2_Read_Byte(unsigned char ack);//IIC2读取一个字节
u8 IIC2_Wait_Ack(void); 				//IIC2等待ACK信号
void IIC2_Ack(void);					//IIC2发送ACK信号
void IIC2_NAck(void);				//IIC2不发送ACK信号
void IIC2_Write_One_Byte(u8 daddr,u8 addr,u8 data);
u8 IIC2_Read_One_Byte(u8 daddr,u8 addr);	 

 
#endif

