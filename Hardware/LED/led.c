/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "led.h"
#include "systick.h"


volatile uint8_t alarm_active = 0;
volatile uint16_t alarm_duration = 0;
volatile uint32_t alarm_timer=0;
volatile uint8_t beep_counter = 0;
volatile uint8_t beep_pattern = 0;
//IO初始化
void BEEP_Init(void)
{
 
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //使能PC端口时钟
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;				 //蜂鸣器和LED端口配置
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
 GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;					
 GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	GPIO_SetBits(GPIOA,GPIO_Pin_6);
}
 
/* 报警指示灯：PB0推挽输出，默认输出高电平（熄灭），报警时拉低点亮 */
void LED_Init(void)
{
 GPIO_InitTypeDef  GPIO_InitStructure;

 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
 GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
 GPIO_Init(GPIOB, &GPIO_InitStructure);

 GPIO_SetBits(GPIOB,GPIO_Pin_0);
}

void HW_Init(void)
{
 
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //使能PC端口时钟
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;				 //蜂鸣器和LED端口配置
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; 		 //推挽输出
 GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;					
 GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	GPIO_SetBits(GPIOA,GPIO_Pin_8);
}

void StartAlarm(uint16_t duration_seconds)
{
    alarm_active = 1;
    alarm_duration = duration_seconds * 1000;  // Convert to milliseconds
    alarm_timer = 0;
    beep_pattern = 0;
    beep_counter = 0;
}