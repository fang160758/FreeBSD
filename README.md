# FreeBSD

### TCP网络数据包的组成部分

网络数据包一般包括头部和数据部分，在TCP协议中，要发送的数据经过TCP模块添加添加TCP头部；然后IP模块添加IP头部和MAC头部；然后在最前面加上报头/起始帧分界符以及末尾加上FCS(帧校验序列)，这样就构成了一个完整的数据包。

![11](http://typora-tests.oss-cn-chengdu.aliyuncs.com/img/11.png)

### TCP协议中为什么要进行分片

当一个TCP数据包太大超过转发设备设定的MTU时需要在发送方进行分片，在接收方进行数据包的重组。

**发送方**

将数据包分为多个TCP头部+数据包的组合，TCP头部中存着不同的数据序号；之后将多个组合交由IP模块，统一添加IP头部和MAC头部，IP头部的ID号设为统一的。如果在IP层的数据包大小大于MTU则需要进行分片，值得注意的是，同一个IP数据报的分片只有第一片才会包含TCP头部信息。

**接收方**

IP模块具有分片重组的功能，如果接收到的包是经过分片的，那么IP模块会将它们还原成原始的数据包。

分片的包会在IP头部的标志字段中进行标记，当收到分片的包时，IP模块会将其暂时存在内部的内存空间中，然后等待IP头部具有相同ID的包全部到达，因为同一个包的所有分片都具有相同的ID。此外，IP头部还有一个分片偏移量的字段，他表示当前分片在整个包中所处的位置。根据这些信息，在所有的分片全部收到之后，就可以将它们还原成原始的包。

数据包的分片和重组里边还涉及到了MTU和MSS的概念：

-   **MTU**：Maxitum Transmission Unit 最大传输单元
-   **MSS**：Maxitum Segment Size 最大分段大小，MSS就是TCP数据包每次能够传输的最大数据分段。

![112](http://typora-tests.oss-cn-chengdu.aliyuncs.com/img/112.png)

### 网络封装中各头部信息

![数据报](http://typora-tests.oss-cn-chengdu.aliyuncs.com/img/数据报.png)

### 我的FreeBSD算法流程

![freebsd](http://typora-tests.oss-cn-chengdu.aliyuncs.com/img/freebsd.png)
