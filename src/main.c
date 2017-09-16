#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include <wpa_command.h>
#include <ams/AmsExport.h>

#define WIFI_AMS_CONNECT "{\"Wifi\":true}"

#define WIFI_AMS_UNCONNECTED "{\"Wifi\":false}"

#define WIFI_FAILED_TIMES 3


static int is_report_unconnect = 0;
static int is_report_connect = 0;
int buflen = 2047;
char monitor_buf[2048] = {0};


int judge_report_connect()
{
    int netpingtime = 0;
    int ret = 0;
    int status = 0;

    printf("begin to  ping server...\n");
    while (netpingtime++ < 3) {
        ret = network_get_status(&status);
        if (ret < 0) {
            printf("wpa connect get status error\n");
        }
        printf("current net status:: %d\n", status);

        if ((status == NETSERVER_CONNECTED) && (is_report_connect == 0)) {
            printf("wifi ams report connect\n");
            ReportSysStatus(WIFI_AMS_CONNECT);
            is_report_unconnect = 0;
            is_report_connect = 1;
            return status;
        } else if (status == NETSERVER_UNCONNECTED) {
            sleep(3);
        } else {
            return status;
        }
    }
    return status;
}

void judge_report_unconnect(){
    printf("wifi ams report unconnect\n");
    ReportSysStatus(WIFI_AMS_UNCONNECTED);
    is_report_unconnect = 1;
    is_report_connect = 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    // int buflen = 127;
    // char monitor_buf[128] = {0};
    int hardsharkfaild = 0;
    int nofoundtime = 0;
    int network_num = 0;

    AmsExInit();

    sleep(2);
    ret =  wifi_get_listnetwork(&network_num);
    if ((network_num == 0) && (is_report_unconnect == 0)) {
        judge_report_unconnect();
    } else {
        judge_report_connect();
    }

    ret = wifi_connect_moni_socket(WIFI_WPA_CTRL_PATH);
    if (ret < 0) {
        printf("monitor socket create error %d\n", ret);
        return -1;
    }

    while (1) {
        buflen = 2047;

        memset(monitor_buf, 0, sizeof(monitor_buf));
        ret = wifi_ctrl_recv(monitor_buf, &buflen);

        if (ret == 0) {
            printf("ret %d buf: %s len  %d\n", ret, monitor_buf, buflen);

            if (strstr(monitor_buf, "Handshake failed")) {
                hardsharkfaild++;
            } else if (strstr (monitor_buf, "CTRL-EVENT-CONNECTED")) {
                hardsharkfaild = 0;
                nofoundtime = 0;
            } else if (strstr(monitor_buf, "CTRL-EVENT-NETWORK-NOT-FOUND")) {
                nofoundtime++;
            } else {
                continue;
            }

            if (((hardsharkfaild == WIFI_FAILED_TIMES) || (nofoundtime == WIFI_FAILED_TIMES)) && (is_report_unconnect == 0)) {
                judge_report_unconnect();
            } else if (((hardsharkfaild == 0) && (nofoundtime == 0)) && (is_report_connect == 0)) {
                judge_report_connect();
            }
        } else if (ret == 1) {
            //  timeout  need to check ip can work
            if (judge_report_connect() != NETSERVER_CONNECTED) {
                if (is_report_unconnect == 0) {
                    judge_report_unconnect();
                }
            } else {
                if (is_report_connect == 0) {
                    judge_report_connect();
                }
            }

        } else {
            printf("recv error :: %d\n", errno);
            wifi_monitor_release();
            ret = wifi_connect_moni_socket(WIFI_WPA_CTRL_PATH);
            if (ret < 0) {
                printf("monitor socket create error %d\n", errno);
                return -1;
            }
        }
    }

    return 0;
}
