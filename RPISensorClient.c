/*
 * ---------------------------------------------------------------------------------------
 * Copyright 2017 by Bodo Bauer <bb@bb-zone.com>
 *
 * This file is part of the RPI Sensor Client 'RPISensorClient'
 *
 * PRISensorClient is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PRISensorClient is distributed in the hope that it will be useful,
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
#include <sys/time.h>

/*
 * ---------------------------------------------------------------------------------------
 * Default settings - may be overwritten by config file values
 * ---------------------------------------------------------------------------------------
 */
#define MQTT_BROKER_IP    "192.168.100.26"
#define MQTT_BROKER_PORT  1883
#define MQTT_KEEPALIVE    60
#define PID_FILE          "/var/run/RPISensorClient.pid"
#define REPORT_CYCLE      300
#define MQTT_INTERFACE    "eth0"
#define DEBUG             0
#define PREFIX            "BB"
#define CONFIG_FILE       "/etc/rpisensorclient.cfg"

/*
 * ---------------------------------------------------------------------------------------
 * Maximum number of sensors to be monitored
 * ---------------------------------------------------------------------------------------
 */
#define MAX_SENSORS    32

/*
 * ---------------------------------------------------------------------------------------
 * Value a sensor would never report
 * ---------------------------------------------------------------------------------------
 */
#define RESET_VALUE   100

/*
 * ---------------------------------------------------------------------------------------
 * Some globals we can't do without... ;)
 * ---------------------------------------------------------------------------------------
 */
int      pidFilehandle    = 0;
bool     deamon           = true;
int8_t   debug            = DEBUG;
char     *prefix          = PREFIX;
char     *pidfile         = PID_FILE;
char     *configFile      = CONFIG_FILE;
char     *mqtt_broker_ip  = MQTT_BROKER_IP;
int      mqtt_broker_port = MQTT_BROKER_PORT;
int      mqtt_keepalive   = MQTT_KEEPALIVE;
char*    mqtt_interface   = MQTT_INTERFACE;
uint64_t report_cycle     = REPORT_CYCLE;

/*
 * ---------------------------------------------------------------------------------------
 * sensor specific data
 * ---------------------------------------------------------------------------------------
 */
typedef enum { DIGITAL, DHT11_TMP, DHT11_HMD } sensorType_t;

typedef struct {
    uint8_t       pin;
    sensorType_t  type;
    uint32_t      freq;
    char          *label;
    bool          invert;
    uint8_t       value;
    uint64_t      next_read;
} sensor_t;

sensor_t sensor_list[MAX_SENSORS];

/*
 * ---------------------------------------------------------------------------------------
 * Function prototypes
 * ---------------------------------------------------------------------------------------
 */
void readSensor(char* id, int pin, char* name, bool invert, uint8_t* value);
bool get_id ( char* id );
void sigendCB(int sigval);
void shutdown_daemon(void);
uint8_t readConfig(void);

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
    
    strcpy(s.ifr_name, mqtt_interface );
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
            syslog(LOG_INFO, "%s: %d", name, new_value);
        }
        if ( ! mqtt_publish( topic, msg ) ) {
            syslog(LOG_ERR, "Error: Did not publish message: %s\n", msg);
        }
    }
}


/*
 * ---------------------------------------------------------------------------------------
 * Get surrent time in milliseconds
 * ---------------------------------------------------------------------------------------
 */
uint64_t current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);                                    // get current time
    uint64_t milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}


/* *********************************************************************************** */
/* read config file                                                                    */
/* *********************************************************************************** */
char *nextValue( char **cursor) {
    while (**cursor && **cursor != ' ') (*cursor)++;                   /*   skip token */
    **cursor = '\0'; (*cursor)++;                                      /* end of token */
    while (**cursor && **cursor == ' ') (*cursor)++;                   /* skip spaces  */
    return *cursor;
}

uint8_t readConfig(void) {
    FILE *fp = NULL;
    fp = fopen(configFile, "rb");
    uint8_t num_sensors=0;
 
    if (fp) {
        char  *line=NULL;
        char  *cursor;
        size_t n=0;
        size_t length = getline(&line, &n, fp);
        
        /* FIXME: look for a lib to read confoig values and replace this hack...       */

        syslog(LOG_INFO, "Reading configuration from %s", configFile);
        
        while ( length != -1) {
            if ( length > 1 ) {                              /* skip empty lines       */
                cursor = line;
                if ( line[length-1] == '\n' ) {             /* remove trailing newline */
                    line[length-1] = '\0';
                }
                
                if ( *cursor != '#') {                          /* skip '#' comments   */
                    char *token = cursor;
                    char *value = nextValue(&cursor);

                    if (!strcmp(token, "MQTT_BROKER_IP")) {
                        mqtt_broker_ip = strdup(value);
                    } else if (!strcmp(token, "MQTT_BROKER_PORT")) {
                        mqtt_broker_port = atoi(value);
                    } else if (!strcmp(token, "PREFIX")) {
                        prefix = strdup(value);
                    } else if (!strcmp(token, "MQTT_INTERFACE")) {
                        mqtt_interface = strdup(value);
                    } else if (!strcmp(token, "MQTT_KEEPALIVE")) {
                        mqtt_keepalive = atoi(value);
                    } else if (!strcmp(token, "DEBUG")) {
                        debug = atoi(value);
                    } else if (!strcmp(token, "REPORT_CYCLE")) {
                        report_cycle = atoi(value) * 10;
                    } else if (!strcmp(token, "PID_FILE")) {
                        pidfile = strdup(value);
                    } else if (!strcmp(token, "SENSOR")) {
                        // Read: Pin Type Invert Frequency Label
                        sensor_list[num_sensors].pin    = atoi(cursor);
                        char *s_type                    = nextValue(&cursor); // need special handling
                        sensor_list[num_sensors].invert = atoi(nextValue(&cursor));
                        sensor_list[num_sensors].freq   = atoi(nextValue(&cursor)) * 10;
                        sensor_list[num_sensors].label  = strdup(nextValue(&cursor));

                        // convert type string to enum
                        if (!strcmp(s_type, "DIGITAL")) {
                            sensor_list[num_sensors].type = DIGITAL;
                        } else if (!strcmp(s_type, "DHT11_TMP")) {
                            sensor_list[num_sensors].type = DHT11_TMP;
                        } else if (!strcmp(s_type, "DHT11_HMD")) {
                            sensor_list[num_sensors].type = DHT11_HMD;
                        } else {
                            syslog(LOG_WARNING, "Warning: Unknown sensor type '%s'. Fall back to DIGITAL", s_type);
                            sensor_list[num_sensors].type = DIGITAL;
                        }
                        
                        // initialize sensor readign with invalid value
                        sensor_list[num_sensors].value     = RESET_VALUE;
                        sensor_list[num_sensors].next_read = (uint64_t)0;
                        
                        if ( debug ) {
                            syslog(LOG_INFO, "%02d: %s sensor '%s' @ pin %d,%sinverted, read every %u uSecs",
                                   num_sensors,
                                   (sensor_list[num_sensors].type == DIGITAL)   ? "Digital"   :
                                   (sensor_list[num_sensors].type == DHT11_TMP) ? "DHT11_TMP" :"DHT11_HMD",
                                   sensor_list[num_sensors].label,
                                   sensor_list[num_sensors].pin,
                                   (sensor_list[num_sensors].invert ? " " : " not "),
                                   sensor_list[num_sensors].freq
                                   );
                        }
                        num_sensors++;
                    }
                }
            }
            free(line);
            n=0;
            length = getline(&line, &n, fp);
        }
        fclose(fp);
    }
    sensor_list[num_sensors].label  = NULL;
    sensor_list[num_sensors].pin    = 0;
    sensor_list[num_sensors].invert = 0;
    return (num_sensors);
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
            debug = 1;
        }
        if (!strcmp(argv[i], "-c")) {       /* '-c' specify configuration file         */
            configFile = strdup(argv[++i]);
        }
    }

    /* ------------------------------------------------------------------------------- */
    /* Read configuration                                                              */
    /* ------------------------------------------------------------------------------- */
    uint8_t num_sensors = readConfig();
    if ( num_sensors==0) {
        syslog(LOG_ERR, "No sensor configuration found in %s", configFile);
        exit(EXIT_FAILURE);
    }
    
    if (debug) {
        syslog(LOG_INFO, "MQTT broker IP: %s",           mqtt_broker_ip);
        syslog(LOG_INFO, "MQTT broker port: %d",         mqtt_broker_port);
        syslog(LOG_INFO, "MQTT interface: %s",           mqtt_interface);
        syslog(LOG_INFO, "MQTT keepalive: %d",           mqtt_keepalive);
        syslog(LOG_INFO, "PREFIX: %s",                   prefix);
        syslog(LOG_INFO, "Full report every %llu seconds", report_cycle);
        syslog(LOG_INFO, "pid/lock file: %s",            pidfile);
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
        syslog(LOG_INFO, "Not demonizing");
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
        syslog(LOG_INFO, "No daemonn, no pid/lock File created");
    }

    /* ------------------------------------------------------------------------------- */
    /* Setup Wiring PI                                                                 */
    /* ------------------------------------------------------------------------------- */
    if(wiringPiSetup()==-1) {
        syslog(LOG_ERR, "Could not setiup wiringPI");
        exit(EXIT_FAILURE);
    } else {
        // Set pins sensors are connected to as input pins
        uint8_t index=0;
        while ( sensor_list[index].label ) {
            pinMode(sensor_list[index].pin, INPUT);
            index++;
        }
    }

    /* ------------------------------------------------------------------------------- */
    /* get MQTT ID basen on MAC address                                                */
    /* ------------------------------------------------------------------------------- */
    if ( !get_id(id) ) {
        syslog(LOG_ERR, "Could not read MAC address of interface %s\n", mqtt_interface );
        exit(EXIT_FAILURE);
    }

    /* ------------------------------------------------------------------------------- */
    /* initialize connection to MQTT server                                            */
    /* ------------------------------------------------------------------------------- */
    if ( !mqtt_init(mqtt_broker_ip, mqtt_broker_port, mqtt_keepalive)) {
        syslog(LOG_ERR, "Unable to connect to MQTT broker at %s:%d",
               mqtt_broker_ip, mqtt_broker_port);
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
    
    uint64_t last_full_report = (uint64_t)0;
    bool     force_reading    = true;
    
    for ( ;; ) {
        uint64_t now       = current_timestamp();
        uint64_t next_time = last_full_report+report_cycle;
        uint8_t  index     = 0;
        
        // time to send a full report?
        if ( next_time <= now ) {
            if (debug>=2) {
                syslog(LOG_INFO, "Send full report");
            }
            force_reading    = true;
            last_full_report = now;
            next_time        = now + report_cycle;
        }
        
        // step through sensors an check if their time is up
        while ( sensor_list[index].label ) {
            if ((sensor_list[index].next_read <= now) || force_reading) {
                // time's up, read sensor value!
                if (force_reading) {
                    sensor_list[index].value = RESET_VALUE;
                }
                readSensor(id, sensor_list[index].pin,
                               sensor_list[index].label,
                               sensor_list[index].invert,
                               &sensor_list[index].value);
                sensor_list[index].next_read = now + (sensor_list[index].freq);

                if (debug>=2) {
                    syslog(LOG_INFO, "Sensor %s next read in %llu usec",
                            sensor_list[index].label,
                            sensor_list[index].next_read-now);
                }
            }
            if (sensor_list[index].next_read < next_time) {
                next_time = sensor_list[index].next_read;
            }
            index++;
        }
        force_reading = false;

        if (debug>=2) {
            syslog(LOG_INFO, "sleep for %lld usec", next_time-now);
        }
        usleep((next_time-now)*1000);
    }
    
    /* ------------------------------------------------------------------------------- */
    /* finish up                                                                       */
    /* ------------------------------------------------------------------------------- */
    shutdown_daemon();
    exit(EXIT_SUCCESS);

    return 0;
}
