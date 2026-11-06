/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "exti.h"
#include "mpu6050.h"
#include "oled.h"
#include "systick.h"
/**
  * @brief  MPU6050外部中断引脚
  * @param  
  * @retval 
  * @attention  RCC_APB2Periph_AFIO开启复用时钟，复用为外部中断
  */
void MPU6050_EXTI_Init(void)
{
	
	NVIC_InitTypeDef NVIC_InitStruct;
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//4级抢占,4级响应。
	EXTI_InitTypeDef EXTI_InitStructure;
	GPIO_InitTypeDef GPIO_InitStruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);//开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);//开启时钟
	
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;//上拉模式
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_5;//PB5配置为上拉输入
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStruct);	
	
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB , GPIO_PinSource5);//将GPIO口配置成外部中断 
	
	EXTI_InitStructure.EXTI_Line = EXTI_Line5;//将对应的GPIO口连接到中断线上
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;//中断事件类型，下降沿
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;//选择模式，中断型
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;//使能该中断
	EXTI_Init(&EXTI_InitStructure);//将配置好的参数写入寄存器
	
	//配置MPU6050外部中断
	NVIC_InitStruct.NVIC_IRQChannel=EXTI9_5_IRQn;//中断通道
	NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;//使能该通道中断
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=2;//主优先级0，最高
	NVIC_InitStruct.NVIC_IRQChannelSubPriority=2;//子优先级，最高
	NVIC_Init(&NVIC_InitStruct);//初始化

}




/**************************************************
函数名称：EXTI4_IRQHandler(void)
函数功能：外部中断4函数
入口参数：无
返回参数：无
***************************************************/
void EXTI4_IRQHandler(void)//中断服务函数
{
	if(EXTI_GetITStatus(EXTI_Line4)!=RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line4);
	}
}

