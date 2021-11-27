/*
 * @Descripttion:
 * @version:
 * @Author: Fang
 * @email: 2192294938@qq.com
 * @Date: 2021-11-27 10:50:40
 * @LastEditors: Fang
 * @LastEditTime: 2021-11-27 21:56:09
 */

#ifndef __FREEBSD_H__
#define __FREEBSD_H__

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// MAC header
typedef struct
{
    unsigned char DesMacAddr[6]; // 6字节目的MAC地址
    unsigned char SrcMaxAddr[6]; // 6字节源MAC地址
    short Proto;                 // 2字节的网络类型
} __attribute__((packed)) MAC_HEADER, *PMAC_HEADER;

// IP header
typedef struct
{
    unsigned char hdr_len : 4;    //首部长度
    unsigned char version : 4;    //版本号
    unsigned char tos;            //服务类型
    unsigned short total_len;     //数据报总长度
    unsigned short identifier;    //分组ID
    unsigned short frag_and_flag; //标记和偏移量
    unsigned char ttl;            //生存时间
    unsigned char protocol;       //高层协议
    unsigned short checksum;      //首部校验和
    unsigned int source_ip;       //源IP地址
    unsigned int dest_ip;         //目的IP地址
} __attribute__((packed)) IP_HEADER, *PIP_HEADER;

// TCP header
typedef struct _TCP_HEADER
{
    unsigned short m_sSourPort;      //源端口
    unsigned short m_sDestPort;      //目的端口
    unsigned int m_uiSequNum;        // TCP序号
    unsigned int m_uiAcknowledgeNum; //稍待的确认
    unsigned char m_uiHeadOff;       //首部长度-保留字段-flag
    short m_sWindowSize;             //窗口尺寸
    short m_sCheckSum;               // TCP校验和
    short m_surgentPointer;          //数据报内容
} __attribute__((packed)) TCP_HEADER, *PTCP_HEADER;

static unsigned char GetIpHeaderHeadVersion(PIP_HEADER pipHeader) { return pipHeader->version; }
static unsigned char GetIpHeaderHeadLen(PIP_HEADER pipHeader) { return pipHeader->hdr_len * 4; }
static unsigned char GetIpHeaderTos(PIP_HEADER pipHeader) { return pipHeader->tos; }
static unsigned short GetIpHeaderTotalLen(PIP_HEADER pipHeader) { return ntohs(pipHeader->total_len); }
static unsigned short GetIPHeaderId(PIP_HEADER pipHeader) { return ntohs(pipHeader->identifier); }
static unsigned char GetIpHeaderDF(PIP_HEADER pipHeader) { return (ntohs(pipHeader->frag_and_flag) >> 14) & 0x1; }
static unsigned char GetIpHeaderMF(PIP_HEADER pipHeader) { return (ntohs(pipHeader->frag_and_flag) >> 13) & 0x1; }
static unsigned short GetIpHeaderOffest(PIP_HEADER pipHeader) { return (ntohs(pipHeader->frag_and_flag) & 0x1fff); }
static unsigned char GetIpHeaderTtl(PIP_HEADER pipHeader) { return pipHeader->ttl; }
static const char *GetIpHeaderProtocol(PIP_HEADER pipHeader)
{
    switch (pipHeader->protocol)
    {
#define DEF_XX(type, str) \
    case (type):          \
        return #str
        DEF_XX(IPPROTO_TCP, TCP);
        DEF_XX(IPPROTO_UDP, UDP);
        DEF_XX(IPPROTO_IGMP, IGMP);
        DEF_XX(IPPROTO_ICMP, ICMP);
        DEF_XX(IPPROTO_IPV6, IPV6);
    default:
        return "UNKNOW";
#undef DEF_XX
    }
    return "UNKNOW";
}
static unsigned short GetIpHeaderCheckSum(PIP_HEADER pipHeader) { return ntohs(pipHeader->checksum); }
static const char *GetIpHeaderSrcIPAddr(PIP_HEADER pipHeader)
{
    in_addr src_addr;
    memcpy(&src_addr, &pipHeader->source_ip, 4);
    return inet_ntoa(src_addr);
}
static const char *GetIpHeaderDecIPAddr(PIP_HEADER pipHeader)
{
    in_addr dec_addr;
    memcpy(&dec_addr, &pipHeader->dest_ip, 4);
    return inet_ntoa(dec_addr);
}

void parse_pack(const char *buf, size_t size);
void parse_MacHeader(PMAC_HEADER pmacHeader);
void parse_IpHeader(PIP_HEADER pipHeader);
unsigned short CRC(const char *data);
void *Slice(void *_arg);
void *FreeBSD(void *arg);
#endif