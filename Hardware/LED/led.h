#ifndef __LED_H
#define __LED_H	 
#include "sys.h"

#define BEEP        PAout(6)// PC13
#define BEEP_ON()   GPIO_ResetBits(GPIOA, GPIO_Pin_6)
#define BEEP_OFF()  GPIO_SetBits(GPIOA, GPIO_Pin_6)
#define LED         PBout(0)	// 报警指示灯，接PB0，低电平点亮
#define LED_ON()    GPIO_ResetBits(GPIOB, GPIO_Pin_0)
#define LED_OFF()   GPIO_SetBits(GPIOB, GPIO_Pin_0)
/* 蜂鸣器和LED同步动作：报警时灯随蜂鸣节奏一起闪 */
#define ALARM_ON()  do{ BEEP_ON();  LED_ON();  }while(0)
#define ALARM_OFF() do{ BEEP_OFF(); LED_OFF(); }while(0)
#define HW GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_8)
void BEEP_Init(void);//初始化
void LED_Init(void);	//报警指示灯初始化
void HW_Init(void);	    
void StartAlarm(uint16_t duration_seconds);
#endif
