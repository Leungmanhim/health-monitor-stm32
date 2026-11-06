#ifndef _ADC_H
#define _ADC_H

#include "stm32f10x.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_dma.h"
#include "systick.h"

void  ADC_Config(void); 
void Get_ADC_Value(void);
float TDS_Value_Conversion();
void  Adc_Init(void);
u16 Get_Adc(u8 ch) ;
u16 Get_Adc_Average(u8 ch,u8 times);
#endif

