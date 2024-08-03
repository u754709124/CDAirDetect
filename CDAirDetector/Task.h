#ifndef __TASK_H
#define __TASK_H

#include "Common.h"

extern enum CurrentPage currentPage;
extern unsigned long lastRefresh;
extern bool updateWeather;
// 数据相关
extern String ch2o;
extern String co2;
extern String temperature;
extern String humidity;
extern String pm2p5;
extern String pm10;
extern String pm1;
// 系统变量
extern bool settingChoosed;
extern int mode;
extern bool buttonEnable;
extern bool voice;
extern bool loadingAnim;
extern bool modalShowed;
extern float tempOffset;
// ADC相关
extern float batteryVoltage;
extern int batteryPercent;
extern bool Charging;
// 初始化函数
void sensorsInit();
void btnInit();
void watchBtn();
// FreeRTOS
void createFadeOnTask();
void createAnotherCoreTask();
void createDrawLoadingTask(char *text);
// Ticker
void startTickerUpdateWeather();

#endif

