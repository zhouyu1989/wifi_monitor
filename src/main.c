#include <stdio.h>
#include <unistd.h>
#include <wpa_command.h>
#include <string.h>

#include <ams/AmsExport.h>

#define WIFI_AMS_CONNECT "{\"Wifi\":true}"

#define WIFI_AMS_UNCONNECTED "{\"Wifi\":false}"

int main(int argc, char **argv)
{
    int ret = 0;
    int status = 0;
    int status_bak = 0;

    AmsExInit();

    while (1) {

        ret = network_get_status(&status);
        if (ret < 0) {
            printf("wpa connect get status error\n");
        }

        if (status_bak != status) {
            if (status == WIFI_CONNECTED) {
                ReportSysStatus(WIFI_AMS_CONNECT);
            } else if (status == NETSERVER_CONNECTED) {
                ReportSysStatus(WIFI_AMS_UNCONNECTED);
            } else {
                ReportSysStatus(WIFI_AMS_UNCONNECTED);
            }
            
            status_bak = status;
        }

        sleep(1);
    }

    return 0;
}
