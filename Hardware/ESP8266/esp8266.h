#ifndef _ESP8266_H_
#define _ESP8266_H_

#define REV_OK		0	//接收完成标志
#define REV_WAIT	1	//接收未完成标志
// 定义命令枚举
typedef enum {
    CMD_NONE,
    CMD_TEMP_ADD,
    CMD_TEMP_SUB,
    CMD_SED_ADD,
    CMD_SED_SUB,
		CMD_SET_ALARM,
		CMD_HRUPlim_ADD,
		CMD_HRUPlim_SUB,
		CMD_HRLOWlim_ADD,
		CMD_HRLOWlim_SUB,
		CMD_SPO2UPlim_ADD,
		CMD_SPO2UPlim_SUB,
		CMD_SPO2LOWlim_ADD,
		CMD_SPO2LOWlim_SUB,
		CMD_REST_ALL
	
} Command_t;

void ESP8266_Clear(void);

_Bool ESP8266_WaitRecive(void);

_Bool ESP8266_SendCmd(char *cmd, char *res);

void ESP8266_SendData(unsigned char *data);

void ESP8266_Init(void);

void USART2_IRQHandler(void);

void extractHourAndMinute(const char *input, int *hour, int *minute);
#endif
