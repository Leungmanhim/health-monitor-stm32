/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/

#include "dma.h"


u16 bufadc[5];  //DMA数据数组

void DMA_Config()
{
	
	DMA_InitTypeDef DMA_InitStructure;                         //定义一个DMA结构体
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);               //使能DMA1时钟
	DMA_InitStructure.DMA_PeripheralBaseAddr=(u32) &ADC1->DR;					//外设地址为ADC1的数据寄存器地址
	DMA_InitStructure.DMA_MemoryBaseAddr=(u32)bufadc;								//内存地址为变量buf的地址
	DMA_InitStructure.DMA_DIR=DMA_DIR_PeripheralSRC; 							//DMA方向为外设到内存
	DMA_InitStructure.DMA_MemoryInc=DMA_MemoryInc_Enable;						//内存地址递增
	DMA_InitStructure.DMA_PeripheralInc=DMA_PeripheralInc_Disable;				//外设地址不递增	
	DMA_InitStructure.DMA_PeripheralDataSize=DMA_PeripheralDataSize_HalfWord;	//外设数据宽度为16位
	DMA_InitStructure.DMA_MemoryDataSize=DMA_MemoryDataSize_HalfWord;			//内存数据宽度为16位
	DMA_InitStructure.DMA_Mode=DMA_Mode_Normal;									//普通模式（非连续模式)
	DMA_InitStructure.DMA_Priority=DMA_Priority_Medium;							//中等优先级
	DMA_InitStructure.DMA_M2M=DMA_M2M_Disable;									//禁止内存到内存模式
	DMA_Init(DMA1_Channel1,&DMA_InitStructure);									//初始化通道1
	
		
}
