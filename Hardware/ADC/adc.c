/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "adc.h"

																	   
void  ADC_Config(void)//初始化ADC
{
	ADC_InitTypeDef ADC_InitStructure; 
	
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE );	//开启ADC1通道时钟
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);						//配置ADC时钟，为PCLK2的8分频，即9Hz
	ADC_DeInit(ADC1);  										//复位ADC1
	ADC_TempSensorVrefintCmd(ENABLE);						//打开温度传感器
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;		//独立ADC模式
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;			//使用扫描模式
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;		//单次转换模式
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;	//转换由软件触发启动,不使用外部触发转换
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;	//数据右对齐
	ADC_InitStructure.ADC_NbrOfChannel = 4;					//要转换的通道数目4
	ADC_DMACmd(ADC1,ENABLE);								//使能ADC的DMA功能	
	ADC_Init(ADC1, &ADC_InitStructure);	
	
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);		//配置ADC1的通道0为55.5个采样周期，序列为1
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 2, ADC_SampleTime_55Cycles5);		//配置ADC1的通道1为55.5个采样周期，序列为2
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 1, ADC_SampleTime_55Cycles5);		//配置ADC1的通道2为55.5个采样周期，序列为3
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_7, 4, ADC_SampleTime_55Cycles5);		//配置ADC1的通道3为55.5个采样周期，序列为4
	ADC_Cmd(ADC1, ENABLE);									//使能ADC1
	ADC_StartCalibration(ADC1);								//开启AD校准
	while(ADC_GetCalibrationStatus(ADC1));					//等待校准完成
	
  }

//获得ADC的值	
void Get_ADC_Value()
{
	DMA_SetCurrDataCounter(DMA1_Channel1,4);				//设置DMA的传送数量为4
	DMA_Cmd(DMA1_Channel1,ENABLE);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);					//使用软件转换启动功能	
	while(DMA_GetFlagStatus(DMA1_FLAG_TC1)!=SET);			//等待DMA传送完成
	DMA_ClearFlag(DMA1_FLAG_TC1);							//清除DMA传送完成标志
	DMA_Cmd(DMA1_Channel1,DISABLE);							//停用DMA通道1
}

float TDS=0.0,TDS_voltage;



extern u16 bufadc[];

float TDS_Value_Conversion()
{
	
	
	TDS_voltage =(float) bufadc[1]/4096*5; // 读取转换的AD值
	
//	if((TDS_value<=0)){TDS_value=0;}
//	if((TDS_value>1500)){TDS_value=1500;}
//	TDS_value=(TDS_value-40)/400*100;






//	OLED_ShowNum(95,   4,TDS_value,3,16); 

//	 if(TDS_voltage>1250)
//	{
//		TDS_voltage=90;
//	}
//	else if(TDS_voltage>560)
//	{
//		TDS_voltage=80;
//	}
//	else if(TDS_voltage>400)
//	{
//		TDS_voltage=70;
//	}
//	else if(TDS_voltage>340)
//	{
//		TDS_voltage=60;
//	}
//	else if(TDS_voltage>300)
//	{
//		TDS_voltage=50;
//	}
//	else if(TDS_voltage>240)
//	{
//		TDS_voltage=40;
//	}
//	else if(TDS_voltage>220)
//	{
//		TDS_voltage=30;
//	}
//	else if(TDS_voltage>190)
//	{
//		TDS_voltage=20;
//	}
//	else if(TDS_voltage>170)
//	{
//		TDS_voltage=10;
//	}
//	else if(TDS_voltage<170)
//	{
//		TDS_voltage=0;
//	}
	return TDS_voltage;

}
void  Adc_Init(void)
{    
	//先初始化IO口
 	RCC->APB2ENR|=1<<2;    //使能PORTA口时钟 
	GPIOA->CRL&=0XFF0FFFFF;//PA5 anolog输入
	GPIOA->CRL&=0XFFFFFF0F;//PA1 anolog输入
	//通道10/11设置			 
	RCC->APB2ENR|=1<<9;    //ADC1时钟使能	  
	RCC->APB2RSTR|=1<<9;   //ADC1复位
	RCC->APB2RSTR&=~(1<<9);//复位结束	    
	RCC->CFGR&=~(3<<14);   //分频因子清零	
	//SYSCLK/DIV2=12M ADC时钟设置为12M,ADC最大时钟不能超过14M!
	//否则将导致ADC准确度下降! 
	RCC->CFGR|=2<<14;      	 
	ADC1->CR1&=0XF0FFFF;   //工作模式清零
	ADC1->CR1|=0<<16;      //独立工作模式  
	ADC1->CR1&=~(1<<8);    //非扫描模式	  
	ADC1->CR2&=~(1<<1);    //单次转换模式
	ADC1->CR2&=~(7<<17);	   
	ADC1->CR2|=7<<17;	   //软件控制转换  
	ADC1->CR2|=1<<20;      //使用用外部触发(SWSTART)!!!	必须使用一个事件来触发
	ADC1->CR2&=~(1<<11);   //右对齐	 
	ADC1->CR2|=1<<23;      //使能温度传感器

	ADC1->SQR1&=~(0XF<<20);
	ADC1->SQR1|=0<<20;     //1个转换在规则序列中 也就是只转换规则序列1 			   
	//设置通道1的采样时间
	ADC1->SMPR2&=~(7<<3);  //通道1采样时间清空	  
 	ADC1->SMPR2|=7<<3;     //通道1  239.5周期,提高采样时间可以提高精确度	 

 	ADC1->SMPR1&=~(7<<18);  //清除通道16原来的设置	 
	ADC1->SMPR1|=7<<18;     //通道16  239.5周期,提高采样时间可以提高精确度	 

	ADC1->CR2|=1<<0;	   //开启AD转换器	 
	ADC1->CR2|=1<<3;       //使能复位校准  
	while(ADC1->CR2&1<<3); //等待校准结束 			 
    //该位由软件设置并由硬件清除。在校准寄存器被初始化后该位将被清除。 		 
	ADC1->CR2|=1<<2;        //开启AD校准	   
	while(ADC1->CR2&1<<2);  //等待校准结束
	//该位由软件设置以开始校准，并在校准结束时由硬件清除  
}

//获得ADC值
//ch:通道值 0~16
//返回值:转换结果
u16 Get_Adc(u8 ch)   
{
	//设置转换序列	  		 
	ADC1->SQR3&=0XFFFFFFE0;//规则序列1 通道ch
	ADC1->SQR3|=ch;		  			    
	ADC1->CR2|=1<<22;       //启动规则转换通道 
	while(!(ADC1->SR&1<<1));//等待转换结束	 	   
	return ADC1->DR;		//返回adc值	
}
//获取通道ch的转换值，取times次,然后平均 
//ch:通道编号
//times:获取次数
//返回值:通道ch的times次转换结果平均值
u16 Get_Adc_Average(u8 ch,u8 times)
{
	u32 temp_val=0;
	u8 t;
	for(t=0;t<times;t++)
	{
		temp_val+=Get_Adc(ch);
		Delay_ms(5);
	}
	return temp_val/times;
} 
	 