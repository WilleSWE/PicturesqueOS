#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <time.h>


// ===============================
// WiFi
// ===============================

const char* ssid = " anndd wifi name here";
const char* password = " your wifi password here, obviously :) ";

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


  // Border waveform för partial
  SendCommand(0x3C);
  SendData(0x80);


  // Display update control
  SendCommand(0x21);
  SendData(0x00);
  SendData(0x00);


  // Border waveform igen (Waveshare gör detta)
  SendCommand(0x3C);
  SendData(0x80);



  // Hela skärmen (400x300)
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



  // Skriv bilddata
  SendCommand(0x24);


  int bufferSize = 400 * 300 / 8;


for(int i = 0; i < bufferSize; i++)
{
  SendData(buffer[i]);
}



  // Starta partial refresh
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
// Buttons / Pages
// ===============================

#define BUTTON_PAGE     17
#define BUTTON_REFRESH  15

int currentPage = 0;
// 0 = Clock
// 1 = Info

bool lastPageButtonState = HIGH;
bool lastRefreshButtonState = HIGH;

unsigned long lastPageButtonTime = 0;
unsigned long lastRefreshButtonTime = 0;

const unsigned long BUTTON_DEBOUNCE = 200;

// ===============================
// Days Until Date
// ===============================

int daysUntil(int month, int day)
{
  time_t now = time(nullptr);

  struct tm current;
  localtime_r(&now, &current);

  // Today's date at midnight
  current.tm_hour = 0;
  current.tm_min = 0;
  current.tm_sec = 0;

  time_t today = mktime(&current);

  // Target date
  struct tm target = current;

  target.tm_mon = month - 1;
  target.tm_mday = day;

  time_t targetTime = mktime(&target);

  // If this year's date has already passed,
  // use next year.
  if(targetTime < today)
  {
    target.tm_year++;
    targetTime = mktime(&target);
  }

  return (targetTime - today) / 86400;
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

  int16_t x1, y1;
  uint16_t w, h;

  canvas.setTextSize(2);

  String title = "PicturesqueOS";

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
    (400 - w) / 2,
    25
  );

  canvas.print(title);


  // ===============================
  // Time
  // ===============================

  struct tm timeinfo;

  time_t now = time(nullptr);

  localtime_r(
    &now,
    &timeinfo
  );


  if(now < 100000)
  {
    String error = "NO TIME";

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
      (400 - w) / 2,
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
    (400 - w) / 2,
    105
  );

  canvas.print(timeString);


  // ===============================
  // Date
  // ===============================

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
    (400 - w) / 2,
    210
  );

  canvas.print(dateString);


  // Page indicator

  canvas.setTextSize(1);

  canvas.setCursor(
    350,
    275
  );

  canvas.print("PAGE");
}


// ===============================
// Draw Info Page
// ===============================

void drawInfo()
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

  canvas.setTextSize(2);

  // ===============================
  // Title
  // ===============================

  canvas.setCursor(
    20,
    18
  );

  canvas.print("PicturesqueOS INFO");


  // ===============================
  // WiFi
  // ===============================

  canvas.setCursor(
    20,
    55
  );

  canvas.print("WiFi:");

  canvas.setCursor(
    100,
    55
  );

  canvas.print(WiFi.SSID());


  // ===============================
  // IP
  // ===============================

  canvas.setCursor(
    20,
    80
  );

  canvas.print("IP:");

  canvas.setCursor(
    100,
    80
  );

  canvas.print(WiFi.localIP());


  // ===============================
  // Uptime
  // ===============================

  unsigned long uptimeSeconds = millis() / 1000;

  unsigned long days =
    uptimeSeconds / 86400;

  unsigned long hours =
    (uptimeSeconds % 86400) / 3600;

  unsigned long minutes =
    (uptimeSeconds % 3600) / 60;

  unsigned long seconds =
    uptimeSeconds % 60;


  canvas.setCursor(
    20,
    110
  );

  canvas.print("Uptime:");

  canvas.setCursor(
    100,
    110
  );

  canvas.print(days);
  canvas.print("d ");

  canvas.print(hours);
  canvas.print("h ");

  canvas.print(minutes);
  canvas.print("m ");


  // ===============================
  // Events
  // ===============================

  canvas.setCursor(
    20,
    150
  );

  canvas.print("Days until:");


  // 29 September

  canvas.setCursor(
    30,
    175
  );

  canvas.print("29 September:");

  canvas.setCursor(
    190,
    175
  );

  canvas.print(daysUntil(9, 29));


  // Halloween

  canvas.setCursor(
    30,
    200
  );

  canvas.print("Halloween:");

  canvas.setCursor(
    190,
    200
  );

  canvas.print(daysUntil(10, 31));


  // Christmas Eve

  canvas.setCursor(
    30,
    225
  );

  canvas.print("Christmas Eve:");

  canvas.setCursor(
    190,
    225
  );

  canvas.print(daysUntil(12, 24));


  // Christmas

  canvas.setCursor(
    30,
    250
  );

  canvas.print("Christmas:");

  canvas.setCursor(
    190,
    250
  );

  canvas.print(daysUntil(12, 25));


  // ===============================
  // Page indicator
  // ===============================

  canvas.setTextSize(1);

  canvas.setCursor(
    350,
    275
  );

  canvas.print("INFO");
}

// ===============================
// Draw Current Page
// ===============================

void drawCurrentPage()
{
  if(currentPage == 0)
  {
    drawClock();
  }
  else
  {
    drawInfo();
  }
}

// ===============================
// Buttons
// ===============================

void handleButtons()
{
  bool pageButtonState =
    digitalRead(BUTTON_PAGE);

  bool refreshButtonState =
    digitalRead(BUTTON_REFRESH);


  // ===============================
  // PAGE button
  // ===============================

  if(
    pageButtonState == LOW &&
    lastPageButtonState == HIGH &&
    millis() - lastPageButtonTime > BUTTON_DEBOUNCE
  )
  {
    lastPageButtonTime = millis();

    currentPage++;

    if(currentPage > 1)
    {
      currentPage = 0;
    }

    drawCurrentPage();

    Display_Update();

    Serial.print("Page changed to: ");
    Serial.println(currentPage);
  }


  // ===============================
  // REFRESH button
  // ===============================

  if(
    refreshButtonState == LOW &&
    lastRefreshButtonState == HIGH &&
    millis() - lastRefreshButtonTime > BUTTON_DEBOUNCE
  )
  {
    lastRefreshButtonTime = millis();

    drawCurrentPage();

    Display_Update();

    Serial.println("Manual refresh");
  }


  lastPageButtonState =
    pageButtonState;

  lastRefreshButtonState =
    refreshButtonState;
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

  pinMode(BUTTON_PAGE, INPUT_PULLUP);
  pinMode(BUTTON_REFRESH, INPUT_PULLUP);


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
  // Handle buttons
  handleButtons();


  // ===============================
  // Clock updates
  // ===============================

  static int lastMinute = -1;


  time_t now = time(nullptr);

  struct tm timeinfo;

  localtime_r(
    &now,
    &timeinfo
  );


  // Only automatically update the
  // clock page once every minute.

  if(
    currentPage == 0 &&
    timeinfo.tm_min != lastMinute
  )
  {
    lastMinute = timeinfo.tm_min;

    drawClock();

    Display_Update();

    Serial.println(
      "Clock updated"
    );
  }


  delay(50);
}
