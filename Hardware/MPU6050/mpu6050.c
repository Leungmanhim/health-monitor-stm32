/*****************凉介单片机设计******************
                      STM32                
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "mpu6050.h"
#include "sys.h"
#include "systick.h"
#include "usart.h"  
#include "oled.h"
#include "stdio.h"

float pitch,roll,yaw; 		//欧拉角
short aacx,aacy,aacz;		//加速度传感器原始数据
short gyrox,gyroy,gyroz;	//陀螺仪原始数据
int MPU_Dir;
u8 MPU_TxBuf[6];//陀螺仪发送数据存储区
u8 MPU_OK = 0;					/* MPU6050的I2C是否通信成功，0=失败 */
u8 MPU_ADDR_Use = MPU_ADDR;		/* 实际使用的器件地址：AD0接地为0x68，AD0悬空/接高可能是0x69 */
u8 MPU_ID = 0;					/* 读回的WHO_AM_I值，用来区分真6050(0x68)和6500/9250兼容片 */
u8 MPU_BusState = 0xFF;			/* 探测失败时的总线电平：bit0=SCL bit1=SDA，0xFF=未采样 */

/* 常见的兼容芯片：MPU6050=0x68，MPU6500=0x70，MPU9250=0x71，MPU9255=0x73。
   市面上很多“MPU-6050”模块实际贴的是6500/9250，ID不是0x68，只认0x68就会一直报I2C ERR */
static u8 MPU_IdValid(u8 id)
{
	return (id==0x68 || id==0x70 || id==0x71 || id==0x73) ? 1 : 0;
}

/* 探测器件地址：AD0引脚悬空的模块地址会变成0x69，两个地址都试一遍，
   否则会出现“模块有电、灯也亮，但一直提示MPU I2C ERR”的情况 */
u8 MPU_ProbeAddr(void)
{
	u8 id;

	MPU_IIC_Init();
	MPU_ADDR_Use = 0x68;
	id = MPU_Read_Byte(MPU_DEVICE_ID_REG);		/* WHO_AM_I */
	if(MPU_IdValid(id)) { MPU_ID = id; return 0x68; }

	MPU_ADDR_Use = 0x69;
	id = MPU_Read_Byte(MPU_DEVICE_ID_REG);
	if(MPU_IdValid(id)) { MPU_ID = id; return 0x69; }

	MPU_ADDR_Use = 0x68;
	return 0;									/* 两个地址都没人应答 -> 接线或供电问题 */
}

/* 探测失败时采样PB6/PB7的真实电平，用来区分“接线/短路”和“模块不应答” */
u8 MPU_ReadBusPins(void)
{
	u8 st;

	/* 先把PB6/PB7改成上拉输入，避免推挽输出把总线强行拉高，量不到真实状态 */
	GPIOB->CRL &= 0x00FFFFFF;			/* 清掉PB6、PB7的模式位 */
	GPIOB->CRL |= 0x88000000;			/* PB6/PB7 = 上拉输入 */
	MPU_IIC_Delay();

	st = 0;
	if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6)) st |= 0x01;	/* SCL */
	if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)) st |= 0x02;	/* SDA */

	MPU_IIC_Init();						/* 恢复输出模式 */
	return st;
}

//初始化MPU6050
//返回值:0,成功
//       其他,错误代码
u8 MPU_Init(void)
{ 
	u8 res; 
	MPU_IIC_Init();//初始化IIC总线
	MPU_Write_Byte(MPU_PWR_MGMT1_REG,0X80);	//复位MPU6050
  Delay_ms(100);
	MPU_Write_Byte(MPU_PWR_MGMT1_REG,0X00);	//唤醒MPU6050 
	MPU_Set_Gyro_Fsr(3);					//陀螺仪传感器,±2000dps
	MPU_Set_Accel_Fsr(0);					//加速度传感器,±2g
	MPU_Set_Rate(50);						//设置采样率50Hz
	MPU_Write_Byte(MPU_INT_EN_REG,0X00);	//关闭所有中断
	MPU_Write_Byte(MPU_USER_CTRL_REG,0X00);	//I2C主模式关闭
	MPU_Write_Byte(MPU_FIFO_EN_REG,0X00);	//关闭FIFO
	MPU_Write_Byte(MPU_INTBP_CFG_REG,0X80);	//INT引脚低电平有效
	res=MPU_Read_Byte(MPU_DEVICE_ID_REG); 
	if(MPU_IdValid(res))//WHO_AM_I不能拿来和器件地址比较(地址可能是0x69)，6500/9250的ID也不是0x68
	{
		MPU_Write_Byte(MPU_PWR_MGMT1_REG,0X01);	//设置CLKSEL,PLL X轴为参考
		MPU_Write_Byte(MPU_PWR_MGMT2_REG,0X00);	//加速度与陀螺仪都工作
		MPU_Set_Rate(50);						//设置采样率为50Hz
 	}else return 1;
	return 0;
}

//设置MPU6050陀螺仪传感器满量程范围
//fsr:0,±250dps;1,±500dps;2,±1000dps;3,±2000dps
//返回值:0,设置成功
//    其他,设置失败 
u8 MPU_Set_Gyro_Fsr(u8 fsr)
{
	return MPU_Write_Byte(MPU_GYRO_CFG_REG,fsr<<3);//设置陀螺仪满量程范围  
}


//设置MPU6050加速度传感器满量程范围
//fsr:0,±2g;1,±4g;2,±8g;3,±16g
//返回值:0,设置成功
//    其他,设置失败 
u8 MPU_Set_Accel_Fsr(u8 fsr)
{
	return MPU_Write_Byte(MPU_ACCEL_CFG_REG,fsr<<3);//设置加速度传感器满量程范围  
}


//设置MPU6050的数字低通滤波器
//lpf:数字低通滤波频率(Hz)
//返回值:0,设置成功
//    其他,设置失败 
u8 MPU_Set_LPF(u16 lpf)
{
	u8 data=0;
	if(lpf>=188)data=1;
	else if(lpf>=98)data=2;
	else if(lpf>=42)data=3;
	else if(lpf>=20)data=4;
	else if(lpf>=10)data=5;
	else data=6; 
	return MPU_Write_Byte(MPU_CFG_REG,data);//设置数字低通滤波器  
}


//设置MPU6050的采样率(假定Fs=1KHz)
//rate:4~1000(Hz)
//返回值:0,设置成功
//    其他,设置失败 
u8 MPU_Set_Rate(u16 rate)
{
	u8 data;
	if(rate>1000)rate=1000;
	if(rate<4)rate=4;
	data=1000/rate-1;
	data=MPU_Write_Byte(MPU_SAMPLE_RATE_REG,data);	//设置数字低通滤波器
 	return MPU_Set_LPF(rate/2);	//自动设置LPF为采样率的一半
}


//得到温度值
//返回值:温度值(扩大了100倍)
short MPU_Get_Temperature(void)
{
  u8 buf[2]; 
  short raw;
	float temp;
	MPU_Read_Len(MPU_ADDR_Use,MPU_TEMP_OUTH_REG,2,buf); 
  raw=((u16)buf[0]<<8)|buf[1];  
  temp=36.53+((double)raw)/340;  
  return temp*100;;
}


//得到陀螺仪值(原始值)
//gx,gy,gz:陀螺仪x,y,z轴的原始读数(带符号)
//返回值:0,成功
//    其他,错误代码
u8 MPU_Get_Gyroscope(short *gx,short *gy,short *gz)
{
  u8 buf[6],res;  
	res=MPU_Read_Len(MPU_ADDR_Use,MPU_GYRO_XOUTH_REG,6,buf);
	if(res==0)
	{
		*gx=((u16)buf[0]<<8)|buf[1];  
		*gy=((u16)buf[2]<<8)|buf[3];  
		*gz=((u16)buf[4]<<8)|buf[5];
	} 	
  return res;;
}


//得到加速度值(原始值)
//gx,gy,gz:陀螺仪x,y,z轴的原始读数(带符号)
//返回值:0,成功
//    其他,错误代码
u8 MPU_Get_Accelerometer(short *ax,short *ay,short *az)
{
  u8 buf[6],res;  
	res=MPU_Read_Len(MPU_ADDR_Use,MPU_ACCEL_XOUTH_REG,6,buf);
	if(res==0)
	{
		*ax=((u16)buf[0]<<8)|buf[1];  
		*ay=((u16)buf[2]<<8)|buf[3];  
		*az=((u16)buf[4]<<8)|buf[5];
	} 	
  return res;;
}



//IIC连续写
//addr:器件地址 
//reg:寄存器地址
//len:写入长度
//buf:数据区
//返回值:0,正常
//    其他,错误代码
u8 MPU_Write_Len(u8 addr,u8 reg,u8 len,u8 *buf)
{
	u8 i; 
  MPU_IIC_Start(); 
	MPU_IIC_Send_Byte((addr<<1)|0);//发送器件地址+写命令	
	if(MPU_IIC_Wait_Ack())	//等待应答
	{
		MPU_IIC_Stop();		 
		return 1;		
	}
  MPU_IIC_Send_Byte(reg);	//写寄存器地址
  MPU_IIC_Wait_Ack();		//等待应答
	for(i=0;i<len;i++)
	{
		MPU_IIC_Send_Byte(buf[i]);	//发送数据
		if(MPU_IIC_Wait_Ack())		//等待ACK
		{
			MPU_IIC_Stop();	 
			return 1;		 
		}		
	}    
  MPU_IIC_Stop();	 
	return 0;	
} 



//IIC连续读
//addr:器件地址
//reg:要读取的寄存器地址
//len:要读取的长度
//buf:读取到的数据存储区
//返回值:0,正常
//    其他,错误代码
u8 MPU_Read_Len(u8 addr,u8 reg,u8 len,u8 *buf)
{ 
 	MPU_IIC_Start(); 
	MPU_IIC_Send_Byte((addr<<1)|0);//发送器件地址+写命令	
	if(MPU_IIC_Wait_Ack())	//等待应答
	{
		MPU_IIC_Stop();		 
		return 1;		
	}
  MPU_IIC_Send_Byte(reg);	//写寄存器地址
  MPU_IIC_Wait_Ack();		//等待应答
  MPU_IIC_Start();
	MPU_IIC_Send_Byte((addr<<1)|1);//发送器件地址+读命令	
  MPU_IIC_Wait_Ack();		//等待应答 
	while(len)
	{
		if(len==1)*buf=MPU_IIC_Read_Byte(0);//读数据,发送nACK 
		else *buf=MPU_IIC_Read_Byte(1);		//读数据,发送ACK  
		len--;
		buf++; 
	}    
  MPU_IIC_Stop();	//产生一个停止条件 
	return 0;	
}



//IIC写一个字节 
//reg:寄存器地址
//data:数据
//返回值:0,正常
//    其他,错误代码
u8 MPU_Write_Byte(u8 reg,u8 data) 				 
{ 
  MPU_IIC_Start(); 
	MPU_IIC_Send_Byte((MPU_ADDR_Use<<1)|0);//发送器件地址+写命令	
	if(MPU_IIC_Wait_Ack())	//等待应答
	{
		MPU_IIC_Stop();		 
		return 1;		
	}
  MPU_IIC_Send_Byte(reg);	//写寄存器地址
  MPU_IIC_Wait_Ack();		//等待应答 
	MPU_IIC_Send_Byte(data);//发送数据
	if(MPU_IIC_Wait_Ack())	//等待ACK
	{
		MPU_IIC_Stop();	 
		return 1;		 
	}		 
  MPU_IIC_Stop();	 
	return 0;
}


//IIC读一个字节 
//reg:寄存器地址 
//返回值:读到的数据
u8 MPU_Read_Byte(u8 reg)
{
	u8 res;
  MPU_IIC_Start(); 
	MPU_IIC_Send_Byte((MPU_ADDR_Use<<1)|0);//发送器件地址+写命令	
	MPU_IIC_Wait_Ack();		//等待应答 
  MPU_IIC_Send_Byte(reg);	//写寄存器地址
  MPU_IIC_Wait_Ack();		//等待应答
  MPU_IIC_Start();
	MPU_IIC_Send_Byte((MPU_ADDR_Use<<1)|1);//发送器件地址+读命令	
  MPU_IIC_Wait_Ack();		//等待应答 
	res=MPU_IIC_Read_Byte(0);//读取数据,发送nACK 
  MPU_IIC_Stop();			//产生一个停止条件 
	return res;		
}


//初始化MPU6050
void MPU6050_Init(void)
{
	u8 retry = 0;
	u8 addr;

	MPU_OK = 0;

	/* 1) 先自动探测地址(0x68/0x69)：模块AD0悬空时地址可能是0x69，
	      原来固定写0x68就会出现“灯亮但不通信” */
	for(retry = 0; retry < 3; retry++)
	{
		addr = MPU_ProbeAddr();
		if(addr) break;
		Delay_ms(200);			/* 模块刚上电可能需要一点时间才应答 */
	}
	if(addr == 0)
	{
		MPU_BusState = MPU_ReadBusPins();	/* 记录PB6/PB7的真实电平，界面上显示出来 */
		return;								/* 两个地址都无应答：保持MPU_OK=0，界面显示M:NG */
	}

	MPU_ADDR_Use = addr;
	if(MPU_Init()) return;		/* 复位并检查WHO_AM_I */
	MPU_OK = 1;					/* I2C通信正常，计步/跌倒已经可用 */

	/* 2) DMP只是跌倒判断的补充，初始化失败也不影响计步和倾斜报警。
	      注意DMP库内部把地址写死0x68、固件只针对6050，所以地址/芯片不是6050时直接跳过 */
	if(MPU_ADDR_Use == 0x68 && MPU_ID == 0x68)
	{
		retry = 0;
		while(mpu_dmp_init())
		{
			Delay_ms(200);
			if(++retry>=5) break;
		}
	}
}


extern u8 QX_Flag,SD_Time,SD_Flag;
volatile u8 Step_Flag = 0;		/* 计步标志：检测到一步时置1，主循环取走后清零 */
volatile u8 Fall_Candidate = 0;	/* 跌倒预判：出现冲击/剧烈运动后置1，8秒内有效 */
static u16 fall_cand_ms = 0;
static u16 rise_ms = 0;			/* 倾角从约14°转到44°所用的时间 */
static u8  prev_tilt = 0;		/* 上一轮的倾倒状态，用来捕捉0→1跳变 */
/**************************************************
函数名称：u8 MPU_Data(void)
函数功能：陀螺仪数据处理：跌倒判断 + 计步
入口参数：无
返回参数：无
***************************************************/
u8 MPU_Data(void)
{
	short ax, ay, az;
	long absx, absy, absz, mag, dev;
	long ax_s, ay_s, az_s, ax2, az2;
	static u16 last_step_tick = 0;
	static u8  step_high = 0;
	static u8  tilt_latch = 0;			/* 倾斜状态锁存，带15°/20°回差 */
	u8 Tilt_Flag = 0;
	extern uint16_t time_count;

	/* 1) 读原始加速度：计步和倾斜判断都只依赖它，DMP初始化失败也能用 */
	if(MPU_Get_Accelerometer(&ax,&ay,&az)==0)
	{
		absx = (ax<0)? -ax : ax;
		absy = (ay<0)? -ay : ay;
		absz = (az<0)? -az : az;
		mag  = absx + absy + absz;
		dev  = mag - 16384;					/* 静止时三轴绝对值和≈1g(16384) */
		if(dev < 0) dev = -dev;

		/* 上电头1秒不参与计步：这段时间手还在拿板子，容易白计一步
		   （屏幕上那个“步数1”就是这么来的，并不是传感器坏了） */
		if(time_count > 1000 && mag > 8192)	/* mag过小说明读到的是无效数据 */
		{
			if(dev > 2000 && step_high == 0)			/* 越过波峰：计一步（0.12g，晃动也能计到） */
			{
				step_high = 1;
				if((uint16_t)(time_count - last_step_tick) > 300)
				{
					last_step_tick = time_count;
					Step_Flag = 1;
				}
			}
			else if(dev < 900)							/* 回落到谷底，允许下次计数 */
			{
				step_high = 0;
			}
		}

		/* 倾斜判断：水平分量² > 竖直分量²×(95/100)，即倾角>44°判定为倾倒；
		   回落到38°以下(60/100)才清除，避免临界角度抖动导致反复计时 */
		ax_s = ax/64; ay_s = ay/64; az_s = az/64;
		ax2  = ax_s*ax_s + ay_s*ay_s;
		az2  = az_s*az_s;
		if(100*ax2 > 95*az2)						tilt_latch = 1;
		else if(100*ax2 < 60*az2)					tilt_latch = 0;
		Tilt_Flag = tilt_latch;

		/* 跌倒判据：只看“倾角反转速度”，不引入撞击条件。
		   - 坐下/慢慢躺靠/慢跑晃动：从14°翻到45°要超过0.7秒 → 不报
		   - 摔倒：0.7秒内快速翻转超过45° → 记候选，保持倾倒6秒后才真正报警 */
		if(100*ax2 > 7*az2)				/* 倾角超过约14°：开始计时 */
		{
			if(rise_ms < 3000) rise_ms += 20;
		}
		else
		{
			rise_ms = 0;
		}

		if(tilt_latch && !prev_tilt)		/* 刚越过44°这个点 */
		{
			if(rise_ms > 0 && rise_ms <= 700)	/* 0.7秒内翻过45° = 快速翻转 */
			{
				Fall_Candidate = 1;
				fall_cand_ms = 0;
			}
		}
		prev_tilt = tilt_latch;

		if(Fall_Candidate)
		{
			fall_cand_ms += 20;
			if(fall_cand_ms > 5000) Fall_Candidate = 0;	/* 翻转后5秒内没有持续倾倒就作废 */
		}
	}
	else
	{
		/* I2C偶发失败时保持上一次的倾斜状态，避免久坐/跌倒计时被清零 */
		Tilt_Flag = tilt_latch;
	}

	/* 2) 跌倒判断：加速度倾角为主，DMP可用时再用欧拉角补充（两者取或，更不容易漏报） */
	QX_Flag = Tilt_Flag;
	if(mpu_dmp_get_data(&pitch,&roll,&yaw)==0)
	{
		if(pitch>45||pitch<-45||roll>45||roll<-45) QX_Flag = 1;
	}

	if(QX_Flag == 0)
	{
		SD_Time = 0;
		SD_Flag = 0;
	}
	return QX_Flag;
}


/**************************************************
函数名称：void MPU_Direction(void)
函数功能：陀螺仪数据发送函数
入口参数：无
返回参数：无

***************************************************/





