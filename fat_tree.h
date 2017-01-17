/*
 * fat_tree.h
 *
 *  Created on: 2017��1��16��
 *      Author: Vincent
 */

#ifndef FAT_TREE_H_
#define FAT_TREE_H_


//�������˲���
#define PortNum 4
#define LevelNum 3
#define ProcessorNum 16
#define SwitchNum 20
#define LinkNum 96
#define SwTop 4
#define SwLower 16
#define SwLowEach 8

//#define VC 4 //virtual channel
//#define BufferDepth 4 //virtual channel buffer depth, ���ڵ���2
#define RouterOverhead 10 //��λΪCLK_CYCLE
#define RouterForwardGap 20 //��λΪCLK_CYCLE

//#define FlitWidth 256
#define ProcessorBufferDepth 10 //processor��buffer�ĳ���
#define RouterBufferDepth 100 //router�����������������

//ʱ����ز���
#define FREQ 1e9  //���ڹ��ķ��棬��λhz
#define CLK_CYCLE 1/FREQ //ʱ������
#define Sim_Start_Time 1 //1s ��ʼ����
#define TimeScale 0.1 //���ģ����ڲ��ɷֲ��������Ʒֲ�����lambda=10����ʾ1s��10��flit���õ���ʱ��������TimeScale��roundȡ��


//Spatial Distribution
#define UNIFORM //�ռ���ȷֲ�

//Time Distribution
#define POISSON_DIST //���ò��ɷֲ�

//�����ƺͲ��ɷֲ���ȡֵ��ΧС��10����λʱ��Ϊ1s����TimeScale����л��㣬������ʱ������

//Poisson�ֲ�����
#define LAMBDA 7 //���ɷֲ������ڲ���ʱ������ָ���ֲ���lambda����ʾ��λʱ����(1s)�����֡�����䵹��Ϊʱ������ƽ��ֵ

//������Ϣ
#define Verbose 1
#define VERBOSE_DEBUG_MESSAGES 1
#define VERBOSE_DETAIL_DEBUG_MESSAGES 2



#endif /* FAT_TREE_H_ */
