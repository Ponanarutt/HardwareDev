# 🎯 **HardwareDev - รหัสวิชา 01204114**  

### **📌 จัดทำโดย**  
- **รุษฐนพล อูปทอง** | **รหัสนักศึกษา:** 6510503735  
- **คณะวิศวกรรมศาสตร์ | สาขา วิศวกรรมคอมพิวเตอร์**  

---

## **📌 รายละเอียดโปรเจค**
โครงการนี้เป็น **เกมบิงโกที่ใช้ ESP32-S3** เป็นตัวควบคุมหลักในการสุ่มตัวเลขและส่งข้อมูลผ่าน **MQTT** ไปยัง Player Board ซึ่งใช้ **จอแสดงผล TFT, OLED, LCD** และ **Buzzer** เพื่อแสดงผลสถานะของเกม  

---

## **รายการไฟล์ในโปรเจค**
### **ไฟล์โค้ดหลักของโปรเจค (Arduino Code)**
| 📄 **ไฟล์**          | 📝 **รายละเอียด** |
|----------------------|------------------|
| `MotherFinal.ino`   | **Mother Board (ESP32-S3)** ควบคุมการสุ่มเลขบิงโก, เชื่อมต่อ MQTT และส่งสถานะเกมไปยัง Player Board |
| `playerFinal.ino`   | **Player Board (ESP32-S3)** รับค่าตัวเลขจาก Mother Board ผ่าน MQTT และแสดงผลบนจอ **TFT/OLED** |

---

### **ไฟล์ภาพ Diagram การเชื่อมต่ออุปกรณ์**
| 🖼 **ไฟล์**         | 📝 **รายละเอียด** |
|---------------------|------------------|
| `Motherdis.png`    | Diagram การต่อสายของ **Mother Board** เช่น **ปุ่มกด, จอแสดงผล, WiFi** |
| `Playerdis.png`    | Diagram การต่อสายของ **Player Board** เช่น **จอ TFT, OLED, ปุ่มกด, Buzzer** |

---

## **🛠 ไลบรารีที่ใช้**
### **ไลบรารีสำหรับการพัฒนา ESP32**
- `Arduino IDE` → ใช้เขียนโค้ดและอัปโหลดไปยัง ESP32  
- `ESP32 WiFi Library` → ใช้เชื่อมต่อ WiFi สำหรับ ESP32  
- `PubSubClient` → ใช้สำหรับเชื่อมต่อ **MQTT Broker (Mosquitto)**  

### **ไลบรารีสำหรับจอแสดงผล**
- `Adafruit_GFX` & `Adafruit_ST7735` → ใช้ควบคุมจอ **TFT Display**  
- `Adafruit_SSD1306` → ใช้ควบคุมจอ **OLED**  
- `LiquidCrystal_I2C` → ใช้ควบคุมจอ **LCD 16x2 ผ่าน I2C**  

### **เครื่องมือเสริม**
- `Node-RED` → ใช้สร้าง **Dashboard** และจัดการ **MQTT Broker**  
- `MQTT Broker (Mosquitto)` → ใช้เผยแพร่ข้อมูลหมายเลขบิงโกผ่าน **MQTT**  

---

## **🚀 วิธีใช้งาน**
1. **เชื่อมต่อ Mother Board (ESP32-S3) และ Player Board ตาม Diagram**  
2. **เปิด `MotherFinal.ino` และ `playerFinal.ino` บน Arduino IDE**  
3. **ตั้งค่า WiFi และ MQTT Broker ใน `config.h`**  
4. **อัปโหลดโค้ดไปยัง ESP32-S3**  
5. **เปิด Node-RED และตั้งค่า MQTT Dashboard**  
6. **เริ่มเล่นเกม Bingo โดย Mother Board จะสุ่มเลขและส่งไปยัง Player Board**  

---




