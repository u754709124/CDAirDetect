#include <Preferences.h>
#include "Net.h"
#include "Common.h"
#include "tftUtil.h"
#include "Task.h"

Preferences prefs; // 声明Preferences对象

// 读取一系列初始化参数
void getInfo(){
  prefs.begin("project");
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  city = prefs.getString("city", "");
  adm = prefs.getString("adm", "");
  location = prefs.getString("location", "");
  bright = prefs.getInt("bright", BRIGHT);
  isAutoBright = prefs.getBool("isAutoBright", false);
  backColor = prefs.getInt("backColor",BACK_BLACK);
  voice = prefs.getBool("voice", true);
  tempOffset = prefs.getFloat("tempOffset", 0.0);
  logInfoln("SET BRIGHTNESS:" + String(bright));
  logInfoln("AUTO BRIGHTNESS:" + String(isAutoBright));
  if(bright<=0) bright = 1;
  prefs.end();
}

// 写入初始化参数
void setInfo(){
  prefs.begin("project");
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putString("city", city);
  prefs.putString("adm", adm);
  prefs.putString("location", location);
  prefs.end();
}

// 写入声音参数
void setVoice(){
  prefs.begin("project");
  prefs.putBool("voice", voice);
  prefs.end();
}

// 写入亮度参数
void setBright(){
  prefs.begin("project");
  prefs.putInt("bright", bright);
  prefs.end();
}

// 写入自动亮度
void setAutoBright()
{
  prefs.begin("project");
  prefs.putInt("isAutoBright", isAutoBright);
  prefs.end();
}

// 写入温度偏移值
void setTempOffset(){
  prefs.begin("project");
  prefs.putFloat("tempOffset", tempOffset);
  prefs.end();
}

// 写入主题参数
void setTheme(){
  prefs.begin("project");
  prefs.putInt("backColor",backColor);
  prefs.end();
}

// 清除所有初始化参数
void clearInfo(){
  prefs.begin("project");
  prefs.clear();
  prefs.end();
}

// 测试用，在读取NVS之前，先写入自己的Wifi信息，免得每次浪费时间再配网
void setInfo4Test(){
  prefs.begin("project");
  prefs.putString("ssid", "sh.soft");
  prefs.putString("pass", "wscc6783234");
  prefs.putString("city", "江阴");
  prefs.putString("adm", "");
  prefs.putString("location", "");
  prefs.putInt("bright", 100);
  prefs.putInt("backColor",BACK_BLACK);
  prefs.end();
}