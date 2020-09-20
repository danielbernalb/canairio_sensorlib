#include "Sensors.hpp"

// Honeywell and Panasonic
#ifndef SENSIRION
HardwareSerial hpmaSerial(1);
HPMA115S0 hpma115S0(hpmaSerial);
#endif
// Sensirium
SPS30 sps30;
// Humidity sensor
Adafruit_AM2320 am2320 = Adafruit_AM2320();

bool WrongSerialData = false;

/******************************************************************************
*   S E N S O R  M E T H O D S
******************************************************************************/

#ifdef SENSIRION
void ErrtoMess(char *mess, uint8_t r) {
    char buf[80];
    Serial.print(mess);
    sps30.GetErrDescription(r, buf, 80);
    Serial.println(buf);
}

void Errorloop(char *mess, uint8_t r) {
    if (r)
        ErrtoMess(mess, r);
    else
        Serial.println(mess);
    setErrorCode(ecode_sensor_timeout);
    delay(500);  // waiting for sensor..
}

void pmSensirionInit() {
    // Begin communication channel
    Serial.println(F("-->[SPS30] starting SPS30 sensor.."));
    if (sps30.begin(SP30_COMMS) == false) {
        Errorloop((char *)"-->[E][SPS30] could not initialize communication channel.", 0);
    }
    // check for SPS30 connection
    if (sps30.probe() == false) {
        Errorloop((char *)"-->[E][SPS30] could not probe / connect with SPS30.", 0);
    } else
        Serial.println(F("-->[SPS30] Detected SPS30."));
    // reset SPS30 connection
    if (sps30.reset() == false) {
        Errorloop((char *)"-->[E][SPS30] could not reset.", 0);
    }
    // start measurement
    if (sps30.start() == true)
        Serial.println(F("-->[SPS30] Measurement OK"));
    else
        Errorloop((char *)"-->[E][SPS30] Could NOT start measurement", 0);
    if (SP30_COMMS == I2C_COMMS) {
        if (sps30.I2C_expect() == 4)
            Serial.println(F("-->[E][SPS30] Due to I2C buffersize only PM values  \n"));
    }
}
#endif

/***
 * PM2.5 and PM10 read and visualization
 **/

void wrongDataState() {
    Serial.println("-->[E][PMSENSOR] !wrong data!");
#ifndef TTGO_TQ
    hpmaSerial.end();
#endif
    WrongSerialData = true;
    init();
    delay(500);
}

void Sensors::loop() {
#ifndef SENSIRION
    int try_sensor_read = 0;
    String txtMsg = "";
    while (txtMsg.length() < 32 && try_sensor_read++ < SENSOR_RETRY) {
        while (hpmaSerial.available() > 0) {
            char inChar = hpmaSerial.read();
            txtMsg += inChar;
        }
    }
    if (try_sensor_read > SENSOR_RETRY) {
        Serial.println("-->[E][PMSENSOR] read > fail!");
        Serial.println("-->[E][PMSENSOR] disconnected ?");
        delay(500);  // waiting for sensor..
    }
#endif

#ifdef PANASONIC
    if (txtMsg[0] == 02) {
        Serial.print("-->[SNGC] read > done!");
        pm25 = txtMsg[6] * 256 + byte(txtMsg[5]);
        pm10 = txtMsg[10] * 256 + byte(txtMsg[9]);
        if (pm25 > 2000 && pm10 > 2000) {
            wrongDataState();
        }
    } else
        wrongDataState();

#elif HONEYWELL  // HONEYWELL
    if (txtMsg[0] == 66) {
        if (txtMsg[1] == 77) {
            Serial.print("-->[HPMA] read > done!");
            statusOn(bit_sensor);
            pm25 = txtMsg[6] * 256 + byte(txtMsg[7]);
            pm10 = txtMsg[8] * 256 + byte(txtMsg[9]);
            if (pm25 > 1000 && pm10 > 1000) {
                wrongDataState();
            }
        } else
            wrongDataState();
    } else
        wrongDataState();
#else  // SENSIRION
    delay(35);  //Delay for sincronization
    do {
        ret = sps30.GetValues(&val);
        if (ret == ERR_DATALENGTH) {
            if (error_cnt++ > 3) {
                ErrtoMess((char *)"-->[E][SPS30] Error during reading values: ", ret);
                return;
            }
            delay(1000);
        } else if (ret != ERR_OK) {
            ErrtoMess((char *)"-->[E][SPS30] Error during reading values: ", ret);
            return;
        }
    } while (ret != ERR_OK);

    Serial.print("-->[SPS30] read > done!");
    statusOn(bit_sensor);

    pm25 = round(val.MassPM2);
    pm10 = round(val.MassPM10);

    if (pm25 > 1000 && pm10 > 1000) {
        wrongDataState();
    }
#endif
}

void pmSerialSensorInit() {
    delay(100);
#ifndef TTGO_TQ
    hpmaSerial.begin(9600, SERIAL_8N1, HPMA_RX, HPMA_TX);
#else
    if (WrongSerialData == false) {
        hpmaSerial.begin(9600, SERIAL_8N1, HPMA_RX, HPMA_TX);
    }
    delay(100);
#endif
}
void Sensors::init() {
    Serial.println("-->[SENSORS] starting PM sensor..");
#if defined HONEYWELL || defined PANASONIC
    pmSerialSensorInit();
#else  //SENSIRION
    pmSensirionInit();
#endif
    // TODO: enable/disable via flag
    Serial.println("-->[SENSORS] starting AM2320 sensor..");
    am2320.begin();  // temp/humidity sensor
}

void Sensors::getHumidityRead() {
    humi = am2320.readHumidity();
    temp = am2320.readTemperature();
    if (isnan(humi))
        humi = 0.0;
    if (isnan(temp))
        temp = 0.0;
    Serial.println("-->[AM2320] Humidity: " + String(humi) + " % Temp: " + String(temp) + " °C");
}

bool Sensors::isDataReady() {
    return dataReady;
}

uint16_t Sensors::getPM1() {
    return pm1;
}

String Sensors::getStringPM1() {
    char output[5];
    sprintf(output, "%03d", getPM1());
    return String(output);
}

uint16_t Sensors::getPM25() {
    return pm25;
}

String Sensors::getStringPM25() {
    char output[5];
    sprintf(output, "%03d", getPM25());
    return String(output);
}

uint16_t Sensors::getPM10() {
    return pm10;
}

String Sensors::getStringPM10() {
    char output[5];
    sprintf(output, "%03d", getPM10());
    return String(output);
}

float Sensors::getHumidity() {
    return humi;
}

float Sensors::getTemperature() {
    return temp;
}

float Sensors::getGas() {
    return gas;
}

float Sensors::getAltitude() {
    return alt;
}

float Sensors::getPressure() {
    return pres;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SENSORSHANDLER)
Sensors sensors;
#endif
