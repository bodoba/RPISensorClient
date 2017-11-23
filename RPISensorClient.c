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
#define MQTT_INTERFACE  "wlan0"
#define MQTT_PREFIX     "Sensor/BB-"
#define MAX_TRIES        5

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
    }
    return success;
}

int main(void)
{
    char id[8], topic[32], msg[64];
    bool success = false;

    if ( get_id(id) ) {
        sprintf( topic, "%s%s/Sensor", MQTT_PREFIX );
        if ( mqtt_init( MQTT_BROKER, MQTT_PORT ) ) {
            if(wiringPiSetup()!=-1) {

            }
        }
    }
    
    if ( success ) {
        if ( ! mqtt_publish( topic, msg ) ) {
            fprintf(stderr, "Error: Did not publish message: %s\n", msg);
        }
    } else {
        fprintf(stderr, "Error: Something went wrong\n");
    }

    mqtt_end();

    return success ? 0 : 1;
}  
