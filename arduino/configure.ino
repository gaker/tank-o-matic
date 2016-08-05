/**
 * 
 *
 * Largely based on https://github.com/whitebox-labs/tentacle/blob/master/examples/tentacle-setup/tentacle_setup_i2c_only/tentacle_setup_i2c_only.ino
 *
 *
 */
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>

// A 30 byte character array to hold incoming data from the sensors
char sensordata[30];

// We need to know how many characters bytes have been received
byte computer_bytes_received = 0;

// We need to know how many characters bytes have been received
byte sensor_bytes_received = 0;

// used to hold the I2C response code.
byte i2c_response_code = 0;

// used as a 1 byte buffer to store in bound bytes from an I2C stamp.
byte in_char = 0;

String stamp_type;

// hold the version of the stamp
char stamp_version[4];

// INT pointer for channel switching - 8-127 I2C addresses
int channel;
int retries;

// error-byte to store result of Wire.transmissionEnd()
byte error;

//Char pointer used in string parsing
char const *cmd;

// we make a 20 byte character array to hold incoming 
// data from a pc/mac/other.
char computerdata[20];

// The shield uses the I2C SCL and SDA pins. On classic Arduinos
// this is Analog 4 and 5 so you can't use those for analogRead() anymore
// However, you can connect other I2C sensors to the I2C bus and share
// the I2C bus.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define OFF 0x0
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

/**
 * setup()
 */
void setup() {
    // LCD's number of columns and rows.
    lcd.begin(16, 2);
  
    lcd.setBacklight(WHITE);

    Serial.begin(9600);
    Wire.begin();

    lcd.setCursor(0, 0);
    lcd.print("Setup Mode. See");
    lcd.setCursor(0, 1);
    lcd.print("serial console");

    Serial.flush();
    Serial.println(F("================================="));
    Serial.println(F("Configure Atlas Scientific Probes"));
    Serial.println(F("================================="));

    scan();
}


/**
 * Are we trying to change the channel?
 */
boolean check_channel_change(String cmd) {

    for (int x = 0; x <= 127; x++) {
        if (cmd == String(x)) {
            return true;
        }
    }
    return false;
}


/**
 * loop()
 */
void loop() {

    if (computer_bytes_received != 0) {
        cmd = computerdata;

        if (check_channel_change(String(cmd))) {

            channel = atoi(cmd);
            Serial.print("Changing channel to ");
            Serial.println(cmd);

            if (change_channel()) {
                // PH
                if (channel == 99) {
                    lcd.setCursor(0, 0);
                    lcd.print("Configuring     ");
                    lcd.setCursor(0, 1);
                    lcd.print("pH Probe        ");

                    Serial.println("\r\r");
                    Serial.println("Calibrate the PH probe.");
                    Serial.println("For more information, see: http://www.atlas-scientific.com/_files/_datasheets/_circuit/pH_EZO_datasheet.pdf");

                    Serial.println("\r\r");
                    Serial.print("====================================");
                    Serial.println("====================================");

                    Serial.println("Clear: Cal,clear");
                    Serial.println("Mid: Cal,mid,X.XX");
                    Serial.println("Low: Cal,low,X.XX");
                    Serial.println("High: Cal,high,X.XX");
                    Serial.println("");
                    Serial.print("Issuing the cal,mid command after the EZO ");
                    Serial.println("class pH circuit has been calibrated will");
                    Serial.print("clear other calibration points. Full calibration ");
                    Serial.println("will have to be redone.");
                    Serial.println("");
                    Serial.println("Issuing a cal,low or cal,high command can be done at any time");
                    Serial.println("and will have no effect on other previously set calibration points.");
                    Serial.print("====================================");
                    Serial.println("====================================");
                } else if (channel == 100) {
                    lcd.setCursor(0, 0);
                    lcd.print("Configuring     ");
                    lcd.setCursor(0, 1);
                    lcd.print("Conductivity    ");


                    Serial.println("\r\r");
                    Serial.println("Calibrate the Conductivity probe.");
                    Serial.println("For more information, see: http://www.atlas-scientific.com/_files/_datasheets/_circuit/EC_EZO_Datasheet.pdf");

                    Serial.println("\r\r");
                    Serial.print("====================================");
                    Serial.println("====================================");

                    Serial.println("Clear: Cal,clear");
                    Serial.println("Dry: Cal,dry");
                    Serial.println("One: Cal,one,n");
                    Serial.println("Low: Cal,low,n");
                    Serial.println("High: Cal,high,n");
                    Serial.println("Query: Cal,?");
                }
            } else {
                Serial.println("CHANNEL NOT AVAILABLE! Empty slot? Not in I2C mode?");
            }

            computer_bytes_received = 0;
            return;
        }

        // Run the command
        Serial.print("> ");
        Serial.println(cmd);
        I2C_call(true);

        if (sensor_bytes_received > 0) {
            Serial.print("< ");
            Serial.println(sensordata);
        }

        computer_bytes_received = 0;
    }

    if (Serial.available() > 0) {
        computer_bytes_received = Serial.readBytesUntil(13, computerdata, 20);
        computerdata[computer_bytes_received] = 0;
    }

}


/**
 * change_channel()
 */
boolean change_channel() {
    stamp_type = "";
    memset(stamp_version, 0, sizeof(stamp_version));

    for (int x = 1; x <= 127; x++) {
        if (channel == x) {
            if (!check_i2c_connection()) {
                return false;
            }
            return true;
        }
    }

    return false;
}

/**
 * check selected i2c channel/address. verify that it's working by 
 * requesting info about the stamp
 */
boolean check_i2c_connection() {

    retries = 0;

    while (retries < 3) {
        retries++;
        // just do a short connection attempt without command 
        // to scan i2c for devices

        Wire.beginTransmission(channel);
        error = Wire.endTransmission();

        if (error == 0) {
            // if error is 0, there's a device
            int r_retries = 0;
            while (r_retries < 3) {
                r_retries++;

                cmd = "i";
                // set cmd to request info (in I2C_call())
                I2C_call(false);

                if (parseInfo()) {
                    return true;
                }
            }
        }
    }
    // no device at this address
    return false;
}


void scan() {
    Serial.println(F("Starting  I2C scan..."));

    int stamp_amount = 0;

    for (channel = 1; channel < 127; channel++) {
        if (change_channel()) {
            stamp_amount++;

            Serial.println(F("=================="));
            Serial.print(F("--CHANNEL: "));
            Serial.println(channel);
            Serial.print(F("-- Type: "));
            Serial.println(stamp_type);
        }
    }

    Serial.println(F("\r\r"));
    Serial.println(F("SCAN COMPLETE"));
    Serial.println(F("-------------------------------------------------"));
    Serial.println(F("Choose the channel id for the probe to configure."));
    Serial.println(F("-------------------------------------------------\r\r"));
}

/**
 * parses the answer to a "i" command. returns true if 
 * answer was parseable, false if not.
 *
 * example:
 * PH EZO  -> '?I,pH,1.1'
 * ORP EZO -> '?I,OR,1.0'   (-> wrong in documentation 'OR' instead of 'ORP')
 * DO EZO  -> '?I,D.O.,1.0' || '?I,DO,1.7' (-> exists in D.O. and DO form)
 * EC EZO  -> '?I,EC,1.0 '
 */
boolean parseInfo() {
    if (sensordata[0] == '?' && sensordata[1] != 'I') {
        Serial.println("Unsupported Sensor type");
        return false;
    }

    // PH EZO
    if (sensordata[3] == 'p' && sensordata[4] == 'H') {
        stamp_type = F("pH EZO");
        stamp_version[0] = sensordata[6];
        stamp_version[1] = sensordata[7];
        stamp_version[2] = sensordata[8];
        stamp_version[3] = 0;
        return true;

    } else if (sensordata[3] == 'O' && sensordata[4] == 'R') {
        stamp_type = F("ORP EZO");
        stamp_version[0] = sensordata[6];
        stamp_version[1] = sensordata[7];
        stamp_version[2] = sensordata[8];
        stamp_version[3] = 0;
        return true;

    } else if (sensordata[3] == 'D' && sensordata[4] == 'O') {
        stamp_type = F("D.O. EZO");
        stamp_version[0] = sensordata[6];
        stamp_version[1] = sensordata[7];
        stamp_version[2] = sensordata[8];
        stamp_version[3] = 0;

        return true;

    } else if (
                sensordata[3] == 'D' &&
                sensordata[4] == '.' &&
                sensordata[5] == 'O' &&
                sensordata[6] == '.') {
        stamp_type = F("D.O. EZO");
        stamp_version[0] = sensordata[8];
        stamp_version[1] = sensordata[9];
        stamp_version[2] = sensordata[10];
        stamp_version[3] = 0;

        return true;

    } else if (sensordata[3] == 'E' && sensordata[4] == 'C') {
        stamp_type = F("EC EZO");
        stamp_version[0] = sensordata[6];
        stamp_version[1] = sensordata[7];
        stamp_version[2] = sensordata[8];
        stamp_version[3] = 0;

        return true;
    } else {
        stamp_type = F("unknown EZO stamp");
        return true;
    }

    Serial.println("can not parse data: ");
    Serial.print("'");
    Serial.print(sensordata);
    Serial.println("'");
    return false;
}

/**
 * parse and call I2C commands.
 */
byte I2C_call(boolean include_output) {

    // reset data counter
    sensor_bytes_received = 0;

    // clear sensordata array;
    memset(sensordata, 0, sizeof(sensordata));

    //call the circuit by its ID number.
    Wire.beginTransmission(channel);

    //transmit the command that was sent through the serial port.
    Wire.write(cmd);

    //end the I2C data transmission.
    Wire.endTransmission();

    i2c_response_code = 254;

    // in case the cammand takes longer to process, we keep 
    // looping here until we get a success or an error
    while (i2c_response_code == 254) {
        if (String(cmd).startsWith("cal") || String(cmd).startsWith("Cal")) {
            // cal-commands take 1600ms or more
            delay(1600);
        } else if (String(cmd) == "r" || String(cmd) == "R") {
            // reading command takes about a second
            delay(1000);
        } else {
            // all other commands: wait 300ms
            delay(300);
        }

    //call the circuit and request 48 bytes (this is more then we need).
    Wire.requestFrom(channel, 32);

    //the first byte is the response code, we read this separately.
    i2c_response_code = Wire.read();

    //are there bytes to receive.
    while (Wire.available()) {
        //receive a byte.
        in_char = Wire.read();

        //if we see that we have been sent a null command.
        if (in_char == 0) {
            // some arduinos (e.g. ZERO) put padding zeroes in the 
            // receiving buffer (up to the number of requested bytes)
            while(Wire.available()) { Wire.read(); }
            break;
        } else {
            //load this byte into our array.
            sensordata[sensor_bytes_received] = in_char;
            sensor_bytes_received++;
        }
    }

    switch (i2c_response_code) {
        case 1:    // decimal 1.
            // The command was successful.
            if (include_output) {
                Serial.println( "< success");
            }
            break;
        case 2:    // decimal 2.
            // The command has failed.
            if (include_output) {
                Serial.println( "< command failed");
            }
            break;
        case 254:  // decimal 254.
            // The command is pending
            if (include_output) {
                Serial.println( "< command pending");
            }
            break;
        case 255:  // decimal 255.
            // No more data to send.
            if (include_output) {
                Serial.println( "No Data");
            }
            break;
        }
    }
}


