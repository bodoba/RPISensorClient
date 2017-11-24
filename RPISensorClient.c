/*
 * ---------------------------------------------------------------------------------------
 * Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
 *
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


void readSensor(char* id, int pin, char* name);
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
        fprintf(stderr, "Error: Could not read MAC address if interface %s\n", MQTT_INTERFACE );
        success = false;
    }
    return success;
}


/*
 * ---------------------------------------------------------------------------------------
 * Read sensor and publish value to MQTT broker
 * ---------------------------------------------------------------------------------------
 */
void readSensor(char* id, int pin, char* name) {
    char topic[32], msg[64];
    int value = digitalRead(pin);
    sprintf(topic, "%s/BB-%s/%d", name, id, pin);
    sprintf(msg, "{\"%s\":\"%d\"}", name, value);
    printf ( "%s %s\n", topic, msg);
    if ( ! mqtt_publish( topic, msg ) ) {
        fprintf(stderr, "Error: Did not publish message: %s\n", msg);
    }
}

int main(void)
{
    char id[8];

    if(wiringPiSetup()!=-1) {
        pinMode(SENSOR_LGT_PIN, INPUT);
        pinMode(SENSOR_SND_PIN, INPUT);
        pinMode(SENSOR_PIR_PIN, INPUT);

        if ( get_id(id) ) {
            if ( mqtt_init(MQTT_BROKER, MQTT_PORT)) {
                readSensor(id, SENSOR_LGT_PIN, "LM393");
                readSensor(id, SENSOR_SND_PIN, "MHSND");
                readSensor(id, SENSOR_PIR_PIN, "ME003");
            } else {
                fprintf(stderr, "Error: Could not connect to MQTT broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
            }
        }
    }
    mqtt_end();
    return 0;
}
