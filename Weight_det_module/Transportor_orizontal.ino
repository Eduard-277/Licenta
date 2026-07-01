#include <EtherCard.h>
#include <Modbus.h>
#include <ModbusIP_ENC28J60.h>
#include <HX711.h>

const int LOADCELL_DOUT_PIN = A0;
const int LOADCELL_SCK_PIN = A1;

HX711 scale;

const int SENSOR_IREG_word0=100;
const int SENSOR_IREG_word1=101;
const int CALIBRATION_RESULT_IREG_word0=102;
const int CALIBRATION_RESULT_IREG_word1=103;
const int CALIBRATION_RESULT_DONE_IREG=104;
const int COMMAND_HREG=200;
const int CALIBRATION_HREG_word0=202;
const int CALIBRATION_HREG_word1=203;




ModbusIP mb;
long ts;
int weight=0;
int command_value;
float weight_float=0;
float scale_measure=1,scale_new_measure=1;

bool ok=0;

union Conversion_word_float{
  word type_Word[2];
  float type_float;
}calibration,calibration_result,weight_conversion,scale_set;

float read_calibration()
{
    calibration.type_Word[1] = mb.Hreg(CALIBRATION_HREG_word0);
    calibration.type_Word[0] = mb.Hreg(CALIBRATION_HREG_word1);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  byte mac[]={0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
  byte ip[]={192, 168, 0, 3};
  mb.config(mac,ip);
  mb.addIreg(SENSOR_IREG_word0);
  mb.addIreg(SENSOR_IREG_word1);
  mb.addIreg(CALIBRATION_RESULT_IREG_word0);
  mb.addIreg(CALIBRATION_RESULT_IREG_word1);
  mb.addHreg(COMMAND_HREG);
  mb.addIreg(CALIBRATION_RESULT_DONE_IREG);
  mb.addHreg(CALIBRATION_HREG_word0);
  mb.addHreg(CALIBRATION_HREG_word1);

  ts=millis();
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.tare();
}

void loop() {
  // put your main code here, to run repeatedly:
  mb.task();

  if(millis()>ts+2000)
  {
    ts=millis();

    read_calibration();
    scale_measure=calibration.type_float;
    scale.set_scale(scale_measure);
    Serial.println("SCALE afara comenzii:");
    Serial.println(scale_measure);

  command_value= mb.Hreg(COMMAND_HREG);
  switch(command_value)
  {
    case 0:
      scale_measure= scale.get_scale();
      weight_float= scale.get_units(10);
      weight=(int)weight_float;


      weight_conversion.type_float=weight_float;
      mb.Ireg(SENSOR_IREG_word0,weight_conversion.type_Word[0]);
      mb.Ireg(SENSOR_IREG_word1,weight_conversion.type_Word[1]);

      mb.Ireg(CALIBRATION_RESULT_DONE_IREG,0);


      Serial.print("UNITS: ");
      Serial.println(weight_float);
      Serial.println(weight);
      Serial.println("SCALE:");
      Serial.println(scale_measure);
      Serial.println("CALIBRATION_word0:");
      Serial.println(mb.Hreg(CALIBRATION_HREG_word0));
      Serial.println("CALIBRATION_word1:");
      Serial.println(mb.Hreg(CALIBRATION_HREG_word1));
      Serial.println("COMMAND:");
      Serial.println(mb.Hreg(COMMAND_HREG));
    break;
  
    case 1:
      if(ok==0)
      {
        scale.tare();
        Serial.print("pune greutate");
        mb.Ireg(CALIBRATION_RESULT_DONE_IREG,1);
        // delay(5000);
        // scale.calibrate_scale(1000,5);
         ok=1;
      }
      else
      {
        mb.Ireg(CALIBRATION_RESULT_DONE_IREG,1);
      }
    break;

    case 2:
      if(ok==1)
      {
        scale.calibrate_scale(1000,5);
        scale_new_measure= scale.get_scale();
        scale_set.type_float=scale_new_measure;
        Serial.println("SCALE NEWwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww:");
        Serial.println(scale_new_measure);
        mb.Ireg(CALIBRATION_RESULT_IREG_word0,scale_set.type_Word[1]);
        mb.Ireg(CALIBRATION_RESULT_IREG_word1,scale_set.type_Word[0]);

        ok=0;
        command_value=0;
      }
      else
      {
        mb.Ireg(CALIBRATION_RESULT_DONE_IREG,2);
      }
    break;
  }


  }

}


