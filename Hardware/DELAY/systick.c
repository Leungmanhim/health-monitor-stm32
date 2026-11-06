/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "systick.h"

void Delay_us(uint32_t us)
{
	uint32_t i;
	SysTick_Config(SystemCoreClock/1000000);
	for(i=0;i<us;i++)
		while(!((SysTick->CTRL)&(1<<16)));
	SysTick->CTRL &=~SysTick_CTRL_ENABLE_Msk;
}

void Delay_ms(uint32_t ms)
{
	uint32_t i;
	SysTick_Config(SystemCoreClock/1000);
	for(i=0;i<ms;i++)
		while(!((SysTick->CTRL)&(1<<16)));
	SysTick->CTRL &=~SysTick_CTRL_ENABLE_Msk;
}
