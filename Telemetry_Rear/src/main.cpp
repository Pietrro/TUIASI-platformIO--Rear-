#include <Arduino.h>
#include "adcObj/adcObj.hpp"
//#include <ArduinoLog.h>
#include "driver/can.h"
#include "aditional/aditional.hpp"
#include <WiFi.h>
#include <ArduinoOTA.h>

#define TX_GPIO_NUM GPIO_NUM_14
#define RX_GPIO_NUM GPIO_NUM_27
#define LOG_LEVEL LOG_LEVEL_ERROR

u_int16_t status;

const char *ssid = "iPhone";
const char *password = "coscos27";

bool bspdActive = 0;
int bspdL;
int bspdH;
int dleft;
int dright;
int gear;
int a1, a2, a3, a4, a5, a6;
int bp;

adcObj damperRightRear(ADC1_CHANNEL_4);
adcObj damperLeftRear(ADC1_CHANNEL_5);
adcObj BSPD(ADC1_CHANNEL_6);          // era inainte channel 5
adcObj brakePressure(ADC1_CHANNEL_7); // era inainte channel 4

static const can_general_config_t g_config = {.mode = TWAI_MODE_NO_ACK, .tx_io = TX_GPIO_NUM, .rx_io = RX_GPIO_NUM, .clkout_io = TWAI_IO_UNUSED, .bus_off_io = TWAI_IO_UNUSED, .tx_queue_len = 1, .rx_queue_len = 5, .alerts_enabled = TWAI_ALERT_ALL, .clkout_divider = 0, .intr_flags = ESP_INTR_FLAG_LEVEL1};
static const can_timing_config_t t_config = CAN_TIMING_CONFIG_500KBITS();
static const can_filter_config_t f_config = CAN_FILTER_CONFIG_ACCEPT_ALL();

twai_message_t tx_msg_obj;

void setup()
{
   Serial.begin(115200);
   //Log.begin(LOG_LEVEL, &Serial);

   // Initializare pini GEAR
   pinMode(2, INPUT_PULLUP);
   pinMode(0, INPUT_PULLUP);
   pinMode(4, INPUT_PULLUP);
   pinMode(16, INPUT_PULLUP);
   pinMode(17, INPUT_PULLUP);
   pinMode(5, INPUT_PULLUP);

   // Initialize CAN
   status = can_driver_install(&g_config, &t_config, &f_config);
   if (status == ESP_OK)
   {
      Serial.println("Can driver installed");
   }
   else
   {
      Serial.println("Can driver installation failed with errors");
   }

   status = can_start();
   if (status == ESP_OK)
   {
      Serial.println("Can started");
   }
   else
   {
      Serial.println("Can starting procedure failed with errors");
   }

   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED)
   {
      delay(500);
      Serial.print(".");
   }

   Serial.println(WiFi.localIP());
   // Inițializare OTA
   ArduinoOTA.begin();

   tx_msg_obj.data_length_code = 8;
   tx_msg_obj.identifier = 0x111;
   tx_msg_obj.flags = CAN_MSG_FLAG_NONE;
}

void loop()
{
   ArduinoOTA.handle();
   dleft = damperLeftRear.getVoltage();
   // Serial.printf("%d\n", dleft);
   dright = damperRightRear.getVoltage();
   // Serial.printf("%d\n", dright);

   tx_msg_obj.data[0] = dleft / 100;
   tx_msg_obj.data[1] = dleft % 100;
   tx_msg_obj.data[2] = dright / 100;
   tx_msg_obj.data[3] = dright % 100;

   gear = 0;
   a1 = digitalRead(2);
   a2 = digitalRead(0);
   a3 = digitalRead(4);
   a4 = digitalRead(16);
   a5 = digitalRead(17);
   a6 = digitalRead(5);

   if (a1 == 0)
      gear = 1;
   else if (a2 == 0)
      gear = 2;
   else if (a3 == 0)
      gear = 3;
   else if (a4 == 0)
      gear = 4;
   else if (a5 == 0)
      gear = 5;
   else if (a6 == 0)
      gear = 6;
   else
      gear = 0;

   //Serial.printf("Gear: gear=%d\n", gear);

   tx_msg_obj.data[4] = gear;

   bp = brakePressure.getVoltage();

   tx_msg_obj.data[5] = bp / 100;
   tx_msg_obj.data[6] = bp % 100;

   if (BSPD.getVoltage() > 2000)
      bspdActive = 1;
   else
      bspdActive = 0;

   tx_msg_obj.data[7] = bspdActive;

   // 00 00 00 00

   // //* Function covert() bitshifts the values and puts them into 2 * 8 bits.
   // //* The value is checked if it is negative and if it is, it is converted to positive and in the last byte of the message the bit
   // //* that corresponds to its position gets converted to one
   // //* Example: if (second value is negative) then 0000+2=0010;

   status = can_transmit(&tx_msg_obj, pdMS_TO_TICKS(1000));
   if (status == ESP_OK)
   {
      Serial.printf("Can message damp sent");
   }
   else
   {
      Serial.printf("Can message sending failed with error code: %s ;\nRestarting CAN driver", esp_err_to_name(status));
      can_stop();
      can_driver_uninstall();
      can_driver_install(&g_config, &t_config, &f_config);
      status = can_start();
      if (status == ESP_OK)
         Serial.printf("Can driver restarted");
   }

   // Serial.print("CAN Msg: ");
   // for (int i = 0; i < 8; i++)
   // {
   //    Serial.printf("%02X ", tx_msg_obj.data[i]);
   // }
   delay(100);
}