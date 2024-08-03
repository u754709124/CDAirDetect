#include <OneButton.h>
#include <PMS.h>
#include <DFRobot_SCD4X.h>
#include <Ticker.h>
#include "esp_adc_cal.h"
#include "net.h"
#include "Task.h"
#include "tftUtil.h"
#include "PreferencesUtil.h"

enum CurrentPage currentPage = SETTING; // 记录当前页面
unsigned long lastRefresh;  // 上次刷新传感器数据的时间
bool updateWeather = false; // 是否需要更新天气
// 按钮
OneButton button1(BTN1, true);
OneButton button2(BTN2, true);
OneButton button3(BTN3, true);
// 系统变量
int mode = OFFLINE_MODE; // 运行模式
bool buttonEnable = true; // 按键使能
bool settingChoosed = true; // 选择的是开始配置按键
bool voice = true; // 声音是否开启
bool loadingAnim = false; // 加载动画是否执行
bool modalShowed = false; // 模态框是否已显示
float tempOffset; // 温度偏移值
float tmpTempOffset; // 记录临时温度偏移值
int tmpBright; // 记录临时亮度
bool tmpIsAutoBright;

//Battary
float batteryVoltage = 4.2;
int batteryPercent = 100;
bool Charging = true;

// SCD40
DFRobot_SCD4X SCD4X(&Wire, SCD4X_I2C_ADDR);
String temperature = "0.0";
String humidity = "0.0";
String co2 = "0";
// PMS
PMS pms(Serial1);
String pm2p5 = "0";
String pm10 = "0";
String pm1 = "0";
// CJSH_CH20
uint8_t packet[9];
String ch2o = "0.00";

// 开启SCD40、PMS、CJSH_CH2O
void sensorsInit(){
  //初始化SCD40
  Wire.setPins(SCD40_SDA, SCD40_SCL);
  Wire.begin();
  bool textShowed = false;
  while(!SCD4X.begin()){
    logInfoln("SCD40初始化失败");
    // 屏幕上绘制文字
    if(!textShowed){
      draw2LineText("未识别到二氧化碳传感器","请检查设备");
      textShowed = true;
    }
    delay(1000);
  }
  SCD4X.enablePeriodMeasure(SCD4X_STOP_PERIODIC_MEASURE);
  SCD4X.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);
  logInfoln("SCD40初始化成功");
  delay(1000);
  Serial1.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pms.activeMode();
  pms.wakeUp();
  logInfoln("PMS7003初始化成功");
  Serial2.begin(9600, SERIAL_8N1, CJSH_RX, CJSH_TX);
  logInfoln("CJSH_CH2O初始化成功");
  delay(1000);
}
// 按键声
void Dida(){
  if(voice){
    tone(BUZZER, 800);
    delay(40);
    noTone(BUZZER);
  }
}
///////////////////////////////////Freertos区域///////////////////////////////////////
void getScd40Data()
{
  if(SCD4X.getDataReadyStatus()) {
    DFRobot_SCD4X::sSensorMeasurement_t data;
    SCD4X.readMeasurement(&data);
    float f_temperature = data.temp;
    float f_humidity = data.humidity;
    uint16_t uint_co2 = data.CO2ppm;
    if(temperature.equals("0.0"))
    {
      temperature = String(f_temperature, 1);
      humidity = String(f_humidity, 1);
      co2 = String(uint_co2);
    }else{
      temperature = String((temperature.toFloat() + f_temperature) / 2, 1);
      humidity = String((humidity.toFloat() + f_humidity) / 2, 1);
      co2 = String((co2.toInt() + uint_co2) / 2);
    }

    logDebug("co2: ");logDebug(co2);logDebugln(" ppm");
    logDebug("temperature: ");logDebug(temperature);logDebugln(" ℃");
    logDebug("humidity: ");logDebug(humidity);logDebugln(" %RH");
    logDebugln("------------------------------------------------------");
  }
}

void getPmsData()
{
  Serial.println("Send read request...");
  pms.requestRead();

  Serial.println("Wait max. 1 second for read...");
  PMS::DATA data;
  if (pms.readUntil(data)){
    uint16_t uint_pm1 = data.PM_AE_UG_1_0;
    uint16_t uint_pm2_5 = data.PM_AE_UG_2_5;
    uint16_t uint_pm10 = data.PM_AE_UG_10_0;
    if(pm2p5.equals("0"))
    {
      pm1 = String(uint_pm1);
      pm2p5 = String(uint_pm2_5);
      pm10 = String(uint_pm10);
    }else{
      pm1 = String((pm1.toInt() + uint_pm1) / 2);
      pm2p5 = String((pm2p5.toInt() + uint_pm2_5) / 2);
      pm10 = String((pm10.toInt() + uint_pm10) / 2);
    }

    logDebug("pm1: ");logDebug(pm1);logDebugln(" ug/m3");
    logDebug("pm2p5: ");logDebug(pm2p5);logDebugln(" ug/m3");
    logDebug("pm10: ");logDebug(pm10);logDebugln(" ug/m3");
    logDebugln("------------------------------------------------------");
  }
}

void getCJSH_Ch2oData(){ 
  int count = 0;
  // 检查是否有足够数据可读
  while(Serial2.available()){
    if(count < 9){
      packet[count] = Serial2.read();
    }else{
      Serial2.read();
    }
    count++;
  }
  // Serial.println(count);
  // for(int i = 0;i < 9; i++){
  //   Serial.println(packet[i]);
  // }
  if(count==9 && packet[0] == 0xff && packet[1] == 0x17){ // 校验数据
    // 计算污染气体浓度值 ppb转pbm(*1000) pbm转mg/m(*1.34)
    float f_ch2o = (float)(packet[4] * 256 + packet[5]) / 1000 * 1.34;
    if(ch2o.equals("0.00")){
      ch2o = String(f_ch2o, 2);
    }else{
      // 取平均值，增加数据平滑性
      ch2o = String((ch2o.toFloat() + f_ch2o) / 2, 2);
    }
    logDebug("CH2O: ");logDebug(ch2o);logDebugln(" mg/m3");
  }
}

void getLightAdc()
{
  int brightSamplingValue = analogRead(LIGHT_ADC);
  int brightness = map(brightSamplingValue, 0, 4095, 1, 65);
  analogWrite(BL, brightness);
}


// 任务句柄
TaskHandle_t anotherCoreTask;
TaskHandle_t getPmsDataTask;
TaskHandle_t drawLoadingTask;
TaskHandle_t fadeOnTask;
// 任务内容
void anotherCore_task(void *pvParameters){
  logInfoln(String("核心") + String(xPortGetCoreID()) + String("开始执行传感器任务"));
  // 初始化按键
  btnInit();
  logInfoln("按键初始化成功");
  // 开始定时更新天气的任务
  startTickerUpdateWeather();
  // 循环获取数据
  while(true){
    if(mode == ONLINE_MODE && wifiConnected()){
      timeClient.update();
    }    
    getScd40Data();
    getCJSH_Ch2oData();
    if(isAutoBright)
    {
      getLightAdc();
    }
    
    vTaskDelay(1000);
  }
  vTaskDelete(anotherCoreTask);
}

void getPmsData_task(void *pvParameters){
  while(true)
  {
    Serial.println("Waking up, wait 30 seconds for stable readings...");
    pms.wakeUp();
    vTaskDelay(30000);
    getPmsData();
    Serial.println("Going to sleep for 60 seconds.");
    pms.sleep();
    vTaskDelay(60000);
  }
  vTaskDelete(getPmsDataTask);
}
void drawLoading_task(void *pvParameters){
  String text = (char *)pvParameters;
  int angle = 0;
  drawLoading(true, text, &angle);
  while(loadingAnim){
    drawLoading(false, text, &angle);
    vTaskDelay(10);
  }
  vTaskDelete(drawLoadingTask);
}
void fadeOn_task(void *pvParameters){
  while(true){
    fadeOn();
    break;
  }
  vTaskDelete(fadeOnTask);
}
// 创建任务
void createAnotherCoreTask(){
  xTaskCreatePinnedToCore(anotherCore_task, "anotherCore_task", 2 * 1024, NULL, 1, &anotherCoreTask, 0);
  xTaskCreatePinnedToCore(getPmsData_task, "getPmsData_task", 1 * 1024, NULL, 1, &getPmsDataTask, 0);
}
void createDrawLoadingTask(char *text){
  loadingAnim = true;
  xTaskCreatePinnedToCore(drawLoading_task, "drawLoading_task", 2 * 1024, (void *)text, 1, &drawLoadingTask, 0);
}
void createFadeOnTask(){
  xTaskCreatePinnedToCore(fadeOn_task, "fadeOn_task", 1 * 1024, NULL, 1, &fadeOnTask, 0);
}
//////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////定时器区域//////////////////////////////////////////
Ticker ticker_updateWeather;
void updateWeatherTask(){
  if(mode == OFFLINE_MODE || !wifiConnected()){
    return;
  }
  updateWeather = true;
}
void startTickerUpdateWeather(){
  // 每隔一段时间更新一次天气
  ticker_updateWeather.attach(UPDATE_WEATHER_INTERVAL, updateWeatherTask);
}
//////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////// 按键区////////////////////////////////////////
// 按键方法
void btn1click(){
  if(!buttonEnable){
    return;
  }
  Dida();
  switch(currentPage){
    int lastChoosedIndex;
    case SETTING:
      if(!settingChoosed){
        settingChoosed = true;
        drawSettingOrOffline(false, "");
      }
      break;
    case PAGE1:
      buttonEnable = false; // 先停止刷新，停止按键监控
      int tmp;
      int nextBigZone;
      switch(currentBigZone){
        case TEM:
          nextBigZone = CO2;
          break;
        case HUM:
          nextBigZone = TEM;
          break;
        case PM2_5:
          nextBigZone = HUM;
          break;
        case CH2O:
          nextBigZone = PM2_5;
          break;
        case CO2:
          nextBigZone = CH2O;
          break; 
        default:
          break;      
      }
      for(int i = 0; i <= 20; i++){
        drawPage1Z(currentBigZone, -i * 16, 0);
        drawPage1Z(nextBigZone, i * 8, 0);
      }
      tmp = zoneIndex[nextBigZone];
      zoneIndex[nextBigZone] = zoneIndex[currentBigZone];
      zoneIndex[currentBigZone] = tmp; 
      for(int i = 20; i >= 0; i--){
        drawPage1Z(nextBigZone, i * 16, 0);
        drawPage1Z(currentBigZone, -i * 8, 0);       
      }
      currentBigZone = nextBigZone;
      buttonEnable = true;
      break;
    case CONFIG:
      if(!modalShowed){
        buttonEnable = false;
        lastChoosedIndex = configChoosedIndex;
        configChoosedIndex = (configChoosedIndex - 1) >= 0 ? (configChoosedIndex - 1) : 5;
        drawConfigOption(lastChoosedIndex);
        drawConfigOption(configChoosedIndex);
        buttonEnable = true;
      }else{
        if(configChoosedIndex == OPTION_WLAN || configChoosedIndex == OPTION_RESET){
          modalLeftChoosed = true;
          drawModal("", false);
        }else if(configChoosedIndex == OPTION_BRIGHT){
          if(bright == MIN_BRIGHT){
            if(isAutoBright == false)
            {
              isAutoBright = true;
              logInfoln("AUTO BRIGHTNESS ENABLED!");
            }
            return;
          }
          bright--;
          analogWrite(BL, bright);
          drawBrightModal(false);
        }else if(configChoosedIndex == OPTION_OFFSET){
          if(tempOffset >= 0.1){
            tempOffset-=0.1;
            drawOffsetModal(false);
          }
        }
      }
      break;
    case CALENDAR:
      // 上一个月
      buttonEnable = false;
      monthOffset-=1;
      offsetDerection = CALENDAR_LEFT_OFFSET;
      drawCalendarDate();
      buttonEnable = true;
      break;
    case BONGOCAT:
      buttonEnable = false;
      if(backFillColor == BACK_BLACK){
        tft.pushImage(25,90,80,80,left_down_black);
      }else{
        tft.pushImage(25,90,80,80,left_down_white);
      }
      delay(75);
      if(backFillColor == BACK_BLACK){
        tft.pushImage(25,90,80,80,left_up_black);
      }else{
        tft.pushImage(25,90,80,80,left_up_white);
      }
      buttonEnable = true;
      break;
    default:
      break;
  }
}
void btn2click(){
  if(!buttonEnable){
    return;
  }
  Dida();
  switch(currentPage){
    case SETTING:
      if(settingChoosed){ // 开始启动服务器配网或重启重新获取数据
        if(getDataFailed){
          ESP.restart();
        }else{
          buttonEnable = false;
          createDrawLoadingTask("服务器启动中");
          // 开启AP配网
          wifiConfigBySoftAP();
          // 关闭加载动画
          loadingAnim = false;
          fadeOff();
          delay(200);
          // 绘制文字等待用户连接
          draw2LineText("连接CC Air Detector热点", "进入192.168.1.1配置");
          createFadeOnTask();
        }      
      }else{ // 离线模式，进入Page1
        getDataFailed = false;
        mode = OFFLINE_MODE;
        currentPage = PAGE1;
        drawPage1();
      }
      break;
    case CONFIG:
      switch(configChoosedIndex){
        case OPTION_BRIGHT:
          if(!modalShowed){
            modalShowed = true;
            tmpBright = bright;
            tmpIsAutoBright = isAutoBright;
            drawBrightModal(true);
          }else{
            if(bright != tmpBright){
              setBright();
            }
            if(tmpIsAutoBright != isAutoBright)
            {
              setAutoBright();
            }
            modalShowed = false;
            drawConfigOption(2);
            drawConfigOption(3);
          }
          break;
        case OPTION_VOICE:
          voice = !voice;
          setVoice();
          drawConfigOption(configChoosedIndex);
          break;
        case OPTION_THEME:
          exchangeTheme();
          drawConfig();
          break;
        case OPTION_WLAN:
          if(!modalShowed){
            modalLeftChoosed = false;
            drawModal("开始配置网络", true);
            modalShowed = true;
          }else{
            if(modalLeftChoosed){
              disconnectWiFi();
              buttonEnable = false;
              currentPage = SETTING;
              createDrawLoadingTask("服务器启动中");
              // 开启AP配网
              wifiConfigBySoftAP();
              // 关闭加载动画
              loadingAnim = false;
              fadeOff();
              delay(200);
              // 绘制文字等待用户连接
              draw2LineText("连接CC Air Detector热点", "进入192.168.1.1配置");
              createFadeOnTask();
            }else{
              drawConfigOption(0);
              drawConfigOption(1);
              drawConfigOption(2);
              drawConfigOption(3);
              modalShowed = false;
            }
          }
          break;
        case OPTION_RESET:
          if(!modalShowed){
            modalLeftChoosed = false;
            drawModal("即将恢复出厂", true);
            modalShowed = true;
          }else{
            if(modalLeftChoosed){
              clearInfo();
              draw2LineText("已恢复出厂", "即将重启");
              delay(1500);
              ESP.restart(); 
            }else{
              drawConfigOption(0);
              drawConfigOption(1);
              drawConfigOption(2);
              drawConfigOption(3);
              modalShowed = false;
            }
          }
          break;
        case OPTION_OFFSET:
          if(!modalShowed){
            modalShowed = true;
            tmpTempOffset = tempOffset;
            drawOffsetModal(true);
          }else{
            if(tempOffset != tmpTempOffset){
              setTempOffset();
            }
            modalShowed = false;
            drawConfigOption(3);
            drawConfigOption(4);
          }
          break;
        default:
          break;          
      }
      break;
    case CALENDAR:
      // 当前月
      buttonEnable = false;
      if(monthOffset > 0){
        offsetDerection = CALENDAR_LEFT_OFFSET;
      }else{
        offsetDerection = CALENDAR_RIGHT_OFFSET;
      }
      monthOffset = 0;
      drawCalendarDate();
      buttonEnable = true;
      break;
    case BONGOCAT:
      buttonEnable = false;
      if(backFillColor == BACK_BLACK){
        tft.pushImage(25,90,80,80,left_down_black);
        tft.pushImage(140,115,80,80,right_down_black);
      }else{
        tft.pushImage(25,90,80,80,left_down_white);
        tft.pushImage(140,115,80,80,right_down_white);
      }
      delay(75);
      if(backFillColor == BACK_BLACK){
        tft.pushImage(25,90,80,80,left_up_black);
        tft.pushImage(140,115,80,80,right_up_black);
      }else{
        tft.pushImage(25,90,80,80,left_up_white);
        tft.pushImage(140,115,80,80,right_up_white);
      }
      buttonEnable = true;
      break;
    default:
      break;
  }
}
void btn3click(){
  if(!buttonEnable){
    return;
  }
  Dida();
  switch(currentPage){
    int lastChoosedIndex;
    case SETTING:
      if(settingChoosed){
        settingChoosed = false;
        drawSettingOrOffline(false, "");
      } 
      break;
    case PAGE1:
      buttonEnable = false; // 先停止刷新，停止按键监控
      int tmp;
      int nextBigZone;
      switch(currentBigZone){
        case TEM:
          nextBigZone = HUM;
          break;
        case HUM:
          nextBigZone = PM2_5;
          break;
        case PM2_5:
          nextBigZone = CH2O;
          break;
        case CH2O:
          nextBigZone = CO2;
          break;
        case CO2:
          nextBigZone = TEM;
          break; 
        default:
          break;      
      }
      for(int i = 0; i <= 20; i++){
        drawPage1Z(currentBigZone, i * 16, 0);
        drawPage1Z(nextBigZone, -i * 8, 0);
      }
      tmp = zoneIndex[nextBigZone];
      zoneIndex[nextBigZone] = zoneIndex[currentBigZone];
      zoneIndex[currentBigZone] = tmp; 
      for(int i = 20; i >= 0; i--){
        drawPage1Z(nextBigZone, -i * 16, 0);
        drawPage1Z(currentBigZone, i * 8, 0);       
      }
      currentBigZone = nextBigZone;
      buttonEnable = true;
      break;  
    case CONFIG:
      if(!modalShowed){
        buttonEnable = false;
        lastChoosedIndex = configChoosedIndex;
        configChoosedIndex = (configChoosedIndex + 1) <= 5 ? (configChoosedIndex + 1) : 0;
        drawConfigOption(lastChoosedIndex);
        drawConfigOption(configChoosedIndex);
        buttonEnable = true;
      }else{
        if(configChoosedIndex == OPTION_WLAN || configChoosedIndex == OPTION_RESET){
          modalLeftChoosed = false;
          drawModal("", false);
        }else if(configChoosedIndex == OPTION_BRIGHT){
          if(bright == MAX_BRIGHT){
            return;
          }
          bright++;
          if(isAutoBright)
          {
            isAutoBright = false;
            logInfoln("AUTO BRIGHTNESS DISABLED!");
          }
          analogWrite(BL, bright);
          drawBrightModal(false);
        }else if(configChoosedIndex == OPTION_OFFSET){
          if(tempOffset < 9.9){
            tempOffset+=0.1;
            
            drawOffsetModal(false);
          }
        }
      }
      break;
    case CALENDAR:
      // 下一个月
      buttonEnable = false;
      monthOffset+=1;
      offsetDerection = CALENDAR_RIGHT_OFFSET;
      drawCalendarDate();
      buttonEnable = true;
      break;  
    case BONGOCAT:
      buttonEnable = false;
      if(backFillColor == BACK_BLACK){
        tft.pushImage(140,115,80,80,right_down_black);
      }else{
        tft.pushImage(140,115,80,80,right_down_white);
      }
      delay(75);
      if(backFillColor == BACK_BLACK){
        tft.pushImage(140,115,80,80,right_up_black);
      }else{
        tft.pushImage(140,115,80,80,right_up_white);
      } 
      buttonEnable = true;
      break;
    default:
      break;
  }
}
void btn1LongClick(){
  if(!buttonEnable || modalShowed){
    return;
  }
  Dida();
  buttonEnable = false;
  switch(currentPage){   
    case PAGE1:
      fadeOff();
      currentPage = CONFIG;
      drawConfig();
      createFadeOnTask();
      break;
    case PAGE2:
      fadeOff();
      currentPage = PAGE1;
      drawPage1();
      createFadeOnTask();
      break;
    case PAGE3:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = PAGE2;
        drawPage2();
      }else{
        currentPage = PAGE1;
        drawPage1();
      }
      createFadeOnTask();
      break;
    case CALENDAR:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = PAGE3;
        drawPage3(true);
      }else{
        currentPage = PAGE1;
        drawPage1();
      }
      createFadeOnTask();
      break;
    case CONFIG:
      fadeOff();
      currentPage = BONGOCAT;
      drawBongoCat();
      createFadeOnTask();
      break;
    case BONGOCAT:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = CALENDAR;
        drawCalendar();
      }else{
        currentPage = PAGE1;
        drawPage1();
      }
      createFadeOnTask();
      break;  
    default:
      break;
  }
  buttonEnable = true;
}
void btn2LongClick(){
  Dida();
  refreshTFT();
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 0);
  logInfoln("进入休眠...");
  esp_deep_sleep_start();
}
void btn3LongClick(){
  if(!buttonEnable || modalShowed){
    return;
  }
  Dida();
  buttonEnable = false;
  switch(currentPage){
    case PAGE1:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = PAGE2;
        drawPage2();
      }else{
        currentPage = BONGOCAT;
        drawBongoCat();
      }
      createFadeOnTask();
      break;
    case PAGE2:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = PAGE3;
        drawPage3(true);
      }else{
        currentPage = CONFIG;
        drawConfig();
      }
      createFadeOnTask();
      break;
    case PAGE3:
      fadeOff();
      if(mode == ONLINE_MODE){
        currentPage = CALENDAR;
        drawCalendar();
      }else{
        currentPage = CONFIG;
        drawConfig();
      }
      createFadeOnTask();
      break;
    case CALENDAR:
      fadeOff();
      currentPage = BONGOCAT;
      drawBongoCat();
      createFadeOnTask();
      break;
    case BONGOCAT:
      fadeOff();
      currentPage = CONFIG;
      drawConfig();
      createFadeOnTask();
      break;  
    case CONFIG:
      fadeOff();
      currentPage = PAGE1;
      drawPage1();
      createFadeOnTask();
      break;  
    default:
      break;
  }
  buttonEnable = true;
}
void btn1DuringLongPress(){
  if(modalShowed){
    if(configChoosedIndex == OPTION_BRIGHT){
      if(bright == MIN_BRIGHT){
        return;
      }
      bright--;
      analogWrite(BL, bright);
      drawBrightModal(false);
    }else if(configChoosedIndex == OPTION_OFFSET){
      if(tempOffset >= 0.1){
        tempOffset-=0.1;
        drawOffsetModal(false);
      }
    }
    delay(10);
  }
}
void btn3DuringLongPress(){
  if(modalShowed){
    if(configChoosedIndex == OPTION_BRIGHT){
      if(bright == MAX_BRIGHT){
        return;
      }
      bright++;
      analogWrite(BL, bright);
      drawBrightModal(false);
    }else if(configChoosedIndex == OPTION_OFFSET){
      if(tempOffset < 9.9){
        tempOffset+=0.1;
        drawOffsetModal(false);
      }
    }
    delay(10);
  }
}
// 初始化各按键
void btnInit(){
  button1.attachClick(btn1click);
  button1.setDebounceMs(20); //设置消抖时长 
  button2.attachClick(btn2click);
  button2.setDebounceMs(20); //设置消抖时长 
  button3.attachClick(btn3click);
  button3.setDebounceMs(20); //设置消抖时长 
  button1.attachLongPressStart(btn1LongClick);
  button1.setPressMs(1200); //设置长按时间
  button2.attachLongPressStart(btn2LongClick);
  button2.setPressMs(1200); //设置长按时间
  button3.attachLongPressStart(btn3LongClick);
  button3.setPressMs(1200); //设置长按时间
  button1.attachDuringLongPress(btn1DuringLongPress);
  button3.attachDuringLongPress(btn3DuringLongPress);
}
// 监控按键
void watchBtn(){
  button1.tick();
  button2.tick();
  button3.tick();
}
//////////////////////////////////////////////////////////////////////////////////////

