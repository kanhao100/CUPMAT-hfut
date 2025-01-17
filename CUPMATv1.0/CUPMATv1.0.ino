/**********************************************************
    项目记录
    项目：校级2020大创_智能物联网水杯垫
    版 本：V1.0
    说 明：
    20200902 吴**  为什么布尔值不用bool，其实都一样，去看看C标准就知道bool其实就是int，
                    arduino执行的C语言标准比keil51新多了，很多写法和keil不一样
                    维护此项目请做好测试以及写好修改说明
                    下一步工作使用三线程，我将继续修复bug,以及重写导入的库文件所用到的函数以及简化代码，向**将为此项目添加esp8266串口数据传输的功能，成**继续完成提醒部分的程序
    bug备忘录：

    工作备忘录：
    机械结构（多个LED灯，设计一个重置按键，以及开关按键
    物联网彻底实现
    bug修完
    经费记录本
    ppt做好
    布尔值改为bool
    布尔值改为bool
    布尔值改为bool
    布尔值改为bool
    布尔值改为bool
    布尔值改为bool
    修改记录:20200830  成**   创建
            20200831 吴**  格式优化；代码整合；细节优化；避免使用delay()函数，改用mills()相关的。
                            试图编写scale_antishake()函数，失败，暂时弃用了
            20200902 吴**  添加LCD1602_initialize(),LCD1602_welcome()两个开机显示函数
                            用remind1_s()暂时替代remind1()，前者显示秒数，因为在开发者调试过程中，分钟时间过长，不方便，开发者使用后者调试更方便
                            主程序方面，完成了饮水过程的准确识别以及饮水过程中的时间差与重量差的读取，提醒部分，计划书写得并不一定合理，故暂只象征性编写了小部分程序，此部分以后编程难度很小
                            经测试，主体功能似乎实现了！版本1.0  bug应该会很多，以后再调
            20200904 向**  添加物联网代码
            20200905 向**  修改贝壳物联代码，修改了几个bug，仍有几个bug未修复，目前饮水次数和用户未饮水时间（min)上传至贝壳物联出现困难，以后改进。
            20200906 成**    初步（划重点）完善提醒部分代码
            20200919 吴**   1.增加电子秤模式。
                             2.删除了HX711.h库文件，通过阅读HX711 datasheet自行编写了HX711_read_raw, HX711_read, HX711_read_raw_average, HX711_read_average,
                             HX711_initialize五个函数，并且完成调试。
                             3.删除了部分无用代码，但仍未进行系统的bug排查。
            20200923 叶**  删除了DHT相关库函数，并根据相关库函数、DHT11 datasheet和自身程序的具体需求编写了DHT_read,DHT_readTemperature,DHT_readHumdity,DHT_begin,
                             DHT_expectPulse五个函数，并且完成调试，注解DHT_read函数数据接收的相关原理和具体步骤
            20200927 吴**  尝试解决了提醒时拿走水杯应该立即停止提醒的问题，改变了部分端口的宏定义参数名字,优化输出部分函数，将remind_mute删除，将静音提醒部分融入remind系列函数，主程
                            序中无需再做user_mute的取值判定。未进行调试。
            20200929        上一次的工作解决失败，需要重新修改。
            20201020 吴**  添加了异常处理提示的代码，解决了在硬件连线不正确的时候，无法初始化，无法串口输出，又没有报错，在各传感器的读取函数中增加了错误提示（串口输出），便于开发者调试。
            20201111 吴**  修改了部分元件的连接端口，连线更合理，修复了recordweight可能会出现负质量的问题，出现负质量不予认定为一次饮水过程的结束。remind系列函数进行了部分优化，另外
                            经测试LCD1602_ususl()此函数耗时长，需要大概1s，是影响loop循环速度的主要影响问题，测得第二个耗时长的函数为HX711_read_average(int),除去这两个函数以后
                            loop循环可在100ms左右很快完成，这些数据为以后的工作提供了参考。
            20201112 吴**  解决了提醒时拿走水杯应该立即停止提醒的问题，解决方案是使用了MsTimer2库实现便捷的实现了定时器中断，并写了一个中断函数detection(),用来在进入remind系列函数
                            的时候对HX711每隔500ms进行一次读取。可能存在的问题是detection()耗时过长，导致3+20s的remind1提醒的时间不够精确，有待以后研究解决。


**********************************************************/

/**********************************************************
                        库文件导入
***********************************************************/
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //这两个库是与LCD1602有关的库文件
#include <aJSON.h>
#include <MsTimer2.h>

/**********************************************************
                        宏定义
***********************************************************/
#define DHT11_PIN 4 //DHT11数据线
#define DOUT_PIN 2   //HX711数据线
#define SCK_PIN 3    //HX711时钟线
#define LED1 52      //第一个lcd灯使用52号digital GPIO
#define BUZZER1 51   //51号digital GPIO，与有源蜂鸣器的VCC连接，是蜂鸣器的主供电口。
#define BUZZER2 53   //53号digital GPIO，低电平触发蜂鸣，是蜂鸣器的控制端口。
//uint8_t DHT11_PIN = 49;
/*完全重现此测试项目需要如下连接：
  DHT11 DATA----D4，
  HX711 DT----D2  SCK----D3
  LED  VCC----D52
  LCD1602_I2C  SCL----SCL   SDA----SDA
  BEEP I/O----D51  VCC---D53
  Esp8266-01 RX----TX,TX----RX
*/

/**********************************************************
                      元件初始化
***********************************************************/
LiquidCrystal_I2C lcd(0x27, 16, 2);
//I2C LCD显示屏初始化,本来是打算放在函数里面的，但是没办法，放在函数里面会出现一些问题，故没有封在函数里面。
/**********************************************************
                      贝壳物联接口定义
***********************************************************/
/*向**的测试接口*/
/*
  String DEVICEID = "19242";      //本设备在贝壳物联上的设备ID
  String APIKEY = "712c5cad8";    //设备密码
  String WORKID = "17229";        //接口“是否在使用中”ID
  String NO_DRINK_SID = "17230";  //接口”未饮水时间（s）“ID
  String NO_DRINK_MINID = "17232";//接口”未饮水时间（min）“ID
  String DRINK_TIMEID = "17231 "; //接口”饮水次数“ID
  String RECORDDRINKID = "13949 "; //接口”饮水量“ID
*/
/*吴**的测试接口*/
String DEVICEID = "19253";      //本设备在贝壳物联上的设备ID
String APIKEY = "2a3f3d417";    //设备密码
String WORKID = "17243";        //接口“是否在使用中”ID
String NO_DRINK_SID = "17244";  //接口”未饮水时间（s）“ID
String NO_DRINK_MINID = "17247";//接口”未饮水时间（min）“ID
String DRINK_TIMEID = "17246";  //接口”饮水次数“ID
String RECORDDRINKID = "17245"; //接口”饮水量“ID

/**********************************************************
                      全局外部控制变量定义
***********************************************************/
//用于放置微信小程序里面用户可以自主设置的变量，现规定，凡是此类变量均在前面加user_
//此中的变量均需要通过ESP8266传输的,并且要求是可以在上位机上控制这些变量的值
bool user_backlight = 1;         //布尔值，用于控制LCD1602的背光显示，默认1开启
bool user_blink = 1;             //布尔值，用于控制LCD1602的闪烁情况，默认1开启
bool user_mute = 0;              //布尔值，用于控制是否开启静音，默认0关闭
bool user_temperature_unit = 0;  //布尔值，用于控制显示的摄氏度还是华氏度，默认为0摄氏度,1为华氏度
bool user_scale_situation = 0;   //布尔值，用于控制是否进入电子秤模式，默认0否
int user_RH = 15;              //用于开发者测试,单位百分比
int user_temperature = 30;     //用于开发者测试,单位摄氏度
double scale_factor = 408.36;   //用于开发者测试，压力传感器的校准参数，需要开发者校准
int user_time1 = 20;
int user_time2 = 30;
int user_time3 = 40;
int user_time4 = 50;
int user_time5 = 20;
int user_time6 = 60;    //设置的六个提醒时间

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

double HX711_initial_weight;

double scale_antishake_weight[5];  //用于压力传感器的防抖的临时数组
double scale_antishake_gap = 0;
//以上两个是函数scale_antishake()的变量,此函数目前未完成

uint8_t dht_data[5];
uint32_t dht_lastreadtime, dht_maxcycles;
bool dht_lastresult;
uint8_t dht_pullTime;
//以上均为DHT11函数相关参数

void setup()
{

  /**********************************************************
                        串口初始化部分
  ***********************************************************/
  LCD1602_initialize();
  pinMode(DOUT_PIN, INPUT_PULLUP);
  pinMode(SCK_PIN, OUTPUT);
  Serial.begin(115200);
  Serial.println();
  HX711_initialize();//初始化阶段不能碰压力传感器，初始化后会设这时的压力为零点
  DHT_begin();
  LCD1602_welcome();
}

void loop()//目前一次loop循环运行时间不超过2s,可以接受，最好不要超过2s
{
  if (user_scale_situation == 0)//控制进入电子秤模式
  {
    RH = DHT_readHumidity();
    temperature = DHT_readTemperature();
    scale_get_units = HX711_read();
    scale_get_units5 = HX711_read_average(5);//尽力减小loop阶段正常运行的时间,故只在loop开头实现读取一次外围数据，重复获取会导致循环时间变长，不要这样做

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
      if (begin_weight - scale_get_units5 > 1)
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
      else
      {
        // ...
      }
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
    /*临时测试用for (int i = 0; i < 10; i++)
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
      if (RH > user_RH && temperature < user_temperature)//符合lv_1的温湿度提醒条件
      {
        if ((unsigned long)millis() / 1000 - begin_time - user_time5 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time5 > 0 && drinking_leaving == 0) //第一级提醒
        {
          remind1_s();
        }
        if ((unsigned long)millis() / 1000 - begin_time - user_time6 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time6 > 0 && drinking_leaving == 0) //第二级提醒
        {
          remind1_s();
        }
      }
      else if (RH < user_RH && temperature < user_temperature)//符合lv_2的温湿度提醒条件
      {
        if ((unsigned long)millis() / 1000 - begin_time - user_time3 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time3 > 0 && drinking_leaving == 0)//第一级提醒
        {
          remind1_s();
        }
        if ((unsigned long)millis() / 1000 - begin_time - user_time4 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time4 > 0 && drinking_leaving == 0) //第二级提醒
        {
          remind1_s();
        }
      }
      if (RH > user_RH && temperature > user_temperature)
      {
        if ((unsigned long)millis() / 1000 - begin_time - user_time3 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time3 > 0 && drinking_leaving == 0)//第一级提醒
        {
          remind1_s();
        }
        if ( (unsigned long)millis() / 1000 - begin_time - user_time4 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time4 > 0 && drinking_leaving == 0) //第二级提醒
        {
          remind1_s();
        }
      }
      if (RH < user_RH && temperature > user_temperature) //符合lv_3的温湿度提醒条件
      {
        if ((unsigned long)millis() / 1000 - begin_time - user_time1 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time1 > 0 && drinking_leaving == 0) //第一级提醒
        {
          remind1_s();
        }
        if ((unsigned long)millis() / 1000 - begin_time - user_time2 < 20 && (unsigned long)millis() / 1000 - begin_time - user_time2 > 0 && drinking_leaving == 0) //第二级提醒
        {
          remind1_s();
        }
      }
    }


    LCD1602_usual();//经测试，此为loop循环占用时间最多的语句，大约需要1秒才能执行完。

    //以下为了测试LED灯
    pinMode(51, OUTPUT);//LED
    digitalWrite(51, HIGH);

    /**********************************************************
                          串口输出部分
    ***********************************************************/
    Serial.print("one reading:\t");
    Serial.println(scale_get_units, 2);
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
    /**********************************************************
                           贝壳物联代码
    ***********************************************************/
    if (millis() - lastCheckInTime > postingInterval || lastCheckInTime == 0)
    {
      CheckIn();
    }
    if (millis() - lastUpdateTime > updatedInterval)
    {
      /*向**的测试接口*/
      /*
        updatel(DEVICEID,"17229",(float)layflag);                         //上传数据”是否在使用中“
        updatel(DEVICEID,"17232",(float)no_drink_time);                   //上传数据”未喝水时间（min)"
        updatel(DEVICEID,"17230",(float)no_drink_time_s);                 //上传数据"未喝水时间(s)"
        updatel(DEVICEID,"17231",(float)drink_times);                     //上传数据"饮水次数“
        updatel(DEVICEID,"13949",(float)recordwater[drink_times]);        //上传数据”饮水量“
      */
      /*吴**的测试接口*/
      updatel(DEVICEID, "17243", (float)layflag);                       //上传数据”是否在使用中“
      updatel(DEVICEID, "17244", (float)no_drink_time);                 //上传数据”未喝水时间（min)"
      updatel(DEVICEID, "17247", (float)no_drink_time_s);               //上传数据"未喝水时间(s)"
      updatel(DEVICEID, "17246", (float)drink_times);                   //上传数据"饮水次数“
      updatel(DEVICEID, "17245", (float)recordwater[drink_times]);      //上传数据”饮水量“

    }
    serialEvent();                              //调用serialEvent()函数获取网站传输的指令
    if (stringComplete)
    {
      inputString.trim();
      //Serial. println( inputString);
      if (inputString == "CLOSED")              // 如果接收到网站的指令为CLOSED,则停止连接
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
  else
  {
    LCD1602_usual_scale();
    Serial.println(HX711_read());
  }
}
/**********************************************************
                        自定义函数
***********************************************************/
/*********************************************************
    函数名称：detection
    函数功能：读取HX711数据，并判断处于什么饮水期，定时器中断函数。
    调用函数：很多
    输入参数：无
    输出参数：
    返回值：
********************************************************/
void detection() {
  for (int i = 0; i < 2; i++)
  {
    Serial.println("Enter interrupt mode successly!!!");
    scale_get_units = HX711_read();
    if (layflag == 1)
    {
      lastlayflag = 1;
    }
    else if (layflag == 0)
    {
      lastlayflag = 0;
    }//保存一下上一次循环的标识符

    //检测是否有物品在传感器上。如果有，将layflag置1
    if (scale_get_units >= 10)
    {
      layflag = 1;//layflag仅仅只能说明有物品放在传感器上，并不能说明进入计时的模式了，所以我们以后希望设置一个按键，按下后，方可进入提醒模式
    }
    else
    {
      layflag = 0;
    }
    if (layflag == 1 && lastlayflag == 0 && drinking_leaving == 0)//第一次放入，一次饮水过程开始
    {
      begin_weight = scale_get_units;
      begin_time = (unsigned long)millis() / 1000;
    }//有物品在传感器上+上一次循环没物品+不处于一次饮水过程中，说明是第一次放入物品，一次饮水开始，记录开始的质量以及时间
    if (layflag == 1 && lastlayflag == 0 && drinking_leaving == 1)//第二次放入，一次饮水过程结束
    {
      end_time = (unsigned long)millis() / 1000;
      drinking_leaving = 0;
      begin_weight = scale_get_units;
      begin_time = (unsigned long)millis() / 1000;//重新开始记录时间
    }
    if (layflag == 0 && lastlayflag == 1)//如果上一次有物品，但是这一次没有物品，说明什么?说明水杯离开水杯垫，离开水杯垫有几个可能，一个是正常饮水一个是意外碰到，需要排除意外碰到的情况,还没怎么写排除(⊙﹏⊙)
    {
      if (begin_weight - scale_get_units > 10)//如果和原来比，比原来小10g以上可认定为饮水过程中，故置其标志位为1
      {
        drinking_leaving = 1;//置一次饮水的”中断期“标志位为1
      }
    }
  }
}

/*********************************************************
    函数名称：DHT_read
    函数功能：检测两次读数间隔是否超过2s，完成一次从DHT11获取
             相关数据的操作
    调用函数：DHT_expectPulse
    输入参数：无
    输出参数：bool
    返回值：dht_lastresult
********************************************************/
bool DHT_read() {
  uint32_t currenttime = millis();
  if (((currenttime - dht_lastreadtime) < 1000))
  {
    return dht_lastresult;
  }
  dht_lastreadtime = currenttime;
  dht_data[0] = dht_data[1] = dht_data[2] = dht_data[3] = dht_data[4] = 0;
  pinMode(DHT11_PIN, INPUT_PULLUP);
  delay(1);
  //上拉电阻使得DHT11的DATA的引脚保持高电平，此时DHT11的引脚状态处于输入状态，时刻检测外部信号
  pinMode(DHT11_PIN, OUTPUT);
  digitalWrite(DHT11_PIN, LOW);
  delay(20);
  //微处理器置低电平，持续时间应该超过18ms，最好不超过30ms，设置I/O为输入状态，DHT11接受信号并准备做出应答
  uint32_t cycles[80];
  pinMode(DHT11_PIN, INPUT_PULLUP);
  delayMicroseconds(dht_pullTime);
  //转变接口状态，此时使DHT11的DATA的引脚处于上拉电阻的状态，低电平信号随之变成高电平
  //DHT11的DATA转变为输出状态，首先它输出83ms的低电平脉冲，随后紧跟87ms的高电平脉冲，此时微处理器的I/O处于
  //输入状态，接受到83ms的低电平应答后就会等待87ms高电平后的数据输入
  if (DHT_expectPulse(LOW) == UINT32_MAX)
  {
    Serial.println(F("DHT timeout waiting for start signal low pulse."));
    dht_lastresult = false;
    return dht_lastresult;
  }
  if (DHT_expectPulse(HIGH) ==  UINT32_MAX)
  {
    Serial.println(F("DHT timeout waiting for start signal high pulse."));
    dht_lastresult = false;
    return dht_lastresult;
  }
  //检测高低电平脉冲是否正常
  for (int i = 0; i < 80; i += 2)
  {
    cycles[i] = DHT_expectPulse(LOW);
    cycles[i + 1] = DHT_expectPulse(HIGH);
    //形成数据脉冲，前一部分储存低电平持续时间，后一部分储存高电平持续时间
    //其中这里的时间间隔以50us为单位，通过计算电平在while循环内循环次数来估计
    //出数据脉冲的占空比
  }
  for (int i = 0; i < 40; ++i)
  {
    uint32_t lowCycles = cycles[2 * i];
    uint32_t highCycles = cycles[2 * i + 1];
    if ((lowCycles ==  UINT32_MAX) || (highCycles ==  UINT32_MAX))
    {
      Serial.println(F("DHT timeout waiting for pulse."));
      dht_lastresult = false;
      return dht_lastresult;
    }
    dht_data[i / 8] <<= 1;
    if (highCycles > lowCycles)
    {
      dht_data[i / 8] |= 1;
    }
    //将数据脉冲储存在事先定义好的数组中
    //起初数据均为0000 0000，输入第一个数据，如果为1，将0000 0000变为0000 0001，
    //并通过左移一位的位操作将0000 0001变为0000 0010，从而使得数据以串行方式输入
  }
  if (dht_data[4] == ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xFF))
  {
    dht_lastresult = true;
    return dht_lastresult;
  }
  else
  {
    Serial.println(F("DHT checksum failure!"));
    dht_lastresult = false;
    return dht_lastresult;
  }
  //存入dht_data中，前两个数据为湿度，后两个数据为温度，最后一位是通过检测前四个数的
  //和来确定输入数据的正确性
}

/**************************************************************
    函数名称：DHT_expectPulse
    函数功能：通过计算电平持续的while循环数获取粗略的
             数据脉冲
    调用函数：无
    输入参数：level(脉冲电平状态)
    输出参数：uint32_t
    返回值：count
***************************************************************/
uint32_t DHT_expectPulse(bool level)
{
#if (F_CPU > 16000000L)
  uint32_t count = 0;
#else
  uint16_t count = 0;
#endif
  while (digitalRead(DHT11_PIN) == level)
  {
    if (count++ >= dht_maxcycles)
    {
      return  UINT32_MAX;
    }
  }
  //通过计算循环次数来计算电平状态的持续时间
  return count;
}

/*******************************************************************
    函数名称：DHT_begin
    函数功能：初始化相关变量，发送开始信号，开启读数间隔检测
             初始化上拉时间
    调用函数：microsecondsToClockCycles
    输入参数：无
    输出参数：无
    返回值：无
********************************************************************/
void DHT_begin()
{
  dht_maxcycles = microsecondsToClockCycles(1000);
  pinMode(DHT11_PIN, INPUT_PULLUP);
  dht_lastreadtime = millis() - 2000;//用于确保dht两次完整读数间隔超过2s的参数
  dht_pullTime = 55;//保证DHT11充分上拉的时间间隔
  while (!DHT_read())
  {
    Serial.println("DHT11 is not work.");
    delay(1000);
  }
  Serial.println("DHT11 is OK.");
}

/****************************************************
    函数名称：DHT_readTemperature
    函数功能：将传感器传入的数据转变为直观的温度数值
    调用函数：DHT_read
    输入参数：无
    输出参数：f
    返回值：摄氏度或者华氏度
*****************************************************/
float DHT_readTemperature()
{
  float f = NAN;
  if (DHT_read())
  {
    f = dht_data[2];
    if (dht_data[3] & 0x80)
    {
      f = -1 - f;
    }
    f += (dht_data[3] & 0x0f) * 0.1;
  }
  return f;
}

/*********************************************************
    函数名称：DHT_readHumidity
    函数功能：将传感器传入的数据转变为直观的湿度百分比
    调用函数：DHT_read
    输入参数：无
    输出参数：f
    返回值：湿度百分比
**************************************************************/
float DHT_readHumidity()
{
  float f = NAN;
  if (DHT_read())
  {
    f = dht_data[0] + dht_data[1] * 0.1;
  }
  return f;
}

/***********************************************************
    函数名称：HX711_read_raw
    函数功能：读取HX711发送的128位增益后的数据
    调用函数：无
    输入参数：无
    输出参数：无
    返回值：HX711_data_128_raw
    说明：return的是原始数据，在主程序中一般不使用！
***********************************************************/
double HX711_read_raw()
{
  unsigned long HX711_data_128_raw = 0;
  double HX711_data_128 = 0;
  digitalWrite(DOUT_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(SCK_PIN, LOW);
  delayMicroseconds(1);
  while (digitalRead(DOUT_PIN) == 1)
  {
  }//当DOUT从高电平变低电平后，表明此时HX711已经准备好发送数据，故当其处于高电平是进入空循环
  for (int i = 0; i < 24; i++)//通过查询HX711 datasheet可知，每次发送的数据为24位，当为25次脉冲时为增益128倍，26次时增益32倍，27时为64
  {
    digitalWrite(SCK_PIN, HIGH);
    delayMicroseconds(1);
    HX711_data_128_raw = HX711_data_128_raw << 1;
    digitalWrite(SCK_PIN, LOW);
    delayMicroseconds(1);
    if (digitalRead(DOUT_PIN) == 1)
    {
      HX711_data_128_raw++;
    }
  }
  digitalWrite(SCK_PIN, HIGH);
  HX711_data_128_raw ^= 0x800000;//为什么要取异或？参考:https://blog.csdn.net/yanjinxu/article/details/47861323
  delayMicroseconds(1);
  digitalWrite(SCK_PIN, LOW);
  delayMicroseconds(1);
  HX711_data_128 = HX711_data_128_raw / scale_factor;
  return HX711_data_128_raw;
}

/***********************************************************
    函数名称：HX711_read
    函数功能：读取HX711发送的128位增益后的,再经过处理后的数据，单位为g，精度能小于0.2g
    调用函数：HX711_read_raw()
    输入参数：无
    输出参数：无
    返回值：HX711_data_128
    说明：
***********************************************************/
double HX711_read()
{
  double HX711_data_128 = 0;
  HX711_data_128 = (double)(HX711_read_raw() - HX711_initial_weight) / scale_factor;
  return HX711_data_128;
}

/***********************************************************
    函数名称：HX711_read_raw_average
    函数功能：读取HX711发送的128位增益后的多次的平均值数据
    调用函数：HX711_read();
    输入参数：读取readtimes次，并取readtimes次的平均值
    输出参数：无
    返回值：HX711_raw_average
    说明：return的是原始数据，在主程序中一般不使用！
***********************************************************/
double HX711_read_raw_average(int readtimes)
{
  double HX711_raw_average = 0;
  double sum = 0;
  for (int i = 0; i < readtimes ; i++)
  {
    sum += HX711_read_raw();
  }
  HX711_raw_average = sum / readtimes;
  readtimes = 0;
  return HX711_raw_average;
}

/***********************************************************
    函数名称：HX711_read_average
    函数功能：读取HX711发送的128位增益后的多次的平均值数据,再经过处理后的数据，单位为g，精度能小于0.2g
    调用函数：HX711_read();
    输入参数：读取readtimes次，并取readtimes次的平均值
    输出参数：无
    返回值：HX711_average
    说明：无
***********************************************************/
double HX711_read_average(int readtimes)
{
  double HX711_average = 0;
  double sum = 0;
  for (int i = 0; i < readtimes ; i++)
  {
    sum += HX711_read();
  }
  HX711_average = sum / readtimes;
  return HX711_average;
}

/***********************************************************
    函数名称：HX711_initialize
    函数功能：初始化HX711，记录最开始的时候的原始读数,并串口输出部分变量便于开发者调试
    调用函数：HX711_read_raw_average
    输入参数：
    输出参数：无
    返回值：无
    说明：无
***********************************************************/
void HX711_initialize()
{
  while (1)
  {
    if (digitalRead(DOUT_PIN) != 1)
    {
      Serial.println("HX711 is OK.");
      if (abs(HX711_read_raw() - HX711_read_raw_average(10)) < 1200) //防抖，大概允许抖动范围为+-3g
      {
        HX711_initial_weight = HX711_read_raw();
        break;
      }
    }
    else
    {
      Serial.println("HX711 is not work.");
      delay(1000);
    }
  }
  Serial.print("HX711_initial_wight:");
  Serial.println(HX711_initial_weight);
  Serial.print("HX711_read_raw:");
  Serial.println(HX711_read_raw());
  Serial.print("HX711_read:");
  Serial.println(HX711_read());
}

/***********************************************************
    函数名称：remind1()
    函数功能：暂定第一种提醒方式,先蜂鸣器、LED灯、LCD显示内容”已<变量>min（或者<变量>h<变量>min）未饮水，马上饮水！！！“常亮3s.
    然后蜂鸣器、LED灯、LCD屏幕显示内容开始闪烁，闪烁间隔0.5s,总计闪烁时间20s，函数结束
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602，，现有bug，执行一次后原设定的常亮3s不再出现，原因不详，估计与millis函数有关,     20200902 吴**  已修复
         20200902发现新bug，当no_drink_time为一位数时，感叹号显示有点问题，小bug,赶工暂未修复
         20201001 吴**已修复
         20201112 吴** 加入定时器中断查询语句
***********************************************************/
void remind1()
{
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  pinMode(BUZZER1, OUTPUT);
  digitalWrite(BUZZER1, HIGH);
  digitalWrite(BUZZER2, HIGH);
  pinMode(BUZZER2, OUTPUT);
  pinMode(LED1, OUTPUT);
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
  if (user_mute == 0)
  {
    digitalWrite(BUZZER2, LOW);
  }
  digitalWrite(LED1, HIGH);
  MsTimer2::set(500, detection); //设置中断，每500ms进入一次中断服务程序
  MsTimer2::start(); //开始计时_开启定时器中断
  old_time = millis();
  while (millis() < old_time + 3000)
  {
    if (drinking_leaving == 1)
    {
      goto remind_over;
    }
  }
  old_time = millis(); //延时3s
  // if (millis() > old_time + 3000) {
  //   old_time = millis();
  digitalWrite(LED1, LOW);
  digitalWrite(BUZZER2, HIGH);
  while (millis() < old_time + 20000)
  {
    if (user_mute == 0)
    {
      digitalWrite(BUZZER2, LOW);
    }
    digitalWrite(LED1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
    digitalWrite(BUZZER2, HIGH);
    digitalWrite(LED1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
  }
remind_over:
  old_time = millis();
  digitalWrite(BUZZER2, HIGH);
  MsTimer2::stop();
}

/***********************************************************
    函数名称：remind1_s()
    函数功能：remind1的 秒数 显示版，
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602，，现有bug，执行一次后原设定的常亮3s不再出现，原因不详，估计与millis函数有关,     20200902 吴** 已修复
         20200902发现新bug，当no_drink_time为一位数时，感叹号显示有点问题，小bug,赶工暂未修复
         20201001 吴**已修复
         20201112 吴** 加入定时器中断查询语句
         仅用于开发者测试！！！
***********************************************************/
void remind1_s()
{
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  pinMode(BUZZER1, OUTPUT);
  digitalWrite(BUZZER1, HIGH);
  digitalWrite(BUZZER2, HIGH);
  pinMode(BUZZER2, OUTPUT);
  pinMode(LED1, OUTPUT);

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
    lcd.print("min");
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
  lcd.print("!");
  if (((unsigned long)millis() / 1000 - begin_time) > 60 && ((unsigned long)millis() / 1000 - begin_time) <= 600)
  {
    lcd.setCursor(0, 1);
  } //手动换行
  lcd.print("!");
  lcd.print("!");
  if (user_mute == 0)
  {
    digitalWrite(BUZZER2, LOW);
  }
  digitalWrite(LED1, HIGH);

  MsTimer2::set(500, detection); //设置中断，每500ms进入一次中断服务程序
  MsTimer2::start(); //开始计时_开启定时器中断

  old_time = millis();
  while (millis() < old_time + 3000)
  {
    if (drinking_leaving == 1)
    {
      goto remind_over;
    }
  }
  old_time = millis(); //延时3s
  digitalWrite(LED1, LOW);
  digitalWrite(BUZZER2, HIGH);
  while (millis() < old_time + 20000)
  {
    if (user_mute == 0)
    {
      digitalWrite(BUZZER2, LOW);
    }
    digitalWrite(LED1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
    digitalWrite(BUZZER2, HIGH);
    digitalWrite(LED1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
  }
remind_over:
  old_time = millis();
  digitalWrite(BUZZER2, HIGH);
  MsTimer2::stop();
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
    函数名称：LCD1602_usual_scale()
    函数功能：称重显示
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：测试用，需要初始化LCD1602
***********************************************************/
void LCD1602_usual_scale()
{
  if (user_backlight == 1)
  {
    lcd.backlight();
  }
  lcd.init();
  lcd.print("weight:");
  lcd.setCursor(0, 1);
  lcd.print(HX711_read());
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
    说明：20200831 吴**编写，
    此函数工作不正常，请勿使用
***********************************************************/
void scale_antishake()
{
  for (int i = 0; i < 5; i++)
  {
    scale_antishake_weight[i] = HX711_read();
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
  if (scale_antishake_gap < 1 && HX711_read() > 10)
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
  if (!CONNECT)                 //如果连接失败，则通过AT指令重新连接
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
  while (Serial.available())
  {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n')
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
void processMessage(aJsonObject * msg)
{
  aJsonObject*method = aJson.getObjectItem(msg, "M");
  aJsonObject*content = aJson.getObjectItem(msg, "C");
  aJsonObject*client_id = aJson.getObjectItem(msg, "ID");
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

/***********************************************************
    函数名称：remind1_mute()
    函数功能：remind1静音提醒版，比remind少一个蜂鸣器，其它一致。
    调用函数：
    输入参数：
    输出参数：
    返回值：无
    说明：需要初始化LCD1602，现已弃用，全部打成了注释
***********************************************************/
/*
  void remind1_mute()
  {
  unsigned long old_time = 0;
  unsigned long old_time1 = 0;
  pinMode(LED1, OUTPUT);
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

  digitalWrite(LED1, HIGH);
  old_time = millis();
  while (millis() < old_time + 3000)
  {
    if (drinking_leaving == 1)
    {
      goto remind_over;
    }
  }
  old_time = millis(); //延时3s
  digitalWrite(LED1, LOW);
  while (millis() < old_time + 20000)
  {
    digitalWrite(LED1, HIGH);
    if (user_blink == 1)
    {
      lcd.display();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
    digitalWrite(LED1, LOW);
    if (user_blink == 1)
    {
      lcd.noDisplay();
    }
    while (millis() < old_time1 + 500)
    {
      if (drinking_leaving == 1)
      {
        goto remind_over;
      }
    }
    old_time1 = millis();
  }
  remind_over:
  old_time = millis();
  }
*/
