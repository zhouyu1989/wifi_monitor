#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/route.h>
#include <sys/un.h>

#include <wpa_command.h>
#include <ams/AmsExport.h>

#define WIFI_AMS_CONNECT "{\"Wifi\":true}"

#define WIFI_AMS_UNCONNECTED "{\"Wifi\":false}"

#define WIFI_EVENT_PATH "/tmp/wifi_monitor_event"

#define WIFI_FAILED_TIMES 3
#define WIFI_ROAM_LIMIT_SIGNAL -65
#define WIFI_SWITCH_LIMIT_SIGNAL -80


static int is_report_unconnect = 0;
static int is_report_connect = 0;
int buflen = 2047;
char monitor_buf[2048] = {0};

static int sock_event_fd = 0;

static int channel[26] = {2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5745, 5765, 5785, 5805, 5825};

static int scan_index[6] = {4, 4, 5, 4, 4, 5};

int judge_report_connect()
{
    int netpingtime = 0;
    int ret = 0;
    int status = 0;

    while (netpingtime++ < 3) {
        ret = network_get_status(&status);
        if (ret < 0) {
            printf("wpa connect get status error\n");
        }
        printf("current net status:: %d\n", status);

        if ((status == NETSERVER_CONNECTED) && (is_report_connect == 0)) {
            printf("wifi ams report connect\n");
            if (ReportSysStatus(WIFI_AMS_CONNECT) != 0) {
                continue;
            }
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
    if (ReportSysStatus(WIFI_AMS_UNCONNECTED) != 0) {
        return;
    }

    is_report_unconnect = 1;
    is_report_connect = 0;
}

int wifi_get_monitor_event() {
    int ret = 0;
    // int buflen = 127;
    // char monitor_buf[128] = {0};
    int hardsharkfaild = 0;
    int nofoundtime = 0;
    int network_num = 0;

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
            } else if ((strstr(monitor_buf, "CTRL-EVENT-DISCONNECTED") != NULL) && (strstr(monitor_buf, "reason=3 locally_generated=1") != NULL)) {
                judge_report_unconnect();
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

static int find_index(int val, int *channel, int len)
{
    int i = 0;

    for (i = 0; i < len; i++) {
        if (channel[i] == val) {
            return i;
        }
    }

    return -1;
}

static int find_flag(int index, int *scan_index, int len)
{
    int i = 0;
    int sum = 0;

    for (i = 0; i < len; i++) {
        if (scan_index[i] + sum > index) {
            return i;
        }
        sum += scan_index[i];
    }
    return -1;
}

static int calc_scan_sum(int index)
{
    int i = 0;
    int sum = 0;

    for (i = 0; i < index; i++) {
        sum += scan_index[i];
    }
    return sum;
}

int wifi_roam_scan_event() {
    int ret = 0;
    int signal = 0;
    int first_time = 0;
    int index = 0;
    int current_freq = 0;
    int freq[6] = {0};
    static unsigned long time_sum = 0;
    int time = 1;
    char testbuf[128] = {0};
    int size = 0;

    while (1) {
        ret = wifi_get_signal(&signal);
        if (ret == 0) {
            printf("wifi current signal %d\n", signal);
            sprintf(testbuf, "wifi current signal :: %d", signal);

            size = send(sock_event_fd, testbuf, 128, 0);
            printf("zpershuai :: send signal size %d\n", size);

            if (signal < WIFI_SWITCH_LIMIT_SIGNAL) {

            } else {
                if (signal < WIFI_ROAM_LIMIT_SIGNAL) {
                    if (first_time == 0) {
                        ret = wifi_get_current_channel(&current_freq);
                        if (ret < 0) {
                            continue;
                        }

                        index = find_index(current_freq, channel, sizeof(channel) / sizeof(int));
                        index = find_flag(index, scan_index, sizeof(scan_index) / sizeof(int));

                        first_time = 1;
                        printf("rbscan index %d\n", index);
                    }
                    else {
                        index++;

                        printf("rbscan index %d\n", index);
                        if (index == sizeof(scan_index) / sizeof(int)) {
                            index = 0;
                        }
                    }
                    memcpy(freq, &channel[calc_scan_sum(index)], scan_index[index] * sizeof(int));

                    wifi_scan_channel(scan_index[index], freq);

                    time_sum++;

                } else {
                    first_time = 0;
                    time = 1;
                    time_sum = 0;
                }

                printf( "robot scan time  %lu", time_sum);
                if ((time_sum > 3 * sizeof(scan_index) / sizeof(int)) &&
                    (time_sum < 5 * sizeof(scan_index) / sizeof(int))) {
                    time = 60;
                }
                else if (time_sum >= 5 * sizeof(scan_index) / sizeof(int)) {
                    time = 60 * 3;
                }
                else {
                    time = 1;
                }

                sleep(time);
            }
        }
    }

    return 0;
}

static int wifi_sock_init() {
    int sock_fd = 0;
    unsigned int len = 0;
    struct sockaddr_un addr;

    memset(&addr, 0, sizeof(addr));

    sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        printf("wifi event client create error %d\n", errno);
        return -1;
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WIFI_EVENT_PATH, strlen(WIFI_EVENT_PATH));
    len = strlen(addr.sun_path) + sizeof(addr.sun_family);

    // if (bind(sock_fd, (struct sockaddr *)&addr, len) < 0)  {
    //     perror("bind error");
    //     close(sock_fd);
    //     return -2;
    // }

    if (connect(sock_fd, (struct sockaddr *)&addr, len) < 0)  {
        perror("connect error");
        close(sock_fd);
        return -3;
    }

    return sock_fd;
}

int main(int argc, char **argv)
{
    while (1) {
        if ((access(WIFI_WPA_CTRL_PATH, F_OK)) != -1) {
            break;
        } else {
            sleep(1);
            continue;
        }
    }
    printf("wpa_supplicant start ok\n");

    while (1) {
        if (AmsExInit() == 0) {
            break;
        } else {
            sleep(1);
            continue;
        }
    }
    printf("ams start ok\n");

    sock_event_fd = wifi_sock_init();
    if (sock_event_fd < 0) {
        perror("wifi socket init error");
        return -1;
    }

    if (fork() == 0) {
        wifi_get_monitor_event();
    } else {
        wifi_roam_scan_event();
    }


    return 0;
}
