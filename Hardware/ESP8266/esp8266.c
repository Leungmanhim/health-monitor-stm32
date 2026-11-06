/*****************凉介单片机设计******************
                      STM32
 * 版本     :  V1.0
 * MCU      :  STM32F103C8T6
 * B站      :  凉介单片机设计
 * 作者     :  凉介
 * 微信     :  danpianji6
  联系我免费获取更多开源资料！单片机定做！
**********************BEGIN***********************/
#include "stm32f10x.h"
#include "esp8266.h"
#include "usart.h"
#include "systick.h"
#include "string.h"
#include "stdio.h"
#include "oled.h"
#include "secrets.h"		//WiFi / 巴法云 uid（本文件不提交到仓库）

/* MyRTC.c里的对时函数：APP下发 TIME 命令时用来校准装置时钟 */
extern u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec);
/* 直接用Unix时间戳对时，绕开日期换算，最稳 */
extern void RTC_SetCounter(u32 CounterValue);
extern void RTC_WaitForLastTask(void);
extern void StartAlarm(u8 seconds);
/* GPS演示定位：APP下发 POS 命令时，用给定坐标充当定位结果 */
extern void GPS_SetDemo(double lo, double la);
extern void GPS_ClearDemo(void);

/* 解析一个十进制小数（不依赖atof，省Flash） */
static double parse_double(const char **pp)
{
    const char *q = *pp;
    double v = 0.0, scale = 1.0;
    int sign = 1;
    while(*q == ' ') q++;
    if(*q == '-') { sign = -1; q++; }
    else if(*q == '+') q++;
    while(*q >= '0' && *q <= '9'){ v = v * 10 + (*q - '0'); q++; }
    if(*q == '.')
    {
        q++;
        while(*q >= '0' && *q <= '9'){ scale *= 10.0; v += (*q - '0') / scale; q++; }
    }
    *pp = q;
    return sign * v;
}

/****************************全局变量定义****************************/
/* 报警时间，定义在此处供main.c extern引用 */
uint8_t Alarm_Hour = 0;
uint8_t Alarm_Min  = 0;

/* 手机远程切换装置屏幕：0~4 对应主界面/心率/经纬度/时间/阈值，0xFF表示无请求 */
volatile u8 Page_Req = 0xFF;

/* 手机直接设定久坐秒数（演示用）：0=无请求 */
volatile u16 Sed_Req = 0;

/* 命令标志，由中断解析手机指令后设置，main.c的process_commands处理 */
volatile Command_t command_flag = CMD_NONE;
volatile unsigned char command_count = 0;   /* 同一条指令连续收到的次数（连击累加） */

/* ESP8266接收缓冲区 */
unsigned char esp8266_buf[512];
unsigned short esp8266_cnt = 0;          /* 当前接收字节数 */
unsigned char  esp8266_recv_end = 0;     /* 接收完成标志 */

/* TCP是否已连接标志 */
static unsigned char tcp_connected = 0;
static unsigned short wait_timeout_ms = 1000; /* 单条AT指令等待响应的超时(ms) */
static unsigned short last_try_tick = 0;      /* 上次尝试重连的时刻(ms) */
volatile unsigned char esp_status = 0;        /* 0=未联网 1=热点已连 2=云端已连 */
volatile unsigned char esp_tx_count = 0;      /* 已成功发出的数据包数，用于现场诊断 */

/****************************内部辅助函数****************************/

/* 在OLED第2页显示联网进度，便于不接串口也能看到卡在哪一步 */
static void ESP_Status(const char *s)
{
    char buf[13];
    unsigned char i;
    for (i = 0; i < 12 && s[i]; i++) buf[i] = s[i];
    for (; i < 12; i++) buf[i] = ' ';
    buf[12] = 0;
    OLED_ShowString(0, 2, (u8 *)buf, 8);
}

/**
  * 函    数：ESP8266_Clear
  * 说    明：清空接收缓冲区
  */
void ESP8266_Clear(void)
{
    memset(esp8266_buf, 0, sizeof(esp8266_buf));
    esp8266_cnt = 0;
    esp8266_recv_end = 0;
}

/**
  * 函    数：ESP8266_WaitRecive
  * 说    明：等待接收完成
  * 返 回 值：1接收完成，0超时
  */
_Bool ESP8266_WaitRecive(void)
{
    unsigned short timeout = wait_timeout_ms;  /* 超时计数，单位约1ms */
    while (esp8266_recv_end == 0)
    {
        Delay_ms(1);
        if (timeout-- == 0)
            return 0;
    }
    return 1;
}

/**
  * 函    数：ESP8266_SendCmd
  * 说    明：发送AT指令，并检测返回中是否包含指定字符串res
  * 参    数：cmd - AT指令字符串，res - 期望返回的关键字(如"OK")
  * 返 回 值：1成功，0失败
  */
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
    unsigned short timeout;

    ESP8266_Clear();
    Usart_SendString(USART2, (unsigned char *)cmd, strlen(cmd));

    if (res == NULL)
    {
        Delay_ms(50);
        return 1;
    }

    /* 关键：必须一直等到“期望的关键字”自己出现为止。
       原来只要收到任何一句OK就返回，而 AT+CIPSEND 的回包是 "OK\r\n>" 两段，
       经常在 '>' 到达之前就返回，导致误判失败、甚至让模块停在等待数据的输入状态里 */
    timeout = wait_timeout_ms;
    while (timeout--)
    {
        if (strstr((const char *)esp8266_buf, res) != NULL)
            return 1;
        Delay_ms(1);
    }
    return 0;
}

/* 通过TCP发送一段原始数据：必须走 AT+CIPSEND，直接往串口写数据是不会发到服务器的 */
_Bool ESP_SendRaw(unsigned char *data)
{
    char cmd_buf[32];
    unsigned short len = strlen((const char *)data);

    xsnprintf(cmd_buf, "AT+CIPSEND=%d\r\n", len);
    if (ESP8266_SendCmd(cmd_buf, ">") == 0)
    {
        /* 服务器推送过来的消息可能把 '>' 冲掉了，重试一次 */
        if (ESP8266_SendCmd(cmd_buf, ">") == 0)
            return 0;
    }
    Usart_SendString(USART2, data, len);
    return 1;
}

/* 建立到巴法云的TCP连接（单连接模式）并订阅主题，成功返回1 */
_Bool ESP8266_ConnectCloud(void)
{
    unsigned char sub_cmd[] = "cmd=1&uid=" BEMFA_UID "&topic=" BEMFA_TOPIC "\r\n";

    wait_timeout_ms = 4000;
    if (ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"bemfa.com\",8344\r\n", "CONNECT") == 0)
    {
        wait_timeout_ms = 1000;
        return 0;
    }
    wait_timeout_ms = 1000;

    /* "CONNECT FAIL" 里也含 CONNECT，这里要单独排除 */
    if (strstr((const char *)esp8266_buf, "FAIL") != NULL)
        return 0;

    tcp_connected = 1;

    /* 关键：必须用 CIPSEND 把 cmd=1 订阅指令真正发到服务器，才能收到手机APP下发的指令 */
    ESP_SendRaw(sub_cmd);
    return 1;
}

/**
  * 函    数：ESP8266_SendData
  * 说    明：通过已建立的TCP连接发送数据到巴法云平台
  * 参    数：data - 要发送的数据字符串
  */
void ESP8266_SendData(unsigned char *data)
{
    static uint16_t last_send_tick = 0;
    extern uint16_t time_count;     /* TIM2中断里累加的毫秒计数 */

    /* 断线时每10秒尝试重连一次，重连成功前不发数据，避免每次白等超时 */
    if (tcp_connected == 0)
    {
        if ((uint16_t)(time_count - last_try_tick) < 10000)
            return;
        last_try_tick = time_count;
        if (ESP8266_ConnectCloud() == 0)
            return;
    }

    /* 限速：两次发送至少间隔1秒，保证按键和OLED刷新不被UART阻塞 */
    if ((uint16_t)(time_count - last_send_tick) < 1000)
        return;
    last_send_tick = time_count;

    if (ESP_SendRaw(data) == 0)
    {
        tcp_connected = 0;          /* 发送不通，标记断线，下次会重连 */
        return;
    }
    esp_tx_count++;
}

/**
  * 函    数：ESP8266_Init
  * 说    明：初始化ESP8266，连接WiFi并连接巴法云TCP服务器
  */
void ESP8266_Init(void)
{
    unsigned char retry = 0;

    /* 等待ESP8266上电启动 */
    ESP_Status("WiFi init...");
    Delay_ms(500);

    /* 1. 测试AT指令 */
    while (ESP8266_SendCmd("AT\r\n", "OK") == 0)
    {
        Delay_ms(300);
        if (++retry > 2) { ESP_Status("AT ERR"); return; }
    }
    ESP_Status("AT OK");

    /* 2. 关闭回显，避免回显干扰解析 */
    ESP8266_SendCmd("ATE0\r\n", "OK");

    /* 3. 设置为STA模式(连接路由器) */
    ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK");

    /* 4. 连接WiFi（热点参数在 User\secrets.h 里配置，必须是2.4GHz） */
    ESP_Status("Joining AP..");
    wait_timeout_ms = 8000;      /* 配网可能要几秒才回OK */
    retry = 0;
    while (ESP8266_SendCmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASS "\"\r\n", "OK") == 0)
    {
        Delay_ms(500);
        if (++retry > 2) { wait_timeout_ms = 1000; ESP_Status("Hotspot ERR"); return; }
    }
    esp_status = 1;
    ESP_Status("Hotspot OK");
    wait_timeout_ms = 3000;

    /* 5. 单连接模式（不用 CIPMUX=1，AT+CIPSEND 指令兼容性更好） */
    ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK");

    /* 6. 连接巴法云TCP服务器（bemfa.com:8344） */
    ESP_Status("Conn cloud..");
    retry = 0;
    while (ESP8266_ConnectCloud() == 0)
    {
        Delay_ms(500);
        if (++retry > 2) { ESP_Status("Cloud ERR"); return; }
    }
    esp_status = 2;
    ESP_Status("Cloud OK");
}

/**
  * 函    数：extractHourAndMinute
  * 说    明：从字符串中提取小时和分钟
  * 参    数：input  - 输入字符串，如 "08:30"
  *           hour   - 输出小时指针
  *           minute - 输出分钟指针
  */
void extractHourAndMinute(const char *input, int *hour, int *minute)
{
    if (input == NULL || hour == NULL || minute == NULL)
        return;

    *hour = 0;
    *minute = 0;

    /* 简单解析 HH:MM 格式 */
    /* hand-written HH:MM parser, replaces sscanf() to save flash */
const char *pp = input;
while (*pp >= '0' && *pp <= '9') { *hour = *hour * 10 + (*pp - '0'); pp++; }
if (*pp == ':') {
pp++;
while (*pp >= '0' && *pp <= '9') { *minute = *minute * 10 + (*pp - '0'); pp++; }
}

    /* 范围限制 */
    if (*hour < 0)   *hour = 0;
    if (*hour > 23)  *hour = 23;
    if (*minute < 0) *minute = 0;
    if (*minute > 59)*minute = 59;
}

/****************************接收数据解析****************************/

/**
  * 函    数：ESP8266_ParseData
  * 说    明：解析ESP8266接收到的数据，识别手机发来的指令并设置command_flag
  */
/* 记录收到的指令：同一条指令连击时累加次数，避免主循环还没来得及处理就被覆盖 */
static void ESP_SetCommand(Command_t c)
{
    if (c == command_flag)
    {
        if (command_count < 30) command_count++;
    }
    else
    {
        command_flag = c;
        command_count = 1;
    }
}

static void ESP8266_ParseData(char *line)
{
    char *p;
    int h = 0, m = 0;
    Command_t c = CMD_NONE;

    /* 巴法云下发的消息格式: +IPD,0,len:cmd=1&uid=...&topic=RenTi&msg=指令内容 */
    p = strstr(line, "msg=");
    if (p == NULL)
        return;

    p += 4;  /* 跳过 "msg=" */

    if (strlen(p) < 3)
        return;

    /* 手机APP实际下发的是三字缩写（从云端抓包确认）：
       twu/twd=体温加减  jza/jzs=久坐加减  rst=恢复出厂
       1x/2x=心率上/下限  1y/2y=血氧上/下限，a=加 s=减 */
    if      (p[0]=='t' && p[1]=='w' && p[2]=='u') c = CMD_TEMP_ADD;
    else if (p[0]=='t' && p[1]=='w' && p[2]=='d') c = CMD_TEMP_SUB;
    else if (p[0]=='j' && p[1]=='z' && p[2]=='a') c = CMD_SED_ADD;
    else if (p[0]=='j' && p[1]=='z' && p[2]=='s') c = CMD_SED_SUB;
    else if (p[0]=='1' && p[1]=='x' && p[2]=='a') c = CMD_HRUPlim_ADD;
    else if (p[0]=='1' && p[1]=='x' && p[2]=='s') c = CMD_HRUPlim_SUB;
    else if (p[0]=='2' && p[1]=='x' && p[2]=='a') c = CMD_HRLOWlim_ADD;
    else if (p[0]=='2' && p[1]=='x' && p[2]=='s') c = CMD_HRLOWlim_SUB;
    else if (p[0]=='1' && p[1]=='y' && p[2]=='a') c = CMD_SPO2UPlim_ADD;
    else if (p[0]=='1' && p[1]=='y' && p[2]=='s') c = CMD_SPO2UPlim_SUB;
    else if (p[0]=='2' && p[1]=='y' && p[2]=='a') c = CMD_SPO2LOWlim_ADD;
    else if (p[0]=='2' && p[1]=='y' && p[2]=='s') c = CMD_SPO2LOWlim_SUB;
    else if (p[0]=='r' && p[1]=='s' && p[2]=='t') c = CMD_REST_ALL;
    /* 闹钟：APP下发形如 "2330ks"，前四位是 时分的四位数字 */
    else if (p[0]>='0' && p[0]<='9' && p[1]>='0' && p[1]<='9' &&
             p[2]>='0' && p[2]<='9' && p[3]>='0' && p[3]<='9' && p[4]=='k')
    {
        h = (p[0]-'0')*10 + (p[1]-'0');
        m = (p[2]-'0')*10 + (p[3]-'0');
        if (h < 24 && m < 60)
        {
            Alarm_Hour = (uint8_t)h;
            Alarm_Min  = (uint8_t)m;
            c = CMD_SET_ALARM;
        }
    }

    /* 兼容早期的大写指令，便于以后换APP */
    else if (strstr(p, "TEMP_ADD")     != NULL) c = CMD_TEMP_ADD;
    else if (strstr(p, "TEMP_SUB")     != NULL) c = CMD_TEMP_SUB;
    else if (strstr(p, "SED_ADD")      != NULL) c = CMD_SED_ADD;
    else if (strstr(p, "SED_SUB")      != NULL) c = CMD_SED_SUB;
    else if (strstr(p, "SET_ALARM")    != NULL) c = CMD_SET_ALARM;
    else if (strstr(p, "HR_UP_ADD")    != NULL) c = CMD_HRUPlim_ADD;
    else if (strstr(p, "HR_UP_SUB")    != NULL) c = CMD_HRUPlim_SUB;
    else if (strstr(p, "HR_LOW_ADD")   != NULL) c = CMD_HRLOWlim_ADD;
    else if (strstr(p, "HR_LOW_SUB")   != NULL) c = CMD_HRLOWlim_SUB;
    else if (strstr(p, "SPO2_UP_ADD")  != NULL) c = CMD_SPO2UPlim_ADD;
    else if (strstr(p, "SPO2_UP_SUB")  != NULL) c = CMD_SPO2UPlim_SUB;
    else if (strstr(p, "SPO2_LOW_ADD") != NULL) c = CMD_SPO2LOWlim_ADD;
    else if (strstr(p, "SPO2_LOW_SUB") != NULL) c = CMD_SPO2LOWlim_SUB;
    else if (strstr(p, "REST_ALL")     != NULL) c = CMD_REST_ALL;
    else if (strstr(p, "SED")          != NULL)
    {
        /* 直接设定久坐阈值: msg=SED 10（秒），范围10~7200，便于演示 */
        const char *q = p + 3;
        u16 v = 0;
        int digits = 0;
        while(*q == ' ') q++;
        while(*q >= '0' && *q <= '9' && digits < 5){ v = v * 10 + (*q - '0'); q++; digits++; }
        if(digits > 0){
            if(v < 10) v = 10;
            if(v > 7200) v = 7200;
            Sed_Req = v;
        }
        return;
    }
    else if (strstr(p, "ALARM")        != NULL)
    {
        /* 闹钟设置: msg=ALARM 08:30 */
        extractHourAndMinute(p + 5, &h, &m);
        Alarm_Hour = (uint8_t)h;
        Alarm_Min  = (uint8_t)m;
        c = CMD_SET_ALARM;
    }
    else if (strstr(p, "TIME")         != NULL)
    {
        /* 手机给装置对时: msg=TIME 20261102211500（年月日时分秒，北京时间）
           装置RTC出厂是从固定初值开始走的，不对时的话闹钟时间永远对不上 */
        const char *q = p + 4;
        int k, ok = 1;
        char buf[15];

        while(*q == ' ') q++;
        for(k = 0; k < 14; k++){
            if(q[k] >= '0' && q[k] <= '9') buf[k] = q[k];
            else { ok = 0; break; }
        }
        buf[14] = 0;

        if(ok){
            u16 y  = (buf[0]-'0')*1000 + (buf[1]-'0')*100 + (buf[2]-'0')*10 + (buf[3]-'0');
            u8  mo = (buf[4]-'0')*10 + (buf[5]-'0');
            u8  da = (buf[6]-'0')*10 + (buf[7]-'0');
            u8  hh = (buf[8]-'0')*10 + (buf[9]-'0');
            u8  mi = (buf[10]-'0')*10 + (buf[11]-'0');
            u8  ss = (buf[12]-'0')*10 + (buf[13]-'0');
            if(y >= 2020 && y <= 2099 && mo >= 1 && mo <= 12 && da >= 1 && da <= 31 &&
               hh < 24 && mi < 60 && ss < 60)
            {
                RTC_Set(y, mo, da, hh, mi, ss);
            }
        }
        return;         /* 对时不是阈值指令，直接返回 */
    }
    else if (strstr(p, "PAGE")         != NULL)
    {
        /* 手机远程切换装置屏幕: msg=PAGE 0~4 */
        const char *q = p + 4;
        while(*q == ' ') q++;
        if(*q >= '0' && *q <= '4') Page_Req = (u8)(*q - '0');
        return;
    }
    else if (strstr(p, "TS")           != NULL)
    {
        /* 时间戳对时: msg=TS 1793700000（手机本地时间的Unix秒）
           直接写RTC计数器，不经过年月日换算，避免日期算错 */
        const char *q = p + 2;
        long v = 0;
        int digits = 0;
        while(*q == ' ') q++;
        while(*q >= '0' && *q <= '9' && digits < 11){ v = v * 10 + (*q - '0'); q++; digits++; }
        if(digits >= 9 && v > 1000000000L)
        {
            /* Unix时间戳本身就是绝对时刻(内部按UTC)，直接写入即可。
               之前多减了8小时导致时间慢8小时，日期没跨天所以看起来只错时间 */
            RTC_SetCounter((u32)v);
            RTC_WaitForLastTask();
            StartAlarm(1);							/* 短鸣一声：确认已对时 */
        }
        return;
    }
    else if (strstr(p, "POS")          != NULL)
    {
        /* 演示定位: msg=POS 110.2415,21.1763；msg=POS 0 表示清除 */
        const char *q = p + 3;
        double lo, la;
        while(*q == ' ') q++;
        if(*q == '0')
        {
            GPS_ClearDemo();
            return;
        }
        lo = parse_double(&q);
        while(*q == ' ' || *q == ',') q++;
        la = parse_double(&q);
        if(lo > 60.0 && lo < 140.0 && la > 0.0 && la < 60.0) GPS_SetDemo(lo, la);
        return;
    }

    if (c != CMD_NONE)
        ESP_SetCommand(c);
}

/****************************USART2中断处理****************************/

/**
  * 函    数：USART2_IRQHandler
  * 说    明：USART2接收中断，ESP8266通过USART2与STM32通信
  *            接收到字节后存入esp8266_buf，检测到响应结束标志后置位recv_end
  */
void USART2_IRQHandler(void)
{
    static unsigned char rx_line[192];      /* 一行完整的接收内容（AT应答或+IPD推送） */
    static unsigned short rx_line_len = 0;
    unsigned short i;
    unsigned char res;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) == RESET)
        return;

    res = USART_ReceiveData(USART2);

    /* '>' 是 AT+CIPSEND 的提示符，后面不带换行，单独处理 */
    if (res == '>' && rx_line_len == 0)
    {
        if (esp8266_cnt < sizeof(esp8266_buf) - 2)
        {
            esp8266_buf[esp8266_cnt++] = '>';
            esp8266_buf[esp8266_cnt]   = '\0';
        }
        esp8266_recv_end = 1;
        return;
    }

    if (res == '\r')
        return;                             /* 丢弃回车，只用换行断句 */

    if (res == '\n')
    {
        rx_line[rx_line_len] = '\0';

        if (rx_line_len > 0)
        {
            if (strstr((const char *)rx_line, "+IPD") != NULL)
            {
                /* 服务器推送：单独解析，绝不碰 AT 应答缓冲，
                   这样按键连击时不会被我们自己的上报数据冲掉 */
                ESP8266_ParseData((char *)rx_line);
            }
            else
            {
                /* AT 应答：追加到应答缓冲，供 ESP8266_SendCmd 匹配关键字 */
                for (i = 0; i < rx_line_len && esp8266_cnt < sizeof(esp8266_buf) - 1; i++)
                    esp8266_buf[esp8266_cnt++] = rx_line[i];
                esp8266_buf[esp8266_cnt] = '\0';

                if (strstr((const char *)esp8266_buf, "OK")      != NULL ||
                    strstr((const char *)esp8266_buf, "ERROR")   != NULL ||
                    strstr((const char *)esp8266_buf, "CONNECT") != NULL ||
                    strstr((const char *)esp8266_buf, "CLOSED")  != NULL)
                {
                    esp8266_recv_end = 1;
                }
            }
        }
        rx_line_len = 0;
        return;
    }

    if (rx_line_len < sizeof(rx_line) - 1)
        rx_line[rx_line_len++] = res;
}
