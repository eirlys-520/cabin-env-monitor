#ifndef HEAD_H
#define HEAD_H

#include <MQTTAsync.h>
#include <MQTTClient.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <string.h>
//#define OLD_ADDRESS     "tcp://218.201.45.7:1883"
#define NEW_ADDRESS "tcp://183.230.40.96:1883"
#define DEV_NAME "env"  //"device_name"  //"first_device"
#define CLIENTID DEV_NAME
#define PRODUCT_ID "RmHW0FCi87"  //"63fg7NqRxY"  //"3XoV8495r6"
#define PASSWD                                                                                      \
  "version=2018-10-31&res=products%2FRmHW0FCi87%2Fdevices%2Fenv&et=19372555238888&method=md5&sign=" \
  "L8a5FhcRNNvVzmw1hqvF1A%3D%3D"
//"version=2018-10-31&res=products%2F63fg7NqRxY%2Fdevices%2Fdevice_name&et=3937255523&method=md5&sign=oCu6NsiOgu2LWSV8HnjYUA%3D%3D"
//"version=2018-10-31&res=products%2F3XoV8495r6%2Fdevices%2Ffirst_device&et=1837255523&method=sha1&sign=dtVQufcgwCrn5zmdmWLXFEtJAQY%3D";
#define QOS 0
#define TIMEOUT 10000L

extern int mqtt_init();
extern int mqtt_send(char* key, float value);
extern void mqtt_deinit();
extern int mqtt_send_multi(const char * params_json);


//#define __cplusplus
#endif  // HEAD_H
