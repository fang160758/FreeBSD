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
#include <queue>
#include "freebsd.h"
// #define PORT 23456
// #define SERVER_IP "192.168.40.145"
#define MTU 500

//分片结构
typedef struct ipasfrag
{
    unsigned char ttl;       //分片的生存时间
    unsigned short offest;   //分片在原始数据包中的偏移
    unsigned short data_len; //每一个分片包含的真正数据长度
    char *frag_data;         //每一个分片包含的真正数据指针
    struct ipasfrag *ipf_next;
    struct ipasfrag *ipf_prev;
} ipasFrag;

//分片头结构
typedef struct ipfheader
{
    struct
    {
        int is_have_last_frag;
        unsigned short data_total_len;
        unsigned short data_hava_len;
    } status;
    PIP_HEADER pipHeader; // IP头部信息
    ipasFrag *ipf_head;
    ipasFrag *ipf_tail;
    struct ipfheader *ipfh_next;
    struct ipfheader *ipfh_prev;
} ipfHeader;

typedef struct
{
    ipfHeader *head;
} ipqhead;

ipqhead ipqHead;

//采用头插法向ipqhead链表中插入一个结点
void insertIpfHeader(ipfHeader *ipfH, ipqhead *ipqH)
{
    if ((ipfH->ipfh_next = ipqH->head) != nullptr)
        ipqH->head->ipfh_prev = ipfH;
    ipqH->head = ipfH;
    ipfH->ipfh_prev = nullptr;
}

//采用头插法向ipfHead链表中插入一个结点
void insertIpasFrag(ipasFrag *ipasF, ipfHeader *ipfH)
{
    if ((ipasF->ipf_next = ipfH->ipf_head) != nullptr)
        ipfH->ipf_head->ipf_prev = ipasF;
    ipfH->ipf_head = ipasF;
    ipasF->ipf_prev = nullptr;
    ipfH->status.data_hava_len += ipasF->data_len; //更新当前链表中已经达到分片的所有数据大小
}

//根据偏移值(offest)在ipfheader链表中查找指定的结点
ipasFrag *findIpasFrag(ipfHeader *ipfH, unsigned short offest)
{
    ipasFrag *ipasF = ipfH->ipf_head;
    while (ipasF != nullptr)
    {
        if (ipasF->offest == offest)
            break;
        ipasF = ipasF->ipf_next;
    }
    return ipasF; //返回结点指针
}

//在ipfHeader链表中更新一个结点(offest相同)
void updateIpasFrag(ipasFrag *new_ipasF, ipasFrag *old_ipasF, ipfHeader *ipfH)
{
    if ((new_ipasF->ipf_next = old_ipasF->ipf_next) != nullptr)
        old_ipasF->ipf_next->ipf_prev = new_ipasF;
    else
        ipfH->ipf_tail = new_ipasF;
    if ((new_ipasF->ipf_prev = old_ipasF->ipf_prev) != nullptr)
        old_ipasF->ipf_prev->ipf_next = new_ipasF;
    else
        ipfH->ipf_head = new_ipasF;
    ipfH->status.data_hava_len += (new_ipasF->data_len - old_ipasF->data_len);
    free(old_ipasF); //释放旧结点的内存空间
}

void *recomebinFrag(void *arg);
//从ipqHead链表中分离一个结点
void detachIpfHeader(ipfHeader *ipfH, ipqhead *ipqHead, bool allArrive = false)
{
    if (ipfH->ipfh_prev == nullptr)
        ipqHead->head = nullptr;
    else
        ipfH->ipfh_prev->ipfh_next = nullptr; //从ipqhead结构中分离指定的ipfheader
    if (allArrive)                            //如果是因为属于该头部的所有分片到达
    {
        pthread_t th;
        pthread_create(&th, NULL, recomebinFrag, ipfH); //启动一个线程执行分片重组
        pthread_join(th, NULL);
    }
    else //如果和因为分片信息出错或是超时，则分离后需要释放所有的分片空间
    {
        ipasFrag *ipasF = ipfH->ipf_head;
        free(ipfH->pipHeader);
        free(ipfH);
        while (ipasF != nullptr)
        {
            ipasFrag *tmp = ipasF;
            ipasF = ipasF->ipf_next;
            free(tmp->frag_data);
            free(tmp);
        }
    }
}

typedef struct frag_node
{
    char *data;
    struct frag_node *next;
    struct frag_node *prev;
} frag_node;

typedef struct
{
    frag_node *head = nullptr;
    frag_node *tail = nullptr;
    // pthread_mutex_t head_mutex;
    // pthread_mutex_t tail_mutex;
} frag_head;
#define N 5
frag_head fragHead[N]; //定义N个分片就绪队列

//采用尾插法把分片随机插入分片就绪队列
void insertFragNode(frag_node *node, frag_head *fragHead)
{
    if ((node->prev = fragHead->tail) != nullptr)
        fragHead->tail->next = node;
    else
        fragHead->head = node;
    node->next = nullptr;
    fragHead->tail = node;
}

//采用头取法，从分片就绪队列取出分片进行重组
frag_node *getFragNode(frag_head *fragHead)
{
    frag_node *fragNode;
    if ((fragNode = fragHead->head) != nullptr)
        if ((fragHead->head = fragHead->head->next) != nullptr)
            fragHead->head->prev = nullptr;
        else
            fragHead->tail = nullptr;
    return fragNode;
}

const char *parseServiceType_getProcedence(unsigned char b)
{
    switch (b >> 5)
    {
    case 7:
        return "NetworkControl";
    case 6:
        return "Internet work Control";
    case 5:
        return "CRITIC/ECP";
    case 4:
        return "Flash Override";
    case 3:
        return "Flash";
    case 2:
        return "Immedite";
    case 1:
        return "Priority";
    case 0:
        return "Routine";
    default:
        return "Unknown";
    }
}

const char *parseServiceType_getTOS(unsigned char b)
{
    b = (b >> 1) & 0x0f;
    switch (b)
    {
    case 0:
        return "Normal service";
    case 1:
        return "Minimize monetary cost";
    case 2:
        return "Maximize reliability";
    case 4:
        return "Maximize throughput";
    case 8:
        return "Minimize delay";
    case 15:
        return "Maximize security";
    default:
        return "Unknown";
    }
}

//采用CRC算法计算IP头部的checksum
unsigned short CRC(const char *data)
{
    unsigned int sum = 0;

    for (int i = 0; i < 20; i += 2)
    {
        sum += ntohs(*(short *)(data + i));
        // printf("%04X ", ntohs(*(short *)(data + i)));
    }
    sum += sum >> 16;
    return ~sum;
}

//解析MAC头部信息
void parse_MacHeader(PMAC_HEADER pmacHeader)
{
    printf("**************************MAC HEADER**************************\n");
    printf("源MAC地址：");
    for (int i = 0; i < 6; ++i)
        printf("%02X.", pmacHeader->SrcMaxAddr[i]);
    printf("\t目的MAC地址：");
    for (int i = 0; i < 6; ++i)
        printf("%02X.", pmacHeader->DesMacAddr[i]);
    printf("\t网络类型：%04X\n", ntohs(pmacHeader->Proto));
    return;
}

//解析IP头部信息
void parse_IpHeader(PIP_HEADER pipHeader)
{
    printf("\n**************************IP  HEADER**************************\n");
    printf("头部长度：%d\t", GetIpHeaderHeadLen(pipHeader));
    printf("版本号：%d\t", GetIpHeaderHeadVersion(pipHeader));
    printf("服务类型：%s,%s\t", parseServiceType_getProcedence(GetIpHeaderTos(pipHeader)),
           parseServiceType_getTOS(GetIpHeaderTos(pipHeader)));
    printf("总长度：%04X\n", GetIpHeaderTotalLen(pipHeader));

    printf("标识符：%04X\t", GetIPHeaderId(pipHeader));
    unsigned short offest_flag = ntohs(pipHeader->frag_and_flag);
    printf("标记：DF-%d,MF-%d\t", GetIpHeaderDF(pipHeader), GetIpHeaderMF(pipHeader));
    printf("分片偏移：%04X\n", GetIpHeaderOffest(pipHeader));

    printf("生存时间：%d\t", GetIpHeaderTtl(pipHeader));
    printf("协议：%s\t", GetIpHeaderProtocol(pipHeader));
    printf("头部校验和：%04X\n", GetIpHeaderCheckSum(pipHeader));
    PTCP_HEADER ptcpHeader = (PTCP_HEADER)(pipHeader + pipHeader->hdr_len * 4);
    printf("源IP地址:端口号：%s:%d\n", GetIpHeaderSrcIPAddr(pipHeader), ntohs(ptcpHeader->m_sSourPort));
    printf("目的IP地址:端口号：%s:%d\n", GetIpHeaderDecIPAddr(pipHeader), ntohs(ptcpHeader->m_sDestPort));
    return;
}

//解析数据包
void parse_pack(const char *buf, size_t size)
{

    parse_MacHeader((PMAC_HEADER)buf);
    PIP_HEADER pipHeader = (PIP_HEADER)(buf + sizeof(MAC_HEADER));
    parse_IpHeader(pipHeader);
    // if (size > 1000)
    // {
    // getchar();
    // for (int i = 0; i < size; i++)
    // {
    //     printf("%02X ", (unsigned char)buf[i]);
    //     if ((i + 1) % 20 == 0)
    //         printf("\n");
    // }
    // printf("\n");
    // pthread_t th;
    // pthread_create(&th, NULL, Slice, (void *)pipHeader);
    // pthread_join(th, NULL);
    // }
}

// IP包分片
void *Slice(void *_arg)
{
    char *arg = (char *)_arg;                                           //整个ip数据报
    PIP_HEADER pipHeader = (PIP_HEADER)arg;                             //转换为IP头结构
    int hdr_len = GetIpHeaderHeadLen(pipHeader);                        //获取头结构的头部长度
    unsigned short data_len = GetIpHeaderTotalLen(pipHeader) - hdr_len; //获取数据报中真正数据的长度
    int size = MTU - hdr_len;                                           //每一个分片数据区域的大小
    arg += hdr_len;                                                     //偏移到真正数据的起始地址
    while (data_len > size)
    {
        unsigned short offest = (arg - (char *)pipHeader - hdr_len) / 8; //计算片偏移
        unsigned short cpySize = size / 8 * 8;
        frag_node *node = (frag_node *)malloc(sizeof(frag_node));        //创建node结点
        node->data = (char *)malloc(sizeof(char) * (hdr_len + cpySize)); //申请空间
        memcpy(node->data, (void *)pipHeader, hdr_len);                  //填充头部
        memcpy(node->data + hdr_len, arg, cpySize);                      //填充数据
        PIP_HEADER phdr = (PIP_HEADER)(node->data);
        phdr->frag_and_flag = htons(ntohs((phdr->frag_and_flag) & 0xe000) | (offest & 0x1fff)); //修改偏移
        phdr->frag_and_flag = htons(ntohs(phdr->frag_and_flag) | 0x2000);                       //修改MF=0
        phdr->total_len = htons(hdr_len + cpySize);                                             //修改总长度
        phdr->checksum = 0;                                                                     //校验和=0
        phdr->checksum = htons(CRC((const char *)phdr));                                        //计算IP头部检验和并设置
        // printf("DEBUG\n");
        // getchar();
        // for (int i = 0; i < hdr_len + cpySize; i++)
        // {
        //     printf("%02X ", (unsigned char)node->data[i]);
        //     if ((i + 1) % 20 == 0)
        //         printf("\n");
        // }
        // printf("\n");

        data_len -= cpySize;
        arg += cpySize;
        int ret = rand() % N;
        insertFragNode(node, &fragHead[ret]);
    }
    if (data_len > 0)
    {
        unsigned short offest = (arg - (char *)pipHeader - hdr_len) / 8; //计算片偏移
        unsigned short cpySize = data_len;
        frag_node *node = (frag_node *)malloc(sizeof(frag_node));        //创建node结点
        node->data = (char *)malloc(sizeof(char) * (hdr_len + cpySize)); //申请空间
        memcpy(node->data, (void *)pipHeader, hdr_len);                  //填充头部
        memcpy(node->data + hdr_len, arg, cpySize);                      //填充数据
        PIP_HEADER phdr = (PIP_HEADER)(node->data);
        phdr->frag_and_flag = htons(ntohs(phdr->frag_and_flag) | 0x2000);
        phdr->frag_and_flag = htons(ntohs((phdr->frag_and_flag) & 0xe000) | (offest & 0x1fff));
        phdr->total_len = htons(cpySize + hdr_len);
        // printf("DEBUG\n");
        // getchar();
        // for (int i = 0; i < hdr_len + cpySize; i++)
        // {
        //     printf("%02X ", (unsigned char)node->data[i]);
        //     if ((i + 1) % 20 == 0)
        //         printf("\n");
        // }
        // printf("\n");

        int ret = rand() % N;
        insertFragNode(node, &fragHead[ret]);
    }
}

//根据ID在ipqHead链表中查找指定的结点
ipfHeader *findIpfHeader(ipqhead *ipqHead, unsigned short id)
{
    ipfHeader *ipfH = ipqHead->head;
    while (ipfH != nullptr)
    {
        if (id == GetIPHeaderId(ipfH->pipHeader))
            break;
        ipfH = ipfH->ipfh_next;
    }
    return ipfH;
}

//重组分片
void *recomebinFrag(void *arg)
{
    ipfHeader *ipfH = (ipfHeader *)arg;
    unsigned short hdr_len = GetIpHeaderHeadLen(ipfH->pipHeader);
    char *buf = (char *)malloc(sizeof(char) * (ipfH->status.data_total_len + hdr_len));
    memcpy(buf, (void *)ipfH->pipHeader, hdr_len);
    char *_buf = buf + hdr_len;
    ipasFrag *ipasF = ipfH->ipf_head;
    free(ipfH->pipHeader);
    free(ipfH);
    while (ipasF != nullptr)
    {
        memcpy(_buf + ipasF->offest * 8, ipasF->frag_data, ipasF->data_len);
        ipasFrag *tmp = ipasF;
        ipasF = ipasF->ipf_next;
        free(tmp->frag_data);
        free(tmp);
    }
    ((PIP_HEADER)buf)->frag_and_flag = 0;
    ((PIP_HEADER)buf)->checksum = 0;
    ((PIP_HEADER)buf)->total_len = htons(ipfH->status.data_total_len + hdr_len);
    ((PIP_HEADER)buf)->checksum = htons(CRC(buf));
    parse_IpHeader((PIP_HEADER)buf);
    // do something;
}

void *FreeBSD(void *arg)
{
    while (1)
    {
        // printf("Child process DEBUG!\n");
        int ret = rand() % N;
        frag_node *fragNode = getFragNode(&fragHead[ret]);
        if (fragNode == nullptr)
            continue;
        PIP_HEADER pipHeader = (PIP_HEADER)fragNode->data;
        ipfHeader *ipfH = findIpfHeader(&ipqHead, GetIPHeaderId(pipHeader));
        if (ipfH == nullptr)
        {
            ipfH = (ipfHeader *)malloc(sizeof(ipfHeader));                     //申请ipfHeader空间
            ipfH->pipHeader = (PIP_HEADER)malloc(sizeof(IP_HEADER));           //申请PIP_HEADER空间
            memcpy(ipfH->pipHeader, pipHeader, GetIpHeaderHeadLen(pipHeader)); //填充PIP_HEADER信息
            //初始化指针及数据
            ipfH->ipf_head = ipfH->ipf_tail = nullptr;
            ipfH->status.data_hava_len = ipfH->status.data_total_len = ipfH->status.is_have_last_frag = 0;
            insertIpfHeader(ipfH, &ipqHead); //头插法插入ipqHead
        }
        ipasFrag *curIpasF = (ipasFrag *)malloc(sizeof(ipasFrag)); //申请ipasFrag空间
        curIpasF->offest = GetIpHeaderOffest(pipHeader);           //初始化偏移
        curIpasF->data_len = GetIpHeaderTotalLen(pipHeader) - GetIpHeaderHeadLen(pipHeader);
        curIpasF->ttl = GetIpHeaderTtl(pipHeader);
        memcpy(curIpasF->frag_data, (PIP_HEADER)(fragNode->data + GetIpHeaderTotalLen(pipHeader)), curIpasF->data_len);
        ipasFrag *ipasF = findIpasFrag(ipfH, GetIpHeaderOffest(pipHeader));
        if (ipasF != nullptr)
            updateIpasFrag(curIpasF, ipasF, ipfH);
        else
            insertIpasFrag(curIpasF, ipfH);
        if (CRC((const char *)pipHeader) != 0)
        {
            detachIpfHeader(ipfH, &ipqHead);
            printf("CheackSum is error!\n");
        }

        if (GetIpHeaderMF(pipHeader) == 0 || ipfH->status.is_have_last_frag)
        {
            if (GetIpHeaderMF(pipHeader) == 0)
            {
                ipfH->status.is_have_last_frag = 1;
                ipfH->status.data_total_len = curIpasF->offest * 8 + curIpasF->data_len;
            }
            if (ipfH->status.data_hava_len == ipfH->status.data_total_len)
                detachIpfHeader(ipfH, &ipqHead, true);
        }
    }
}
