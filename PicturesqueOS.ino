#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <time.h>


// ===============================
// WiFi
// ===============================

const char* ssid = "wifi";
const char* password = "password";

const char* ntpServer = "pool.ntp.org";



// ===============================
// Waveshare Pico-ePaper 4.2 V2
// ===============================

#define EPD_CLK  10
#define EPD_DIN  11
#define EPD_CS   9
#define EPD_DC   8
#define EPD_RST  12
#define EPD_BUSY 13

// ===============================
// Refresh control
// ===============================

bool firstUpdate = true;

unsigned long lastFullRefresh = 0;

const unsigned long FULL_REFRESH_INTERVAL =
  6UL * 60UL * 60UL * 1000UL;



// ===============================
// Canvas
// ===============================

GFXcanvas1 canvas(400,300);



// ===============================
// SPI
// ===============================

void Epd_Spi_Write(uint8_t data)
{
  for(int i=0;i<8;i++)
  {
    digitalWrite(
      EPD_DIN,
      (data & 0x80) ? HIGH : LOW
    );

    digitalWrite(EPD_CLK,HIGH);
    delayMicroseconds(1);

    digitalWrite(EPD_CLK,LOW);

    data <<= 1;
  }
}



void SendCommand(uint8_t command)
{
  digitalWrite(EPD_DC,LOW);
  digitalWrite(EPD_CS,LOW);

  Epd_Spi_Write(command);

  digitalWrite(EPD_CS,HIGH);
}



void SendData(uint8_t data)
{
  digitalWrite(EPD_DC,HIGH);
  digitalWrite(EPD_CS,LOW);

  Epd_Spi_Write(data);

  digitalWrite(EPD_CS,HIGH);
}



void WaitUntilIdle()
{
  while(digitalRead(EPD_BUSY)==HIGH)
  {
    delay(10);
  }
}



// ===============================
// Display Init
// ===============================

void Display_Init()
{
  // Reset
  digitalWrite(EPD_RST, HIGH);
  delay(100);

  digitalWrite(EPD_RST, LOW);
  delay(2);

  digitalWrite(EPD_RST, HIGH);
  delay(100);


  WaitUntilIdle();


  // Soft reset
  SendCommand(0x12);

  WaitUntilIdle();



  // Display update control
  SendCommand(0x21);
  SendData(0x40);
  SendData(0x00);



  // Border waveform
  SendCommand(0x3C);
  SendData(0x05);



  // Data entry mode
  SendCommand(0x11);
  SendData(0x03);



  // Window 400x300
  SendCommand(0x44);
  SendData(0x00);
  SendData(0x31);



  // Window Y
  SendCommand(0x45);
  SendData(0x00);
  SendData(0x00);
  SendData(0x2B);
  SendData(0x01);



  // Cursor X
  SendCommand(0x4E);
  SendData(0x00);



  // Cursor Y
  SendCommand(0x4F);
  SendData(0x00);
  SendData(0x00);


  WaitUntilIdle();
}

// ===============================
// Full Refresh
// ===============================

void Display_FullRefresh()
{

  Display_Init();


  uint8_t* buffer = canvas.getBuffer();

  int bufferSize = 400 * 300 / 8;



  // New RAM
SendCommand(0x24);

for(int i=0;i<bufferSize;i++)
{
  SendData(buffer[i]);
}



  // Old RAM
  SendCommand(0x26);

  for(int i=0;i<bufferSize;i++)
  {
    SendData(buffer[i]);
  }



  // Refresh

  SendCommand(0x22);
  SendData(0xF7);

  SendCommand(0x20);


  WaitUntilIdle();

}



// ===============================
// Partial Refresh
// ===============================

void Display_PartialRefresh()
{
  uint8_t* buffer = canvas.getBuffer();



  SendCommand(0x3C);
  SendData(0x80);


  // Display update control
  SendCommand(0x21);
  SendData(0x00);
  SendData(0x00);


 
  SendCommand(0x3C);
  SendData(0x80);



  // whole screen (400x300)
  SendCommand(0x44);
  SendData(0x00);
  SendData(0x31);


  SendCommand(0x45);
  SendData(0x00);
  SendData(0x00);
  SendData(0x2B);
  SendData(0x01);



  // Start position
  SendCommand(0x4E);
  SendData(0x00);


  SendCommand(0x4F);
  SendData(0x00);
  SendData(0x00);




  SendCommand(0x24);


  int bufferSize = 400 * 300 / 8;


for(int i = 0; i < bufferSize; i++)
{
  SendData(buffer[i]);
}



  SendCommand(0x22);
  SendData(0xFF);


  SendCommand(0x20);


  WaitUntilIdle();
}
// ===============================
// Smart Refresh
// ===============================

void Display_Update()
{
  unsigned long now = millis();


  if(firstUpdate)
  {
    Display_FullRefresh();

    firstUpdate=false;
    lastFullRefresh=now;

    return;
  }


  if(now-lastFullRefresh > FULL_REFRESH_INTERVAL)
  {
    Display_FullRefresh();

    lastFullRefresh=now;
  }
  else
  {
    Display_PartialRefresh();
  }
}

// ===============================
// Draw Clock
// ===============================

void drawClock()
{

  canvas.fillScreen(1);


  canvas.drawRect(
    5,
    5,
    390,
    290,
    1
  );


  canvas.setTextColor(0);



  int16_t x1,y1;
  uint16_t w,h;





  canvas.setTextSize(2);


  String title="PicturesqueOS";


  canvas.getTextBounds(
    title,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h
  );


  canvas.setCursor(
    (400-w)/2,
    25
  );


  canvas.print(title);




  // time

  struct tm timeinfo;

  time_t now=time(nullptr);


  localtime_r(
    &now,
    &timeinfo
  );



  if(now < 100000)
  {

    String error="NO TIME";


    canvas.getTextBounds(
      error,
      0,
      0,
      &x1,
      &y1,
      &w,
      &h
    );


    canvas.setCursor(
      (400-w)/2,
      140
    );


    canvas.print(error);

    return;

  }




  char timeString[10];


  strftime(
    timeString,
    sizeof(timeString),
    "%H:%M",
    &timeinfo
  );



  canvas.setTextSize(8);



  canvas.getTextBounds(
    timeString,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h
  );


  canvas.setCursor(
    (400-w)/2,
    105
  );


  canvas.print(timeString);





  // date

  char dateString[40];


  strftime(
    dateString,
    sizeof(dateString),
    "%d/%m/%Y",
    &timeinfo
  );



  canvas.setTextSize(2);



  canvas.getTextBounds(
    dateString,
    0,
    0,
    &x1,
    &y1,
    &w,
    &h
  );



  canvas.setCursor(
    (400-w)/2,
    210
  );


  canvas.print(dateString);

}



// ===============================
// WiFi
// ===============================

void connectWiFi()
{

  Serial.print("Connecting WiFi");


  WiFi.begin(
    ssid,
    password
  );


  while(WiFi.status()!=WL_CONNECTED)
  {

    delay(500);

    Serial.print(".");

  }


  Serial.println();

  Serial.println("WiFi connected");

}



// ===============================
// Setup
// ===============================

void setup()
{

  delay(1000);


  Serial.begin(115200);



  pinMode(EPD_CLK,OUTPUT);
  pinMode(EPD_DIN,OUTPUT);
  pinMode(EPD_CS,OUTPUT);
  pinMode(EPD_DC,OUTPUT);
  pinMode(EPD_RST,OUTPUT);
  pinMode(EPD_BUSY,INPUT);



  digitalWrite(EPD_CS,HIGH);
  digitalWrite(EPD_CLK,LOW);

  connectWiFi();



  configTime(
    0,
    0,
    ntpServer,
    "time.nist.gov"
  );

  Display_Init();

  setenv(
    "TZ",
    "CET-1CEST,M3.5.0,M10.5.0",
    1
  );


  tzset();



  delay(3000);



  drawClock();


  Display_Update();



  Serial.println(
    "PicturesqueOS running"
  );

}



// ===============================
// Loop
// ===============================

void loop()
{

  static int lastMinute=-1;



  time_t now=time(nullptr);


  struct tm timeinfo;


  localtime_r(
    &now,
    &timeinfo
  );



  if(timeinfo.tm_min != lastMinute)
  {

    lastMinute=timeinfo.tm_min;



    drawClock();


    Display_Update();



    Serial.println(
      "Clock updated"
    );

  }



  delay(1000);

}
