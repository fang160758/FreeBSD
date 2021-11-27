/*
 * @Descripttion:
 * @version:
 * @Author: Fang
 * @email: 2192294938@qq.com
 * @Date: 2021-11-27 14:16:04
 * @LastEditors: Fang
 * @LastEditTime: 2021-11-27 22:20:16
 */
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <thread>
#include "freebsd.h"

int main()
{

    // struct sockaddr_in addr;
    // int addr_len = sizeof(addr);
    // bzero(&addr, sizeof(addr));
    // addr.sin_family = AF_INET;
    // addr.sin_port = htons(PORT);
    // inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    int sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd < 0)
    {
        printf("create socket failed！\n");
        exit(0);
    }
    switch (fork())
    {
    case -1:
    {
        printf("fork error!\n");
        exit(0);
    }
    case 0:
    {

        FreeBSD(nullptr);
    }
    default:
    {
        while (1)
        {
            int size;
            char *buf = (char *)malloc(sizeof(char) * 2000);
            bzero(buf, 2000);
            // size = recvfrom(sockfd, buf, 2000 - 1, 0, (struct sockaddr *)&addr, (socklen_t *)&addr_len);

            if ((size = recvfrom(sockfd, buf, 2000 - 1, 0, NULL, NULL)) < 0)
            {
                printf("recvfrom failed!\n");
                exit(0);
            }

            parse_pack((const char *)buf, size);
            pthread_t th;
            pthread_create(&th, nullptr, Slice, buf + 20);

            free(buf);
        }
    }
    }
}