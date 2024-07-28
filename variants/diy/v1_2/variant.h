// For OLED LCD
#define I2C_SDA 21
#define I2C_SCL 22

// GPS
#undef GPS_RX_PIN
#undef GPS_TX_PIN
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_UBLOX

#define BUTTON_PIN 0  // The middle button GPIO on the T-Beam
#define LED_PIN 4           // add status LED (compatible with core-pcb and DIY targets)

#define USE_RF95
#define LORA_DIO0 2  // a No connect on the SX1262/SX1268 module
#define LORA_RESET 14 // RST for SX1276, and for SX1262/SX1268
#define LORA_DIO1 15  // IRQ for SX1262/SX1268
#define LORA_DIO2 RADIOLIB_NC  // BUSY for SX1262/SX1268

#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_CS 5