/*
 * ---------------------------------------------------------------------------------------
 * Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
 *
 * This file is part of the RPI Sensor Client 'RPISensorClient'
 *
 * ReadDHT11 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ReadDHT11 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RPISensorClient.  If not, see <http://www.gnu.org/licenses/>.
 * ---------------------------------------------------------------------------------------
 */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <wiringPi.h>
#include "MQTT.h"

/* 
 * ---------------------------------------------------------------------------------------
 * MQTT Broker to connect to
 * ---------------------------------------------------------------------------------------
 */
#define MQTT_BROKER     "192.168.100.26"
#define MQTT_PORT       1883

/*
 * ---------------------------------------------------------------------------------------
 * Topic will be up from the last two bytes of this interface concatenated to the 
 * defined prefix and followed by the PIN number the sensor is connected to
 * ---------------------------------------------------------------------------------------
 */
#define MQTT_INTERFACE  "eth0"

/*
 * ---------------------------------------------------------------------------------------
 * Where are the sensors connected to?
 * ---------------------------------------------------------------------------------------
 */
#define SENSOR_LGT_PIN  2
#define SENSOR_SND_PIN  3
#define SENSOR_PIR_PIN 12

/*
 * ---------------------------------------------------------------------------------------
 * Intervals
 * ---------------------------------------------------------------------------------------
 */
#define CYCLE_TIME      1 // seconds between two readings
#define REPORT_CYCLE  900 // how many cycles between two full reports

void readSensor(char* id, int pin, char* name, uint8_t* value);
bool get_id ( char* id );

/*
 * ---------------------------------------------------------------------------------------
 * get the last two bytes of the MAC address of the MQTT_INTERFACE
 * ---------------------------------------------------------------------------------------
 */
bool get_id ( char* id ) {
    bool success = false;
    struct ifreq s;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    
    strcpy(s.ifr_name, MQTT_INTERFACE );
    if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
        sprintf(id, "%02x%02x",s.ifr_addr.sa_data[4], s.ifr_addr.sa_data[5]);
        success = true;
    } else {
        fprintf(stderr, "Error: Could not read MAC address if interface %s\n",
                MQTT_INTERFACE );
        success = false;
    }
    return success;
}

/*
 * ---------------------------------------------------------------------------------------
 * Read sensor and publish value to MQTT broker
 * ---------------------------------------------------------------------------------------
 */
void readSensor(char* id, int pin, char* name, uint8_t* old_value) {
    char topic[32], msg[64];
    uint8_t new_value = digitalRead(pin);
    if ( *old_value != new_value ) {
        *old_value = new_value;
        
        sprintf(topic, "%s/BB-%s/%d", name, id, pin);
        sprintf(msg, "{\"%s\":\"%d\"}", name, new_value);
//        printf ( "%s %s\n", topic, msg);
        
        if ( ! mqtt_publish( topic, msg ) ) {
            fprintf(stderr, "Error: Did not publish message: %s\n", msg);
        }
    }
}

/*
 * ---------------------------------------------------------------------------------------
 * M A I N
 * ---------------------------------------------------------------------------------------
 */
int main(void)
{
    char id[8];
    uint8_t lgt_value=100;
    uint8_t snd_value=100;
    uint8_t pir_value=100;
    
    uint32_t countdown = REPORT_CYCLE;
    
    if(wiringPiSetup()!=-1) {
        pinMode(SENSOR_LGT_PIN, INPUT);
        pinMode(SENSOR_SND_PIN, INPUT);
        pinMode(SENSOR_PIR_PIN, INPUT);
        if ( get_id(id) ) {
            if ( mqtt_init(MQTT_BROKER, MQTT_PORT)) {
                // main cycle
                for ( ;; ) {
                    readSensor(id, SENSOR_LGT_PIN, "LGT", &lgt_value);
                    readSensor(id, SENSOR_SND_PIN, "SND", &snd_value);
                    readSensor(id, SENSOR_PIR_PIN, "PIR", &pir_value);
                    
                    if ( countdown > 0 ) {
                        countdown--;
                    } else {
                        countdown = REPORT_CYCLE;
                        // change value to enforce report of actual value
                        lgt_value++;
                        snd_value++;
                        pir_value++;
                    }
                    sleep(CYCLE_TIME);
                }
                mqtt_end();
            } else {
                fprintf(stderr, "Error: Could not connect to MQTT broker: %s:%d\n",
                        MQTT_BROKER,
                        MQTT_PORT);
            }
            
        }
    }
    return 0;
}
