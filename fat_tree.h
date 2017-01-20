/*
 * fat_tree.h
 *
 *  Created on: 2017年1月16日
 *      Author: Vincent
 */

#ifndef FAT_TREE_H_
#define FAT_TREE_H_


//网络拓扑参数
#define PortNum 16
#define LevelNum 3
#define ProcessorNum 1024
#define SwitchNum 320
//#define LinkNum 6144
#define SwTop 64
#define SwLower 256
#define SwLowEach 128

//#define VC 4 //virtual channel
//#define BufferDepth 4 //virtual channel buffer depth, 大于等于2
#define RouterOverhead 12 //单位为CLK_CYCLE, 数据从输入端口缓存到输出端口缓存
#define RouterForwardGap 2 //单位为CLK_CYCLE， 数据从路由器向外发送时间间隔，带宽
#define ProcessorForwardGap 1//单位为CLK_CYCLE，processor向外发送数据间隔，注意与数据产生间隔相区分，数据产生后先放入buffer中，发送数据是从buffer中读取

//#define FlitWidth 256
#define ProcessorBufferDepth 10 //processor中buffer的长度
#define RouterBufferDepth 50 //router的输入和输出缓存深度

//时钟相关参数
#define FREQ 312.5e6  //时钟周期，单位hz
#define CLK_CYCLE 1/FREQ //时钟周期
#define Sim_Start_Time 1 //1s 开始仿真
#define TimeScale 0.1 //不改，用于泊松分布和自相似分布，如lambda=10，表示1s内10个flit，得到的时间间隔除以TimeScale再round取整


//Spatial Distribution
#define UNIFORM //空间均匀分布

//Time Distribution
#define POISSON_DIST //采用泊松分布

//自相似和泊松分布的取值范围小于10，单位时间为1s，但TimeScale会进行换算，最后乘以时钟周期

//Poisson分布参数
#define LAMBDA 7 //泊松分布中用于产生时间间隔的指数分布的lambda，表示单位时间内(1s)到达的帧数，其倒数为时间间隔的平均值, 这里lambda表示注入率(0,10]
#define UniformInjectionRate 0.3 //用于时间均匀分布的注入率，范围(0,1]

//调试信息
#define Verbose 1
#define VERBOSE_DEBUG_MESSAGES 1
#define VERBOSE_DETAIL_DEBUG_MESSAGES 2



#endif /* FAT_TREE_H_ */
