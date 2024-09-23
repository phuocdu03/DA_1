#include <Keypad_I2C.h>          // Sử dụng thư viện Keypad_I2C cho keypad qua giao thức I2C
#include <Keypad.h>              // Sử dụng thư viện Keypad cho keypad
#include <Wire.h>                 // Sử dụng thư viện Wire cho giao thức I2C

#include <RTClib.h>               // Sử dụng thư viện RTClib cho đồng hồ thời gian thực

#include <SPI.h>                  // Sử dụng thư viện SPI cho giao tiếp SPI
#include <MFRC522.h>              // Sử dụng thư viện MFRC522 cho RFID

#include <SD.h>                   // Sử dụng thư viện SD cho thẻ SD

#include <LiquidCrystal_I2C.h>    // Sử dụng thư viện LiquidCrystal_I2C cho LCD qua giao thức I2C

#include <freertos/FreeRTOS.h>    // Sử dụng thư viện FreeRTOS cho hệ điều hành thời gian thực
#include <freertos/task.h>        // Sử dụng thư viện FreeRTOS cho tạo các task

TimerHandle_t timer1;             // Biến TimerHandle_t để tạo timer

SemaphoreHandle_t Semaphore_SDcard;  // Biến SemaphoreHandle_t để tạo semaphore cho SD card

// RTC
RTC_DS1307 rtc;

// Mảng chứa tên các ngày trong tuần
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
int year, month, day, hour, minute, second;
String dayofweek;
int userCheckInMonth, userCheckInDay, userCheckInHour, userCheckInMinute, userCheckInSecond;
String userCheckInDayofweek;

// Định nghĩa chân cho RFID
#define SS_RFID 5
#define RST_RFID 2

// RFID
MFRC522 rfid(SS_RFID, RST_RFID);

// Tạo một tệp để lưu trữ dữ liệu
File myFile;

// Định nghĩa chân CS cho module SDCARD
#define CS_SD 4

#define I2CADDR 0x21   // Gán địa chỉ I2C

const byte ROWS = 4;  // Giới hạn số hàng
const byte COLS = 4;  // Giới hạn số cột

// Gán các phím hoạt động (4x4)
char keys[ROWS][COLS] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

// Xác định các chân hoạt động (4x4)
byte rowPins[ROWS] = {0, 1, 2, 3}; // Kết nối với chân hàng của nút nhấn
byte colPins[COLS] = {4, 5, 6, 7}; // Kết nối với chân cột của nút nhấn

// rowPins: Đặt các chân hàng của nút nhấn
// colPins: Đặt các chân cột của bàn phím
// ROWS: Đặt số hàng
// COLS: Đặt số cột
// I2CADDR: Đặt địa chỉ I2C
// PCF8574: Đặt số IC
Keypad_I2C keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574 );

char pressedKey = ' ';  // Biến lưu giá trị nút được nhấn


// Thiết lập số cột và hàng cho màn hình LCD
int lcdColumns = 16;
int lcdRows = 2;

// Thiết lập địa chỉ, số cột và số hàng cho màn hình LCD
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

// Xác định thời gian checkIn
const int checkInHour = 7;
const int checkInMinute = 0;
const int checkInSecond = 0;

// Chân cho đèn LED và buzze
const int redLED = 13;
const int greenLED = 12;
const int buzzer = 25;

// Cấu trúc lưu trữ thông tin sinh viên
struct sinhvien{
  String ten;  // Tên sinh viên
  String mssv; // Mã số sinh viên
};

sinhvien sv[100]; // Mảng để lưu trữ thông tin của tối đa 100 sinh viên
int n = 0;        // Biến đếm số lượng sinh viên
int flag_tenmssv = 0; // Cờ để xác định tên hay MSSV

String uidString;   // Biến để lưu trữ UID của thẻ
String tenString;   // Lưu trữ tên sinh viên tạm thời
String mssvString;  // Lưu trữ MSSV sinh viên tạm thời

String trangthai; // Trạng thái điểm danh

String tennow;    // Tên sinh viên hiện tại
String mssvnow;   // MSSV hiện tại

int flag_savenewsinhvien = 0; // Cờ để thông báo lưu thông tin sinh viên mới
int flag_deletesinhvien = 0;  // Cờ để thông báo xóa thông tin sinh viên
int flag_SDcard;   // Cờ để quản lý thẻ SD

int flag_LCD = 0;  // Cờ để điều khiển LCD
String tenlcd;     // Lưu trữ tên sinh viên tạm thời để hiển thị trên LCD
String mssvlcd;    // Lưu trữ MSSV tạm thời để hiển thị trên LCD



// Hàm kiểm tra CheckIn
void verifyCheckIn(){
  if ((userCheckInHour < checkInHour) ||
      ((userCheckInHour == checkInHour) && (userCheckInMinute <= checkInMinute)) ||
      ((userCheckInHour == checkInHour) && (userCheckInMinute == checkInMinute) && (userCheckInSecond <= checkInSecond))) {
    trangthai = "SOON";  // Nếu thời gian điểm danh sớm
    digitalWrite(greenLED, HIGH); // Bật đèn LED xanh
    digitalWrite(redLED, LOW);    // Tắt đèn LED đỏ
    Serial.println("SOON");       // In ra Serial Monitor "SOON"
  }
  else{
    trangthai = "LATE";   // Nếu thời gian điểm danh đã qua
    digitalWrite(redLED, HIGH);   // Bật đèn LED đỏ
    digitalWrite(greenLED, LOW);  // Tắt đèn LED xanh
    Serial.println("LATE");        // In ra Serial Monitor "LATE"
  }
}


// Hàm ghi lại thời gian khi sv điểm danh
void userTimeCheckIn(){
  userCheckInMonth = month;           
  userCheckInDay = day;                
  userCheckInDayofweek = dayofweek;   
  userCheckInHour = hour;              
  userCheckInMinute = minute;          
  userCheckInSecond = second;          
}



// Hàm lưu thông tin sinh viên mới vào thẻ SD
void SaveNewSinhvien() {
  // Kích hoạt chân CS của thẻ SD
  digitalWrite(CS_SD, LOW);

  myFile = SD.open("/sinhvien.csv", FILE_APPEND);
  // Nếu tệp mở thành công, ghi vào tệp
  if (myFile) {
    Serial.println("File opened OK");
    myFile.print(uidString);
    myFile.print(", ");
    myFile.print(mssvnow);
    myFile.print(", ");
    myFile.println(tennow);

    Serial.println("Successfully written on SD card");
    myFile.close();
    flag_savenewsinhvien = 1;
  }
  else {
    flag_savenewsinhvien = 0;
    Serial.println("Error opening sinhvien.csv");
  }
  // Tắt chân CS của thẻ SD  
  digitalWrite(CS_SD, HIGH);
}



// Hàm xóa thông tin sinh viên khỏi thẻ SD
void DeleteSinhvien() {
  // Kích hoạt chân CS của thẻ SD
  digitalWrite(CS_SD, LOW);

  // Mở tệp sinhvien.csv để đọc
  myFile = SD.open("/sinhvien.csv");
  if (myFile) {
    Serial.println("File opened OK");

    // Chuỗi để lưu trữ nội dung tệp
    String fileContent = "";

   // Đọc từng dòng trong tệp và loại bỏ dòng chứa MSSV cần xóa
while (myFile.available()) {
  String line = myFile.readStringUntil('\n');  // Đọc một dòng từ tệp
  if (line.indexOf(mssvnow) == -1) {
    // Nếu không chứa MSSV cần xóa, thêm dòng đó vào chuỗi fileContent
    fileContent += line + "\n";
  }
}
    myFile.close();

    // Mở tệp sinhvien.csv để ghi
    myFile = SD.open("/sinhvien.csv", FILE_WRITE);
    if (myFile) {
      // Ghi lại nội dung tệp sau khi loại bỏ thông tin sinh viên cần xóa
      myFile.print(fileContent);
      myFile.close();
      Serial.println("Successfully deleted rows containing " + String(mssvnow));
      flag_deletesinhvien = 1;
    } else {
      Serial.println("Error opening sinhvien.csv");
    }
  } else {
    Serial.println("Error opening sinhvien.csv");
    flag_deletesinhvien = 0;
  }
  // Tắt chân CS của thẻ SD  
  digitalWrite(CS_SD, HIGH);
}


// Hàm kiểm tra thông tin sinh viên từ thẻ SD và ghi lại thời gian điểm danh
void CheckSinhvien() {
  // Kích hoạt chân CS của thẻ SD
  digitalWrite(CS_SD, LOW);

  // Mở tệp sinhvien.csv để đọc
  myFile = SD.open("/sinhvien.csv", FILE_READ);
  // Nếu tệp mở thành công, đọc từ tệp
  if (myFile) {
    Serial.println("File opened OK");
    tenString = "";
    mssvString = "";

    // Đọc từng dòng trong tệp
    while (myFile.available()) {
      String line = myFile.readStringUntil('\n');
      // Kiểm tra xem UID có tồn tại trong dòng không
      if (line.indexOf(uidString) != -1) {
        tenString = line.substring(line.lastIndexOf(",") + 1); //  Trích xuất chuỗi từ vị trí sau dấu phẩy cuối cùng trong line đến hết dòng và lưu vào biến tenString
        tenString.trim(); // Loại bỏ khoảng trắng ở đầu và cuối chuỗi
        mssvString = line.substring(line.lastIndexOf(",", line.lastIndexOf(",") - 1) + 1, line.lastIndexOf(",")); // Trích xuất chuỗi từ vị trí sau dấu phẩy thứ hai từ cuối line đến dấu phẩy cuối cùng và lưu vào biến mssvString
        mssvString.trim(); // Loại bỏ khoảng trắng ở đầu và cuối chuỗi
        break;
      }
    }

    myFile.close();
  } else {
    Serial.println("Error opening sinhvien.csv");
  }

  // Mở tệp timecheckin.csv để ghi
  myFile = SD.open("/timecheckin.csv", FILE_APPEND);
  // Nếu tệp mở thành công, ghi vào tệp
  if (myFile) {
    Serial.println("File opened OK");
    myFile.print(uidString);
    myFile.print(", ");
    myFile.print(tenString);
    myFile.print(", ");
    myFile.print(mssvString);
    myFile.print(", ");      
    
    // Lưu thời gian điểm danh vào thẻ SD
    myFile.print(year);
    myFile.print('/');
    myFile.print(month);
    myFile.print('/');
    myFile.print(day);
    myFile.print(", ");
    myFile.print(dayofweek);
    myFile.print(',');
    myFile.print(hour);
    myFile.print(':');
    myFile.print(minute);
    myFile.print(':');
    myFile.print(second);
    myFile.print(", ");
    myFile.println(trangthai);
    
    Serial.println("Successfully written on SD card");
    myFile.close();
  } else {
    Serial.println("Error opening timecheckin.csv for writing");
  }

  // Tắt chân CS của thẻ SD  
  digitalWrite(CS_SD, HIGH);
}


// Hàm xử lý bàn phím I2C
void I2CKeypad(){
  char key = keypad.getKey();  // Đọc phím được nhấn từ keypad
  
  if (key) {
    Serial.println(key);  // In giá trị phím ra Serial Monitor
    pressedKey = key;  // Lưu giá trị phím vào biến pressedKey
  }

  // Xử lý khi nhấn các phím
  if (key == '*') {
    sv[n].ten = "";
    sv[n].mssv = "";
    flag_tenmssv = 1;  // Bật cờ để nhập MSSV
    
    flag_LCD=1;  // Bật cờ để hiển thị thông báo trên LCD
  } else if (key == '#') {
    flag_tenmssv = 0;  // Tắt cờ nhập tên
  } else if (key == 'D') {
    Serial.print("Ten: ");
    Serial.println(sv[n].ten);
    Serial.print("MSSV: ");
    Serial.println(sv[n].mssv);

    tennow=sv[n].ten;
    mssvnow=sv[n].mssv;

    // Kiểm tra nếu tên không được nhập
    if((mssvnow !="") && (tennow =="")){
      flag_SDcard = 2;
    } else {
      flag_SDcard = 1;
    }

    xSemaphoreGive(Semaphore_SDcard); // Tăng giá trị của semaphore từ 0 lên 1

    n++;  // Tăng số lượng sinh viên lưu trữ
  } else if (flag_tenmssv==1) {
    sv[n].mssv.concat(String(key));  // Nối ký tự phím vào MSSV
    mssvlcd=sv[n].mssv;  // Lưu MSSV vào biến để hiển thị trên LCD
  } else if (flag_tenmssv==0){
    sv[n].ten.concat(String(key));  // Nối ký tự phím vào tên
    tenlcd=sv[n].ten;  // Lưu tên vào biến để hiển thị trên LCD
  }
}


// Hàm đọc thời gian từ module RTC DS1307
void DS1307 () {
  DateTime now = rtc.now();  // Đọc thời gian hiện tại từ RTC
  year = now.year();  // Lấy thông tin năm
  month = now.month();  // Lấy thông tin tháng
  day = now.day();  // Lấy thông tin ngày
  dayofweek = daysOfTheWeek[now.dayOfTheWeek()];  // Lấy thông tin ngày trong tuần
  hour = now.hour();  // Lấy thông tin giờ
  minute = now.minute();  // Lấy thông tin phút
  second = now.second();  // Lấy thông tin giây

  // In thời gian ra Serial Monitor
  Serial.print("ESP32 RTC Date Time: ");
  Serial.print(String(year));
  Serial.print('/');
  Serial.print(String(month));
  Serial.print('/');
  Serial.print(String(day));
  Serial.print(" (");
  Serial.print(dayofweek);
  Serial.print(") ");
  Serial.print(String(hour));
  Serial.print(':');
  Serial.print(String(minute));
  Serial.print(':');
  Serial.println(String(second));
}


// Hàm xử lý khi đọc thẻ RFID
void RFID() {
  if (rfid.PICC_IsNewCardPresent()) {  // Kiểm tra nếu có thẻ mới
    if (rfid.PICC_ReadCardSerial()) {  // Đọc dữ liệu từ thẻ
      Serial.print("Tag UID: ");
      uidString = String(rfid.uid.uidByte[0]) + String(rfid.uid.uidByte[1]) + String(rfid.uid.uidByte[2]) + String(rfid.uid.uidByte[3]);
      Serial.println(uidString);

      rfid.PICC_HaltA();  // Tạm dừng thẻ
      rfid.PCD_StopCrypto1();  // Dừng mã hóa trên PCD

      userTimeCheckIn();  // Ghi lại thời gian check-in
      verifyCheckIn();  // Xác định trạng thái check-in

      flag_LCD = 0;  // Reset cờ hiển thị trên LCD

      flag_SDcard = 0;  // Reset cờ quản lý SD card

      xSemaphoreGive(Semaphore_SDcard);  // Tăng giá trị của semaphore từ 0 lên 1

      tone(buzzer, 1000);  // Kêu buzzer
      delay(100);
      noTone(buzzer);
    }
  }
}


// Hàm xử lý SD card
void SDcard() {
  if (flag_SDcard == 1) {
    SaveNewSinhvien();  // Lưu thông tin mới của sinh viên vào thẻ SD
  } else if (flag_SDcard == 2) {
    DeleteSinhvien();  // Xóa thông tin sinh viên khỏi thẻ SD
  } else if (flag_SDcard == 0) {
    CheckSinhvien();  // Kiểm tra thông tin sinh viên và lưu dữ liệu vào thẻ SD
  }
}

   
// Hàm xử lý LCD thông qua giao tiếp I2C
void I2CLCD() {
  if (flag_LCD == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);

    if (mssvString.isEmpty()) {
      lcd.print(uidString);
      for(int i=0;i<11-uidString.length();i++){
        lcd.print(' ');
      }
      if(uidString !="") lcd.print(" *:SAVE");
    } else {
      lcd.clear();
      lcd.print(mssvString);
      lcd.print(" ");
      lcd.print(tenString);
      lcd.print("        ");
      if(uidString !=""){
        lcd.setCursor(0, 2);
        lcd.print("*:DELETE");
      }
    }

    lcd.setCursor(0, 1);

    if (userCheckInDay <= 9) {
      lcd.print("0");
      lcd.print(userCheckInDay);
    } else {
      lcd.print(userCheckInDay);
    }
    lcd.print('/');
    if (userCheckInMonth <= 9) {
      lcd.print("0");
      lcd.print(userCheckInMonth);
    } else {
      lcd.print(userCheckInMonth);
    }

    lcd.print(" ");

    if (userCheckInHour <= 9) {
      lcd.print("0");
      lcd.print(userCheckInHour);
    } else {
      lcd.print(userCheckInHour);
    }
    lcd.print(':');
    if (userCheckInMinute <= 9) {
      lcd.print("0");
      lcd.print(userCheckInMinute);
    } else {
      lcd.print(userCheckInMinute);
    }
    lcd.print(" ");
    lcd.print(trangthai);
  } 
  
  else if (flag_LCD == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(pressedKey);
    lcd.print(" ");

    if (flag_tenmssv == 0 || flag_tenmssv == 1) {
      lcd.print("MSSV: ");
      lcd.print(mssvlcd);
      lcd.print("           ");
    }

    pressedKey = ' ';

    lcd.setCursor(0, 1);
    if (flag_tenmssv == 0) {
      lcd.print("  TEN:");
      lcd.print(tenlcd);
      lcd.print("            ");
    }
    if (flag_tenmssv == 0) {
      lcd.setCursor(0, 2);
      lcd.print("D:SAVE ");
    }
    else {
      lcd.print("#:NHAPTEN D:DELETE ");
    }
    
    if (flag_savenewsinhvien == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DONE SAVE");
      delay(1000);
      flag_savenewsinhvien = 0;
      flag_tenmssv = 2;
    }

    if (flag_deletesinhvien == 1) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DONE DELETE");
      delay(1000);
      flag_deletesinhvien = 0;
      flag_tenmssv = 2;
    }

    if (flag_tenmssv == 2){
      lcd.clear();
    }
  }
}



// Task xử lý chức năng của I2C Keypad
void task1(void *pvPara) {
  while (1) {
    Serial.println("I2CKeypad: ON");
    I2CKeypad();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// Task xử lý chức năng của RFID
void task2(void *pvPara) {
  while (1) {
    Serial.println("RFID: ON");
    RFID();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Task xử lý chức năng của LCD
void task3(void *pvPara) {
  while (1) {
    Serial.println("LCD: ON");
    I2CLCD();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Task xử lý chức năng của SD Card
void task4(void *pvPara) {
  while (1) {
    xSemaphoreTake(Semaphore_SDcard, portMAX_DELAY);
    SDcard();
  }
}

// Task xử lý chức năng của DS1307
void Timer_Task(TimerHandle_t xTimer) {
  Serial.println("DS1307: ON");
  DS1307();
}



void setup () {
 // Thiết lập chân LED và buzzer là đầu ra
pinMode(redLED, OUTPUT);  
pinMode(greenLED, OUTPUT);
pinMode(buzzer, OUTPUT);

// Khởi tạo Serial
Serial.begin(9600);
delay(1000);
while (!Serial);
// Keypad
Wire.begin(); 
keypad.begin(makeKeymap(keys)); 

// CÀI ĐẶT MODULE RTC
if (!rtc.begin()) {
  Serial.println("RTC module is NOT found");
  Serial.flush();
  while (1);
}

// Cài đặt tự động RTC với ngày và giờ trên máy tính
rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

// Khởi tạo bus SPI
SPI.begin(); 
// Khởi tạo MFRC522 
rfid.PCD_Init(); 

// Cài đặt cho thẻ SD
  Serial.print("Initializing SD card...");
if (!SD.begin(CS_SD)) {
  Serial.println("Initialization failed!");
  return;
}
Serial.println("Initialization done.");

// Khởi tạo LCD
lcd.init();               
lcd.backlight();


timer1 = xTimerCreate("RTC", pdMS_TO_TICKS(1000) , pdTRUE , 0 , Timer_Task );
//xTimerCreate tạo một timer
//"RTC": Tên của timer, pdMS_TO_TICKS(1000) chu kỳ thời gian, chuyển từ mili giây sang ticks
//pdTRUE chế độ tự động lặp lại timer sau khi hết chu kỳ, 0 id của timer, Timer_Task hàm callback được gọi khi timer đếm hết chu kỳ

if(timer1 == NULL) printf("Failed to create timer");
if (xTimerStart(timer1, 0) != pdPASS) printf("Failed to start timer");

Semaphore_SDcard = xSemaphoreCreateBinary(); // Tạo semaphore kiểu binary

xTaskCreatePinnedToCore(task1,"I2CKeypad",4096,NULL,1,NULL,1);
xTaskCreatePinnedToCore(task2,"RFID",16384,NULL,1,NULL,1);
xTaskCreatePinnedToCore(task3,"I2CLCD",4096,NULL,1,NULL,0);
xTaskCreatePinnedToCore(task4,"SDcard",16384,NULL,2,NULL,1);

}

void loop () {

}
