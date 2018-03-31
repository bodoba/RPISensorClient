/*
 * ---------------------------------------------------------------------------------------
 * Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
 *
 *
 * This file is part of the RPI DHT11 Sensor Client 'ReadDHT11'
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
 * along with ReadDHT11.  If not, see <http://www.gnu.org/licenses/>.
 * ---------------------------------------------------------------------------------------
 */

#include "DHT11.h"

bool dht11_read_val( uint8_t pin, int *humidity, int *celcius ) {
    uint8_t lststate=HIGH;
    uint8_t counter=0;
    uint8_t j=0, i;
    bool success = false;
    int dht11_val[5]={0,0,0,0,0};
    
    pinMode(pin,OUTPUT);
    digitalWrite(pin,LOW);
    delay(18);
    digitalWrite(pin,HIGH);
    
    delayMicroseconds(40);
    pinMode(pin,INPUT);
    
    for (i=0;i<MAX_TIME;i++) {
        counter=0;
        while(digitalRead(pin)==lststate){
            counter++;
            delayMicroseconds(1);
            if(counter==255) {
                break;
            }
        }
        lststate=digitalRead(pin);
        if (counter==255) {
            break;
        }
        
        // top 3 transistions are ignored
        if ((i>=4)&&(i%2==0)) {
            dht11_val[j/8]<<=1;
            if(counter>16) {
                dht11_val[j/8]|=1;
            }
            j++;
        }
    }
    
    // verify cheksum and print the verified data
    if( (j>=40) && (dht11_val[4]==((dht11_val[0]+dht11_val[1]+dht11_val[2]+dht11_val[3]) & 0xFF))) {
        *humidity = dht11_val[0];
        *celcius  = dht11_val[2];
        success = true;
    }
    
    return success;
}
