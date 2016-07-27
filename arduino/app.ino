/**
 * 
 *
 * Based on: 
 * https://github.com/whitebox-labs/tentacle/blob/master/examples/asynchronous/i2c_asynchronous/i2c_asynchronous.ino
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <Wire.h>
#include <LiquidCrystal.h>

// Change this depending on wiring.
LiquidCrystal lcd(12, 11, 6, 5, 4, 3);

// A list of I2C ids that you set your circuits to.
int channel_ids[] = {99, 100};

// Change to false if you prefer celcius.
boolean const display_temp_in_farenheit = true;

// A list of channel names (must be the same order as in channel_ids[]) - 
// only used to designate the readings in serial communications
// {"DO", "ORP", "PH", "EC"};
char const *channel_names[] = {"PH", "EC"};

// set how many I2C circuits are attached to the Tentacle
#define TOTAL_CIRCUITS 2

// set baud rate for host serial monitor(pc/mac/other)
const unsigned int baud_host  = 9600;

// set at what intervals the readings are sent to the computer 
// (NOTE: this is not the frequency of taking the readings!)
const unsigned int send_readings_every = 2500; // 5000;

unsigned long next_serial_time;

// A 30 byte character array to hold incoming data from the sensors
char sensordata[30];
// We need to know how many characters bytes have been received
byte sensor_bytes_received = 0;
// used to hold the I2C response code.
byte code = 0;
// used as a 1 byte buffer to store in bound bytes from the I2C Circuit.
byte in_char = 0;

// an array of strings to hold the readings of each channel
String readings[TOTAL_CIRCUITS];

// INT pointer to hold the current position in the 
// channel_ids/channel_names array
int channel = 0;

// time to wait for the circuit to process a read command. 
// datasheets say 1 second.
const unsigned int reading_delay = 1000;

// holds the time when the next reading should be ready from the circuit
unsigned long next_reading_time;

// wether or not we're waiting for a reading
boolean request_pending = false;

// the frequency of the led blinking, in milliseconds
const unsigned int blink_frequency = 250;

// holds the next time the led should change state
unsigned long next_blink_time;

// keeps track of the current led state
boolean led_state = LOW;

// 
float temp;
float ph;


/**
 * setup()
 */
void setup() {
    // LCD's number of columns and rows.
    lcd.begin(16, 2);

    // set pinMode for the temp probe
    pinMode(2, OUTPUT);

    Serial.begin(9600);
    Wire.begin();

    lcd.setCursor(0, 0);
    lcd.print("Tank-o-meter");
}


void loop() {
    do_temp_reading();
    do_sensor_readings();
    do_serial();
}


void do_temp_reading() {
    float voltage_out;
    digitalWrite(A0, LOW);

    digitalWrite(2, HIGH);
    delay(2);

    voltage_out = analogRead(0);

    digitalWrite(2, LOW);

    voltage_out = voltage_out * .0048;
    voltage_out = voltage_out * 1000;

    temp = 0.0512 * voltage_out - 20.5128;
}


/**
 * update_display()
 *
 * Write to the LCD display
 */
void update_display() {

    // temp 
    lcd.setCursor(0, 0);
    lcd.print("T:       ");
    lcd.setCursor(3, 0);

    if (display_temp_in_farenheit) {
        float converted_temp;

        converted_temp = temp * 1.8;
        converted_temp = converted_temp + 32;
        lcd.print(converted_temp, 1);
    } else {
        lcd.print(temp);
    }

    // pH == 0
    lcd.setCursor(8, 0);
    lcd.print("pH: ");
    lcd.setCursor(12, 0);
    lcd.print(readings[0]);

    // Salinity == 1
    String salinity = get_salinity(readings[1]);

    lcd.setCursor(0, 1);
    lcd.print(salinity);
}

/**
 * get_salinity()
 *
 * reading    String from the conductivity circuit. eg:
 *            0.00,0,0.00,1.000
 *
 *            Which is in the format of: EC,TDS,Salinity,SG
 *              Where parameter is
 *               EC      Electrical conductivity
 *               TDS     Total dissolved solids
 *               S       Salinity
 *               SG      Specific gravity of sea water
 *
 * For purposes of display on the LCD, I only want specific gravity.
 */
String get_salinity(String reading) {
    int ec_idx = reading.indexOf(",");
    if (ec_idx == -1) {
        return "";
    }
    int tds_idx = reading.indexOf(",", ec_idx + 1);
    int sal_idx = reading.indexOf(",", tds_idx + 1);

    return reading.substring(sal_idx + 1);
}


/** 
 * do serial communication in a "asynchronous" way
 */
void do_serial() {
    // is it time for the next serial communication?
    if (millis() >= next_serial_time) {
        Serial.print("Temp: ");
        Serial.println(temp);

        for (int i = 0; i < TOTAL_CIRCUITS; i++) {
            Serial.print(channel_names[i]);
            Serial.print(": ");
            Serial.println(readings[i]);
        }
        next_serial_time = millis() + send_readings_every;
        update_display();
    }
}


/**
 * take sensor readings in an "asynchronous" way
 */
void do_sensor_readings() {
    // Is there a pending request?
    if (request_pending) {
        if (millis() >= next_reading_time) {
            receive_reading();
        }
    } else {
        // nope...
        // switch to the next channel (increase current channel by 1,
        // and roll over if we're at the last channel using the %
        // modulo operator)
        channel = (channel + 1) % TOTAL_CIRCUITS;
        request_reading();
    }
}


/**
 * Request a reading from the current channel
 */
void request_reading() {
    request_pending = true;

    // call the circuit by its ID number.
    Wire.beginTransmission(channel_ids[channel]);

    // request a reading by sending 'r'
    Wire.write('r');

    // end the I2C data transmission.
    Wire.endTransmission();

    // calculate the next time to request a reading
    next_reading_time = millis() + reading_delay;
}


/**
 * Receive data from the I2C bus
 */
void receive_reading() {
    // reset data counter
    sensor_bytes_received = 0;

    // clear sensordata array;
    memset(sensordata, 0, sizeof(sensordata));

    // call the circuit and request 48 bytes (this is more then we need).
    Wire.requestFrom(channel_ids[channel], 48, 1);

    code = Wire.read();

    // are there bytes to receive?
    while (Wire.available()) {
        // receive a byte.
        in_char = Wire.read();

        // if we see that we have been sent a null command.
        if (in_char == 0) {
            Wire.endTransmission();
            break;
        } else {
            // load this byte into our array.
            sensordata[sensor_bytes_received] = in_char;
            sensor_bytes_received++;
        }
    }

    switch (code) {
        case 1:         // decimal 1  means the command was successful.
            readings[channel] = sensordata;
            break;
        case 2:         // decimal 2 means the command has failed.
            readings[channel] = "error: command failed";
            break;
        case 254:       // decimal 254  means the command has not finished.
            readings[channel] = "reading not ready";
            break;
        case 255:       // decimal 255 means there is no further data to send.
            readings[channel] = "error: no data";
            break;
    }

    // set pending to false, so we can continue to the next sensor
    request_pending = false;
}

