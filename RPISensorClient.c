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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/if.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>

#include <wiringPi.h>
#include "MQTT.h"

/* 
 * ---------------------------------------------------------------------------------------
 * Default settings
 * ---------------------------------------------------------------------------------------
 */
#define MQTT_BROKER_IP    "192.168.100.26"
#define MQTT_BROKER_PORT  1883
#define MQTT_KEEPALIVE    60
#define PID_FILE          "/var/run/RPISensorClient.pid"

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
#define REPORT_CYCLE  300 // how many cycles between two full reports

/* 
 * ---------------------------------------------------------------------------------------
 * Some globals we can't do without... ;)
 * ---------------------------------------------------------------------------------------
 */
bool debug    = false;
bool deamon   = true;
char *prefix  = "BB";
int  pidFilehandle = 0;
char *pidfile = PID_FILE;


/*
 * ---------------------------------------------------------------------------------------
 * sensor specific data
 * ---------------------------------------------------------------------------------------
 */
typedef struct {
    uint8_t pin;
    char   *label;
    bool   reverse;
} sensor_t;


/*
 * ---------------------------------------------------------------------------------------
 * Function prototypes
 * ---------------------------------------------------------------------------------------
 */
void readSensor(char* id, int pin, char* name, bool invert, uint8_t* value);
bool get_id ( char* id );
void sigendCB(int sigval);
void shutdown_daemon(void);

/*
 * ---------------------------------------------------------------------------------------
 * there are many ways to die
 * ---------------------------------------------------------------------------------------
 */
void sigendCB(int sigval)
{
    switch(sigval)
    {
        case SIGHUP:
            syslog(LOG_WARNING, "Received SIGHUP signal.");
            break;
        case SIGINT:
        case SIGTERM:
            syslog(LOG_INFO, "Daemon exiting");
            shutdown_daemon();
            exit(EXIT_SUCCESS);
            break;
        default:
            syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sigval));
            break;
    }
}

/*
 * ---------------------------------------------------------------------------------------
 * shutdwown deamon
 * ---------------------------------------------------------------------------------------
 */
void shutdown_daemon(void) {
    closelog();
    mqtt_end();
    if (deamon) {
        close(pidFilehandle);
        unlink(pidfile);
    }
}

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
        success = false;
    }
    return success;
}

/*
 * ---------------------------------------------------------------------------------------
 * Read sensor and publish value to MQTT broker
 * ---------------------------------------------------------------------------------------
 */
void readSensor(char* id, int pin, char* name, bool invert, uint8_t* old_value) {
    char topic[32], msg[64];
    uint8_t new_value = digitalRead(pin);
    if ( invert ) {
        if ( new_value != 0 ) {
            new_value = 0;
        } else {
            new_value = 1;
        }
    }
    if ( *old_value != new_value ) {
        *old_value = new_value;
        
        sprintf(topic, "%s/%s-%s/%d", name, prefix, id, pin);
        sprintf(msg, "{\"%s\":\"%d\"}", name, new_value);
        if ( debug ) {
            syslog(LOG_INFO, "%s %s\n", topic, msg);
        }
        if ( ! mqtt_publish( topic, msg ) ) {
            syslog(LOG_ERR, "Error: Did not publish message: %s\n", msg);
        }
    }
}

/*
 * ---------------------------------------------------------------------------------------
 * M A I N
 * ---------------------------------------------------------------------------------------
 */
int main(int argc, char *argv[]) {
    char id[8];

    openlog(NULL, LOG_PID, LOG_USER);       /* use syslog to create a trace            */
    
    /* ------------------------------------------------------------------------------- */
    /* Process command line options                                                    */
    /* ------------------------------------------------------------------------------- */
    
    /* FIXME: Use getopt_long and provide some help to the user just in case...        */
    for (int i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-d")) {       /* '-d' turns debug mode on                */
            debug = true;
            syslog(LOG_INFO, "Reading sensors every %d seconds", CYCLE_TIME);
            syslog(LOG_INFO, "Sending full report every %d cycles", REPORT_CYCLE);
        }
    }

    /* ------------------------------------------------------------------------------- */
    /* Deamonize                                                                       */
    /* ------------------------------------------------------------------------------- */
    if (deamon) {
        /* If we got a good PID, then we can exit the parent process                   */
        pid_t pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        } else  if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        
        umask(0);                           /* Change the file mode mask               */
        pid_t sid = setsid();               /* Create a new SID for the child process  */
        if (sid < 0) {
            syslog(LOG_ERR, "Could not get SID");
            exit(EXIT_FAILURE);
        }
        
        if ((chdir("/tmp")) < 0) {          /* Change the current working directory    */
            syslog(LOG_ERR, "Could not chage working dir to /tmp");
            exit(EXIT_FAILURE);
        }
        
        /* use /dev/null for the standard file descriptors                             */
        int fd = open("/dev/null", O_RDWR); /* Open /dev/null as STDIN                 */
        dup(fd);                            /* STDOUT to /dev/null                     */
        dup(fd);                            /* STDERR to /dev/null                     */
    } else {
        syslog(LOG_INFO, "Debug mode, not demonizing");
    }
    
    /* ------------------------------------------------------------------------------- */
    /* use lockfile to ensure only one copy is running                                 */
    /* ------------------------------------------------------------------------------- */
    if (deamon) {
        pidFilehandle = open(pidfile, O_RDWR|O_CREAT, 0600);
        
        if (pidFilehandle != -1 ) {                       /* Open failed               */
            if (lockf(pidFilehandle,F_TLOCK,0) != -1) {   /* Try to lock the pid file  */
                char buffer[10];
                sprintf(buffer,"%d\n",getpid());              /* Get and format PID    */
                write(pidFilehandle, buffer, strlen(buffer)); /* write pid to lockfile */
            } else {
                syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
                exit(EXIT_FAILURE);
            }
        } else {
            syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
            exit(EXIT_FAILURE);
        }
    } else {
        syslog(LOG_INFO, "Debug mode, not pid/lock File created");
    }

    /* ------------------------------------------------------------------------------- */
    /* Setup Wiring PI                                                                 */
    /* ------------------------------------------------------------------------------- */
    if(wiringPiSetup()==-1) {
        syslog(LOG_ERR, "Could not setiup wiringPI");
        exit(EXIT_FAILURE);
    } else {
        // Set pins sensors are connected to as input pins
        pinMode(SENSOR_LGT_PIN, INPUT);
        pinMode(SENSOR_SND_PIN, INPUT);
        pinMode(SENSOR_PIR_PIN, INPUT);
    }

    /* ------------------------------------------------------------------------------- */
    /* get MQTT ID basen on MAC address                                                */
    /* ------------------------------------------------------------------------------- */
    if ( !get_id(id) ) {
        syslog(LOG_ERR, "Could not read MAC address of interface %s\n", MQTT_INTERFACE );
        exit(EXIT_FAILURE);
    }

    /* ------------------------------------------------------------------------------- */
    /* initialize connection to MQTT server                                            */
    /* ------------------------------------------------------------------------------- */
    if ( !mqtt_init(MQTT_BROKER_IP, MQTT_BROKER_PORT, MQTT_KEEPALIVE)) {
        syslog(LOG_ERR, "Unable to connect to MQTT broker at %s:%d",
               MQTT_BROKER_IP, MQTT_BROKER_PORT);
        exit(EXIT_FAILURE);
    }

    /* ------------------------------------------------------------------------------- */
    /* Signals to handle                                                               */
    /* ------------------------------------------------------------------------------- */
    signal(SIGHUP,  sigendCB);      /* catch hangup signal                             */
    signal(SIGTERM, sigendCB);      /* catch term signal                               */
    signal(SIGINT,  sigendCB);      /* catch interrupt signal                          */

    /* ------------------------------------------------------------------------------- */
    /* now we can do our business                                                      */
    /* ------------------------------------------------------------------------------- */
    syslog(LOG_INFO, "Startup successfull" );
    
    uint8_t  lgt_value=100;
    uint8_t  snd_value=100;
    uint8_t  pir_value=100;
    uint32_t countdown = REPORT_CYCLE;

    for ( ;; ) {
        readSensor(id, SENSOR_LGT_PIN, "LGT", true,  &lgt_value);
        readSensor(id, SENSOR_SND_PIN, "SND", true,  &snd_value);
        readSensor(id, SENSOR_PIR_PIN, "PIR", false, &pir_value);
        
        if ( countdown > 0 ) {
            countdown--;
        } else {
            countdown = REPORT_CYCLE;
            // change value to enforce report of actual value
            syslog(LOG_INFO,"Trigger full report");
            lgt_value++;
            snd_value++;
            pir_value++;
        }
        sleep(CYCLE_TIME);
    }
    /* ------------------------------------------------------------------------------- */
    /* finish up                                                                       */
    /* ------------------------------------------------------------------------------- */
    shutdown_daemon();
    exit(EXIT_SUCCESS);

    return 0;
}
