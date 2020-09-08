/**********************************************************
    项目记录
    项目：校级2020大创_智能物联网水杯垫
    版 本：V1.0
    说 明：
    20200902 吴韦举  为什么布尔值不用bool，其实都一样，去看看C标准就知道bool其实就是int，
                    arduino执行的C语言标准比keil51新多了，很多写法和keil不一样
                    维护此项目请做好测试以及写好修改说明
                    下一步工作使用三线程，我将继续修复bug,以及重写导入的库文件所用到的函数以及简化代码，向勉益将为此项目添加esp8266串口数据传输的功能，成帅继续完成提醒部分的程序
    bug备忘录：
              提醒时拿走水杯应该立即停止提醒
              ode
    修改记录:20200830以前  成帅  创建
            20200831 吴韦举  格式优化；代码整合；细节优化；
                            试图编写scale_antishake()函数，失败，暂时弃用了
            20200902 吴韦举  添加LCD1602_initialize(),LCD1602_welcome()两个开机显示函数
                            用remind1_s()暂时替代remind1()，前者显示秒数，因为在开发者调试过程中，分钟时间过长，不方便，开发者使用后者调试更方便
                            主程序方面，完成了饮水过程的准确识别以及饮水过程中的时间差与重量差的读取，提醒部分，计划书写得并不一定合理，故暂只象征性编写了小部分程序，此部分以后编程难度很小
                            经测试，主体功能似乎实现了！版本1.0  bug应该会很多，以后再调
            20200904 向勉益  添加物联网代码
            20200905 向勉益  修改贝壳物联代码，修改了几个bug，仍有几个bug未修复，目前饮水次数和用户未饮水时间（min)上传至贝壳物联出现困难，以后改进。


**********************************************************/

/**********************************************************
                        库文件导入
***********************************************************/
#include <dht11.h>
#include "DHTesp.h"
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //这两个库是与LCD1602有关的库文件
#include <aJSON.h>

//此板块以后将进行整理，将库文件里面我们需要的代码进行重写，现在暂时使用他人的库函数

/**********************************************************
                        宏定义
***********************************************************/
#define DHT11_PIN 49
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;
/*重现此测试项目需要如下连接：
  DHT11 DATA----D49，
  HX711 DT----A2  SCK----A3
  LED  VCC----D51
  LCD1602_I2C  SCL----SCL   SDA----SDA
  BEEP I/O----D53  VCC---D52
  Esp8266-01 RX----TX,TX----RX
*/

/**********************************************************
                      元件初始化
***********************************************************/
dht11 DHT;
DHTesp dht;
HX711 scale;
LiquidCrystal_I2C lcd(0x27, 16, 2);
//I2C LCD显示屏初始化,本来是打算放在函数里面的，但是没办法，放在函数里面会出现一些问题，故没有封在函数里面。
/**********************************************************
                      贝壳物联接口定义
***********************************************************/
String DEVICEID = "19242";      //本设备在贝壳物联上的设备ID
String APIKEY = "712c5cad8";    //设备密码
String WORKID = "17229";        //接口“是否在使用中”ID
String NO_DRINK_SID = "17230";  //接口”未饮水时间（s）“ID
String NO_DRINK_MINID = "17232";//接口”未饮水时间（min）“ID
String DRINK_TIMEID = "17231 "; //接口”饮水次数“ID
String RECORDDRINKID = "13949 "; //接口”饮水量“ID

/**********************************************************
                      全局外部控制变量定义
***********************************************************/
//用于放置微信小程序里面用户可以自主设置的变量，现规定，凡是此类变量均在前面加user_
//此中的变量均需要通过ESP8266传输的,并且要求是可以在上位机上控制这些变量的值
int user_backlight = 1;         //布尔值，用于控制LCD1602的背光显示，默认1开启
int user_blink = 1;             //布尔值，用于控制LCD1602的闪烁情况，默认1开启
int user_mute = 0;              //布尔值，用于控制是否开启静音，默认0关闭
int user_temperature_unit = 0;  //布尔值，用于控制显示的摄氏度还是华氏度，默认为0摄氏度,1为华氏度
int user_scale_situation = 0;   //布尔值，用于控制是否进入电子秤模式(此模式暂未编写，计划中)，默认0否
int user_RH_1 = 15;             //用于开发者测试,单位百分比
int user_temperature_1 = 30;    //用于开发者测试,单位摄氏度
int user_RH_2 = 85;             //用于开发者测试,单位百分比
int user_temperature_2 = 30;    //用于开发者测试,单位摄氏度
int user_time1 = 15;            //用于用户设置其第一次(第二级)提醒的时间,单位s
int user_time2 = 50;            //用于用户设置其第二次(第二级)提醒的时间,单位s
double scale_fact = 5.4915;     //用于开发者测试，压力传感器的校准参数，需要开发者校准

/**********************************************************
                      全局内部变量定义
***********************************************************/
//以下变量部分是需要实时传输给上位机的，有部分不要
float temperature = 88.88;        //最后输出到LCD和上传到上位机的温度数据，单位摄氏度
float RH = 88.88;                 //最后输出到LCD和上传到上位机的相对湿度数据，”单位“百分比
double recordwater[50];           //每一次喝掉的水量，单位g
unsigned long timeb[50];          //每一次的水杯在水杯垫上的停滞时间，单位s
unsigned long no_drink_time = 0;  //记录用户未饮水时间，单位min
unsigned long no_drink_time_s = 0;//记录用户未饮水时间，单位s
int drink_times = 0;              //饮水次数
unsigned long begin_time = 0;     //一次饮水过程中，开始时的时间存档容器
unsigned long end_time = 0;       //一次饮水过程中，结束时的时间存档容器
double begin_weight = 0;          //一次饮水过程中，开始时的压力传感器读数存档容器
double end_weight = 0;            //一次饮水过程中，结束时的压力传感器读数存档容器
double scale_get_units;           //一次loop里面的一次压力传感器读数临时容器
double scale_get_units5;          //一次loop里面的五次压力传感器读数平均值临时容器
unsigned long old_time = 0;       //似乎是用于millis函数延迟的部分，以后会优化
int layflag = 0;                  //布尔值，判断水杯在不在水杯垫上，默认为0
int lastlayflag = 0;              //布尔值，判断上一次loop循环水杯在不在水杯垫上，默认为0
int drinking_leaving = 0;         //布尔值，判断一次饮水过程中是否处于拿起水杯喝水（水杯离开水杯垫）的状态，默认为0
unsigned long temp = 0;           //临时变量，随意调用，用完归零。

unsigned long lastCheckInTime = 0;              //记录上次报到的时间
unsigned long lastUpdateTime = 0;               //记录上次上传数据的时间
const unsigned long postingInterval = 40000;    //若未报到成功，每隔40s向服务器报到一次
const unsigned long updatedInterval = 5000;     //数据上次间隔5s
String inputString = "";                        //串口读取到的内容
boolean stringComplete = false;                 //串口是否读取完毕
boolean CONNECT = true;                         //连接状态
boolean isCheckIn = false;                      //是否连接状态
char* parseJson(char *jsonString);              //定义aJson字符串


double scale_antishake_weight[5];  //用于压力传感器的防抖的临时数组
double scale_antishake_gap = 0;
//以上两个是函数scale_antishake()的变量,此函数目前未完成

void setup()
{

  /**********************************************************
                        串口初始化部分
  ***********************************************************/
  LCD1602_initialize();
  dht.setup(49, DHTesp::DHT11); // Connect DHT sensor to GPIO 17
  Serial.begin(115200);
  Serial.println();
  //压力传感器初始化部分，example里面复制的，还没修改的，以后会重写，初始化阶段不能碰压力传感器，初始化后会设这时的压力为零点
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  // default "128" (Channel A) is used here.
  Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read()); // print a raw reading from the ADC
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20)); // print the average of 20 readings from the ADC
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5)); // print the average of 5 readings from the ADC minus the tare weight (not set yet)
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1); // print the average of 5 readings from the ADC minus tare weight (not set) divided
  // by the SCALE parameter (not set yet)
  scale.set_scale(2280.f); // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();            // reset the scale to 0
  Serial.println("After setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read()); // print a raw reading from the ADC
  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20)); // print the average of 20 readings from the ADC
  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5)); // print the average of 5 readings from the ADC minus the tare weight, set with tare()
  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1); // print the average of 5 readings from the ADC minus tare weight, divided
  // by the SCALE parameter set with set_scale

  LCD1602_welcome();
}

void loop()//目前一次loop循环运行时间不超过2s,可以接受，最好不要超过2s
{
  //delay(dht.getMinimumSamplingPeriod());//忘了什么用了，貌似没用

  RH = dht.getHumidity();
  temperature = dht.getTemperature();
  scale_get_units = scale.get_units() * scale_fact;
  scale_get_units5 = scale.get_units(5) * scale_fact;//尽力减小loop阶段正常运行的时间,故只在loop开头实现读取一次外围数据，重复获取会导致循环时间变长，不要这样做

  if (layflag == 1)
  {
    lastlayflag = 1;
  }
  else if (layflag == 0)
  {
    lastlayflag = 0;
  }//保存一下上一次循环的标识符

  //检测是否有物品在传感器上。如果有，将layflag置1
  if (abs(scale_get_units - scale_get_units5) < 1.5 && scale_get_units >= 10)//前者是为了防抖(效果目前并不明显）
  {
    layflag = 1;//layflag仅仅只能说明有物品放在传感器上，并不能说明进入计时的模式了，所以我们以后希望设置一个按键，按下后，方可进入提醒模式
  }
  else
  {
    layflag = 0;
  }
  Serial.print("layflag:");
  Serial.println(layflag);//方便调试

  if (layflag == 1 && lastlayflag == 0 && drinking_leaving == 0)//第一次放入，一次饮水过程开始
  {
    begin_weight = scale_get_units5;
    begin_time = (unsigned long)millis() / 1000;
  }
  //有物品在传感器上+上一次循环没物品+不处于一次饮水过程中，说明是第一次放入物品，一次饮水开始，记录开始的质量以及时间
  if (layflag == 1 && lastlayflag == 0 && drinking_leaving == 1)//第二次放入，一次饮水过程结束
  {
    end_time = (unsigned long)millis() / 1000;
    timeb[drink_times] = end_time - begin_time;
    recordwater[drink_times] = begin_weight - scale_get_units5;
    no_drink_time_s = (end_time - begin_time) + no_drink_time_s;
    drink_times ++;//完成一次正常的饮水,开始下一次
    drinking_leaving = 0;
    begin_weight = scale_get_units5;
    begin_time = (unsigned long)millis() / 1000;//重新开始记录时间
  }
  if (layflag == 0 && lastlayflag == 1)//如果上一次有物品，但是这一次没有物品，说明什么?说明水杯离开水杯垫，离开水杯垫有几个可能，一个是正常饮水一个是意外碰到，需要排除意外碰到的情况,还没怎么写排除(⊙﹏⊙)
  {
    if (begin_weight - scale_get_units5 > 10)//如果和原来比，比原来小10g以上可认定为饮水过程中，故置其标志位为1
    {
      drinking_leaving = 1;//置一次饮水的”中断期“标志位为1
    }
  }
  //注意：millis()溢出的情况暂未考虑到，以后补。
  //以下均是调试过程中需要的
  Serial.print("drink_times：");
  Serial.println(drink_times);
  Serial.print("recordwater：");
  Serial.println(recordwater[drink_times - 1]);//此处有时候会是0，原因未知，很奇怪
  Serial.print("begin_weight：");
  Serial.println(begin_weight);
  Serial.print("begin_time：");
  Serial.println(begin_time);
  Serial.print("timeb(degrees: s)：");
  Serial.println(timeb[drink_times - 1]);//此处有时候会是0，原因未知，很奇怪
  Serial.print("no_drink_time(degrees: s)：");
  Serial.println(no_drink_time_s);
  Serial.print("no_drink_time(degrees: min)：");
  Serial.println(no_drink_time);
  /*测试用for (int i = 0; i < 10; i++)
    {
    Serial.println(recordwater[i]);
    }
    for (int i = 0; i < 10; i++)
    {
    Serial.println(timeb[i]);
    }
  */
  /**********************************************************
                         提醒部分
  ***********************************************************/
  if (layflag == 1 && lastlayflag == 1)//连续两次检测到有物品稳定地在上面
  {
    if (RH > user_RH_1 && temperature < user_temperature_1)//符合当前湿度大于用户设定的湿度1和符合当前温度小于用户设定的温度1
    {
      if ((unsigned long)millis() / 1000 - begin_time - user_time1 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time1 > 0)//第一级提醒，目前设定的是15s
      {
        if (user_mute == 0)//检测外部控制变量
        {
          remind1_s();
        }
        else
        {
          remind1_mute();
        }
      }
      if ((unsigned long)millis() / 1000 - begin_time - user_time2 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time2 > 0)//第二级提醒,目前设定是30s
      {
        if (user_mute == 0)
        {
          remind1_s();
        }
        else
        {
          remind1_mute();
        }
      }
    }
    if (RH < user_RH_2 && temperature > user_temperature_2)//符合当前湿度小于用户设定的湿度2和符合当前温度大于用户设定的温度2
    {
      if ((unsigned long)millis() / 1000 - begin_time - user_time1 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time1 > 0)//第一级提醒，目前设定的是15s
      {
        if (user_mute == 0)
        {
          remind1_s();
        }
        else
        {
          remind1_mute();
        }
      }
      if ((unsigned long)millis() / 1000 - begin_time - user_time2 < 15 && (unsigned long)millis() / 1000 - begin_time - user_time2 > 0)//第二级提醒
      {
        if (user_mute == 0)
        {
          remind1_s();
        }
        else
        {
          remind1_mute();
        }
      }
    }
  }

  LCD1602_usual();
  //LCD1602_usual_balance();

  //以下为了测试LED灯
  pinMode(51, OUTPUT);//LED
  digitalWrite(51, HIGH);

  /**********************************************************
                        串口输出部分
  ***********************************************************/
  Serial.print("one reading:\t");
  Serial.print(scale_get_units, 2);
  Serial.print("\n");
  //  Serial.print("| average:\t");
  // Serial.println(scale.get_units(5), 2);
  // Serial.print("later:\t");
  // Serial.println(scale.get_units(5) * scale_fact, 2);

  Serial.println("Status\tHumidity (%)\tTemperature (C)\t");
  //Serial.print(dht.getStatusString());
  Serial.print("\t");
  Serial.print(RH, 1);
  Serial.print("\t\t");
  Serial.print(temperature, 1);
  Serial.print("\n");
  /*Serial.print("\t\t");
    Serial.print(dht.toFahrenheit(temperature), 1);
    Serial.print("\t\t");
    Serial.print(dht.computeHeatIndex(temperature, RH, false), 1);
    Serial.print("\t");
    Serial.println(dht.computeHeatIndex(dht.toFahrenheit(temperature), RH, true), 1);
  */
  /*old_time = 0;
    while (millis() < old_time + 1500) {}
    old_time = millis();*/

  /**********************************************************
                         贝壳物联代码
  ***********************************************************/
  if(millis() - lastCheckInTime > postingInterval||lastCheckInTime == 0)
  {
    CheckIn();
  }
  if(millis() - lastUpdateTime > updatedInterval)
  {
    updatel(DEVICEID,"17229",(float)layflag);                         //上传数据”是否在使用中“
    updatel(DEVICEID,"17232",(float)no_drink_time);                   //上传数据”未喝水时间（min)"
    updatel(DEVICEID,"17230",(float)no_drink_time_s);                 //上传数据"未喝水时间(s)"
    updatel(DEVICEID,"17231",(float)drink_times);                     //上传数据"饮水次数“
    updatel(DEVICEID,"13949",(float)recordwater[drink_times]);        //上传数据”饮水量“
  }
  serialEvent();                              //调用serialEvent()函数获取网站传输的指令
  if(stringComplete)
  {
    inputString.trim();
    //Serial. println( inputString);
    if(inputString == "CLOSED")               // 如果接收到网站的指令为CLOSED,则停止连接
    {
      Serial. println("connect closed!");
      CONNECT = false;
      isCheckIn = false;
    }
    else                                     //否则，处理接收到的命令
    {
      int len = inputString.length() + 1;
      if (inputString.startsWith("{") && inputString.endsWith("}"))
      {
        char jsonString[len];
        inputString.toCharArray(jsonString, len);
        aJsonObject * msg = aJson.parse(jsonString);
        processMessage( msg) ;              //处理接收到的JSON数据
        aJson.deleteItem(msg);
      }
   }
    inputString = "";
    stringComplete = false;
 }
}


/**********************************************************
                        自定义函数
***********************************************************/

/***********************************************************
    函数名称：remind1()
    函数功能：暂定第一种提醒方式,先蜂鸣器、LED灯、LCD显示内容”已<变量>min（或者<变量>h<变量>min）未饮水，马上饮水！！！“常亮3s.
    然后蜂鸣器、LED灯、LCD屏幕显示内容开始闪烁，闪烁间隔0.5s,总计闪烁时间20s，函数结束
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602，，现有bug，执行一次后原设定的常亮3s不再出现，原因不详，估计与millis函数有关,     20200902 吴韦举  已修复
         20200902发现新bug，当no_drink_time为一位数时，感叹号显示有点问题，小bug,赶工暂未修复
***********************************************************/
void remind1()
{
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  int led1 = 51;    //第一个lcd灯使用51号digital端口
  int buzzer1 = 53; //53号数字端口，与有源蜂鸣器的VCC连接，是蜂鸣器的主供电口。
  int buzzer2 = 52; //52号数字端口，低电平触发蜂鸣，是蜂鸣器的控制端口。

  pinMode(buzzer1, OUTPUT);
  digitalWrite(buzzer1, HIGH);
  digitalWrite(buzzer2, HIGH);
  pinMode(buzzer2, OUTPUT);
  pinMode(led1, OUTPUT);

  lcd.init();
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  uint8_t yi[8] = {0x1F, 0x01, 0x11, 0x1F, 0x10, 0x10, 0x11, 0x1F};
  uint8_t yin[8] = {0x0A, 0x1F, 0x15, 0x0A, 0x0A, 0x0B, 0x0F, 0x0D};
  uint8_t shui[8] = {0x04, 0x04, 0x15, 0x0E, 0x0E, 0x15, 0x0C, 0x04};
  uint8_t wei[8] = {0x04, 0x0E, 0x04, 0x1F, 0x0E, 0x15, 0x15, 0x15};
  uint8_t ma[8] = {0x1E, 0x0A, 0x0A, 0x0F, 0x01, 0x1F, 0x01, 0x03};
  uint8_t shang[8] = {0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x1F}; //字库像素点的定义
  lcd.createChar(0, yi);
  lcd.createChar(1, yin);
  lcd.createChar(2, shui);
  lcd.createChar(3, wei);
  lcd.createChar(4, ma);
  lcd.createChar(5, shang);
  lcd.clear();
  lcd.write(byte(0));
  if (no_drink_time <= 60)
  {
    lcd.print(no_drink_time);
    lcd.print("min");
  }
  else
  {
    lcd.print((int)no_drink_time / 60);
    lcd.print("h");
    lcd.print(no_drink_time - ((int)no_drink_time / 60) * 60);
    lcd.print("min");
  }
  lcd.write(byte(3));
  lcd.write(byte(1));
  lcd.write(byte(2));
  lcd.print(",");
  lcd.write(byte(4));
  lcd.write(byte(5));
  lcd.write(byte(1));
  if (no_drink_time > 600)
  {
    lcd.setCursor(0, 1);
  } //LCD1602一行显示的字符不够我们显示的，只能手动换行
  lcd.write(byte(2));
  if (no_drink_time > 60 && no_drink_time <= 600)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");
  lcd.print("!");
  if (no_drink_time <= 60)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");

  digitalWrite(buzzer2, LOW);
  digitalWrite(led1, HIGH);
  old_time = millis();
  while (millis() < old_time + 3000)
  {
  }
  old_time = millis(); //延时3s
  // if (millis() > old_time + 3000) {
  //   old_time = millis();
  digitalWrite(led1, LOW);
  digitalWrite(buzzer2, HIGH);
  while (millis() < old_time + 20000)
  {
    digitalWrite(buzzer2, LOW);
    digitalWrite(led1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
    digitalWrite(buzzer2, HIGH);
    digitalWrite(led1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
  }
  old_time = millis();
  //  }
}

/***********************************************************
    函数名称：remind1_s()
    函数功能：remind1的 秒数 显示版，
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602，，现有bug，执行一次后原设定的常亮3s不再出现，原因不详，估计与millis函数有关,     20200902 吴韦举 已修复
         20200902发现新bug，当no_drink_time为一位数时，感叹号显示有点问题，小bug,赶工暂未修复
         仅用于开发者测试！！！
***********************************************************/
void remind1_s()
{
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  int led1 = 51;    //第一个lcd灯使用51号digital端口
  int buzzer1 = 53; //53号数字端口，与有源蜂鸣器的VCC连接，是蜂鸣器的主供电口。
  int buzzer2 = 52; //52号数字端口，低电平触发蜂鸣，是蜂鸣器的控制端口。

  pinMode(buzzer1, OUTPUT);
  digitalWrite(buzzer1, HIGH);
  digitalWrite(buzzer2, HIGH);
  pinMode(buzzer2, OUTPUT);
  pinMode(led1, OUTPUT);

  lcd.init();
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  uint8_t yi[8] = {0x1F, 0x01, 0x11, 0x1F, 0x10, 0x10, 0x11, 0x1F};
  uint8_t yin[8] = {0x0A, 0x1F, 0x15, 0x0A, 0x0A, 0x0B, 0x0F, 0x0D};
  uint8_t shui[8] = {0x04, 0x04, 0x15, 0x0E, 0x0E, 0x15, 0x0C, 0x04};
  uint8_t wei[8] = {0x04, 0x0E, 0x04, 0x1F, 0x0E, 0x15, 0x15, 0x15};
  uint8_t ma[8] = {0x1E, 0x0A, 0x0A, 0x0F, 0x01, 0x1F, 0x01, 0x03};
  uint8_t shang[8] = {0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x1F}; //字库像素点的定义
  lcd.createChar(0, yi);
  lcd.createChar(1, yin);
  lcd.createChar(2, shui);
  lcd.createChar(3, wei);
  lcd.createChar(4, ma);
  lcd.createChar(5, shang);
  lcd.clear();
  lcd.write(byte(0));
  if ((unsigned long)millis() / 1000 - begin_time <= 60)
  {
    lcd.print((unsigned long)millis() / 1000 - begin_time);
    lcd.print("s");
  }
  else
  {
    lcd.print((int)((unsigned long)millis() / 1000 - begin_time) / 60);
    lcd.print("h");
    lcd.print(((unsigned long)millis() / 1000 - begin_time) - ((int)((unsigned long)millis() / 1000 - begin_time) / 60) * 60);
    lcd.print("s");
  }
  lcd.write(byte(3));
  lcd.write(byte(1));
  lcd.write(byte(2));
  lcd.print(",");
  lcd.write(byte(4));
  lcd.write(byte(5));
  lcd.write(byte(1));
  if (((unsigned long)millis() / 1000 - begin_time) > 600)
  {
    lcd.setCursor(0, 1);
  } //LCD1602一行显示的字符不够我们显示的，只能手动换行
  lcd.write(byte(2));
  if (((unsigned long)millis() / 1000 - begin_time) > 60 && ((unsigned long)millis() / 1000 - begin_time) <= 600)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");
  lcd.print("!");

  digitalWrite(buzzer2, LOW);
  digitalWrite(led1, HIGH);
  old_time = millis();
  while (millis() < old_time + 3000)
  {
  }
  old_time = millis(); //延时3s
  // if (millis() > old_time + 3000) {
  //   old_time = millis();
  digitalWrite(led1, LOW);
  digitalWrite(buzzer2, HIGH);
  while (millis() < old_time + 20000)
  {
    digitalWrite(buzzer2, LOW);
    digitalWrite(led1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
    digitalWrite(buzzer2, HIGH);
    digitalWrite(led1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
  }
  old_time = millis();
  //  }
}

/***********************************************************
    函数名称：remind1_mute()
    函数功能：remind1静音提醒版，比remind少一个蜂鸣器，其它一致。
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602
***********************************************************/
void remind1_mute()
{
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  int led1 = 51; //第一个lcd灯使用51号端口

  pinMode(led1, OUTPUT);
  lcd.init();

  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  uint8_t yi[8] = {0x1F, 0x01, 0x11, 0x1F, 0x10, 0x10, 0x11, 0x1F};
  uint8_t yin[8] = {0x0A, 0x1F, 0x15, 0x0A, 0x0A, 0x0B, 0x0F, 0x0D};
  uint8_t shui[8] = {0x04, 0x04, 0x15, 0x0E, 0x0E, 0x15, 0x0C, 0x04};
  uint8_t wei[8] = {0x04, 0x0E, 0x04, 0x1F, 0x0E, 0x15, 0x15, 0x15};
  uint8_t ma[8] = {0x1E, 0x0A, 0x0A, 0x0F, 0x01, 0x1F, 0x01, 0x03};
  uint8_t shang[8] = {0x04, 0x04, 0x04, 0x07, 0x04, 0x04, 0x04, 0x1F}; //字库像素点的定义
  lcd.createChar(0, yi);
  lcd.createChar(1, yin);
  lcd.createChar(2, shui);
  lcd.createChar(3, wei);
  lcd.createChar(4, ma);
  lcd.createChar(5, shang);
  lcd.clear();
  lcd.write(byte(0));
  if (no_drink_time <= 60)
  {
    lcd.print(no_drink_time);
    lcd.print("min");
  }
  else
  {
    lcd.print((int)no_drink_time / 60);
    lcd.print("h");
    lcd.print(no_drink_time - ((int)no_drink_time / 60) * 60);
    lcd.print("min");
  }
  lcd.write(byte(3));
  lcd.write(byte(1));
  lcd.write(byte(2));
  lcd.print(",");
  lcd.write(byte(4));
  lcd.write(byte(5));
  lcd.write(byte(1));
  if (no_drink_time > 600)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.write(byte(2));
  if (no_drink_time > 60 && no_drink_time <= 600)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");
  lcd.print("!");
  if (no_drink_time <= 60)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");

  digitalWrite(led1, HIGH);
  old_time = millis();
  while (millis() < old_time + 3000)
  {
  }
  old_time = millis(); //延时3s
  // if (millis() > old_time + 3000) {
  //   old_time = millis();
  digitalWrite(led1, LOW);
  while (millis() < old_time + 20000)
  {
    digitalWrite(led1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
    digitalWrite(led1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
    }
    old_time1 = millis();
  }
  old_time = millis();
  //  }
}

/***********************************************************
    函数名称：LCD1602_usual()
    函数功能：用于显示温度数据和相对湿度数据，在未达到提醒要求的时候，使用此函数在LED上显示数据。
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602
***********************************************************/
void LCD1602_usual()
{
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  lcd.init();
  lcd.print("Temp:       RH:");
  lcd.setCursor(0, 1);
  if (user_temperature_unit == 0)
  {
    lcd.print(temperature);
    lcd.print((char)223); //输出°
    lcd.print("C");
  } //输出摄氏度
  if (user_temperature_unit == 1)
  {
    lcd.print((float)temperature * 1.8 + 32.00);
    lcd.print((char)223);
    lcd.print("F");
  } //输出华氏度
  lcd.setCursor(15, 1);
  lcd.rightToLeft();
  lcd.print((char)37); //输出%百分号
  lcd.setCursor(10, 1);
  lcd.leftToRight();
  lcd.print(RH);       //RH若占5个字符，光标此时位于(9，1),RH如果不是5个字符，此处需要重写。
  lcd.setCursor(1, 0); //光标还原
}

/***********************************************************
    函数名称：LCD1602_initialize()
    函数功能：用于刚开机时，在LCD1602屏幕上显示initializing
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602
***********************************************************/
void LCD1602_initialize()
{
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  lcd.init();
  lcd.print("Initializing");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");
  lcd.setCursor(1, 0); //光标还原
}

/***********************************************************
    函数名称：LCD1602_welcome()
    函数功能：用于初始化完毕，欢迎使用
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602
***********************************************************/
void LCD1602_welcome()
{
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  lcd.init();
  lcd.print("Welcome to");
  lcd.setCursor(0, 1);
  lcd.print("CUPMAT   V1.0");
  lcd.setCursor(1, 0); //光标还原
}

/***********************************************************
    函数名称：LCD1602_usual_balance()
    函数功能：称重显示
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：测试用，需要初始化LCD1602
***********************************************************/
void LCD1602_usual_balance()
{
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  lcd.init();
  lcd.print("weight:");
  lcd.setCursor(0, 1);
  lcd.print(scale.get_units() * scale_fact);
  lcd.print("g");
  lcd.setCursor(0, 1); //光标还原
}

/***********************************************************
    函数名称：scale_antishake()
    函数功能：检测是否有物品在传感器上。如果有，将layflag置1,并且能够防抖
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：20200831 吴韦举编写，
    此函数工作不正常，请勿使用
***********************************************************/
void scale_antishake()
{
  for (int i = 0; i < 5; i++)
  {
    scale_antishake_weight[i] = scale.get_units() * scale_fact;
  }
  for (int i = 0; i < 5; i++)
  {
    for (int j = 4; j >= 0; j--)
    {
      if (scale_antishake_gap < abs(scale_antishake_weight[i] - scale_antishake_weight[j]))
      {
        scale_antishake_gap = abs(scale_antishake_weight[i] - scale_antishake_weight[j]);
      }
    }
  }
  if (scale_antishake_gap < 1 && scale.get_units()*scale_fact > 10)
  {
    layflag = 1;
  }
  else
  {
    layflag = 0;
  }
  //scale_antishake_gap = 0;
  for (int i = 0; i < 4; i++)
  {
    scale_antishake_weight[i] = 0;
  }
  Serial.print(layflag);
  Serial.print("\t\n");
  Serial.print(scale_antishake_gap);
  scale_antishake_gap = 0;
}
/***********************************************************
    函数名称：CheckIn()
    函数功能：连接失败后重新连接
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：如果连接失败，则通过AT指令重新连接，否则输入设备ID和密码进行连接
***********************************************************/
void CheckIn()
{
  if(!CONNECT)                  //如果连接失败，则通过AT指令重新连接
  {
    Serial.print("++ +");
    delay(500);
    Serial.print("\r\n");
    delay(1000);
    Serial.print("AT+RST\r\n");
    delay(6000); 
    CONNECT = true;
    lastCheckInTime == 0;   
  }
  else                           //否则输入设备ID和密码进行连接
  {
    Serial.print("{\"M\":\"checkin\",\"ID\":\"");
    Serial.print(DEVICEID);
    Serial.print("\",\"K\":\"");
    Serial.print(APIKEY);
    Serial.print("\"}\r\n"); 
    lastCheckInTime = millis();
  }  
}
/***********************************************************
    函数名称：updatel()
    函数功能：上传数据至贝壳物联
    调用函数：
    输入参数：
    输出参数：
    返回值：无
***********************************************************/
void updatel(String did, String inputid, float value)
{
    Serial.print("{\"M\":\"update\",\"ID\":\"");
    Serial.print(did);
    Serial.print("\",\"V\":{\"");
    Serial.print(inputid);
    Serial.print("\":\"");
    Serial.print(value);
    Serial.println("\"}}");
    lastCheckInTime = millis();   
    lastUpdateTime = millis();       
}
/***********************************************************
    函数名称：serialEvent()
    函数功能：获取网络传输的指令，读取网站的命令
    调用函数：
    输入参数：
    输出参数：
    返回值：无
***********************************************************/
void serialEvent()
{
  while(Serial.available())
  {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if(inChar == '\n')
    {
      stringComplete = true; 
    }
  }  
}
/***********************************************************
    函数名称：processMessage()
    函数功能：处理接受来自网站的命令
    调用函数：
    输入参数：
    输出参数：
    返回值：无
***********************************************************/
void processMessage(aJsonObject *msg)
{
  aJsonObject*method = aJson.getObjectItem(msg,"M");
  aJsonObject*content = aJson.getObjectItem(msg,"C");
  aJsonObject*client_id = aJson.getObjectItem(msg,"ID");
  //char*st = aJson.print(msg);
  if (!method) 
  {
    return;
  }
  //Serial. println(st);
  //free(st);
    String M = method -> valuestring;
    if (M == "checkinok" )
    {
      isCheckIn = true;
    }
}
