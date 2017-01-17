/*
 * processor.cc
 *
 *  Created on: 2017年1月16日
 *      Author: Vincent
 *
 *  功能：
 *  1. 发送和接收packet，一个packet对应一个flit
 *  2. 不采用流控模型，每间隔相应时间直接发送数据
 *
 */


#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include "fat_tree_m.h"
#include "fat_tree.h"

using namespace omnetpp;


// Processor
class Processor : public cSimpleModule
{
  private:
    cMessage *selfMsgGenMsg; //产生flit定时，package产生按泊松分布或均匀分布
    cMessage *selfMsgSendMsg; //发送flit定时，每周期都检查buffer，再发送

    long numFlitSent;
    long numFlitReceived;
    long numDropped;
    long flitByHop; //用于计算链路利用率

    cOutVector hopCountVector;
    cOutVector flitDelayTime;

    FatTreeMsg* OutBuffer[ProcessorBufferDepth]; //用于存放flit,输出flit的缓存

  public:
    Processor();
    virtual ~Processor();
  protected:
    virtual FatTreeMsg *generateMessage();
    virtual void forwardMessage(FatTreeMsg *msg);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual int ppid2plid(int ppid);
    virtual int plid2ppid(int plid);
    virtual int getNextRouterPortP(); //计算与processor相连的router的端口
    virtual double Poisson();


    // The finish() function is called by OMNeT++ at the end of the simulation:
    virtual void finish() override;
};

Define_Module(Processor);

Processor::Processor(){
    selfMsgGenMsg=nullptr;
    selfMsgSendMsg=nullptr;
}


Processor::~Processor(){
    cancelAndDelete(selfMsgGenMsg);
    cancelAndDelete(selfMsgSendMsg);
}

void Processor::initialize()
{
    // Initialize variables
    numFlitSent = 0;
    numFlitReceived = 0;
    numDropped = 0;
    flitByHop = 0;

    hopCountVector.setName("HopCount");
    flitDelayTime.setName("flitDelayTime");

    selfMsgSendMsg = new cMessage("selfMsgSendMsg");//注意顺序，先发送buffer里面的msg，再产生新的msg，这样一个flit需要2个周期才会发出去
    scheduleAt(Sim_Start_Time, selfMsgSendMsg);
    selfMsgGenMsg = new cMessage("selfMsgGenMsg");
    scheduleAt(Sim_Start_Time, selfMsgGenMsg);

    for(int i = 0; i < ProcessorBufferDepth; i++)
        OutBuffer[i] = nullptr;


}

void Processor::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        //********************发送新数据的自定时消息********************
        if(msg == selfMsgSendMsg){
            //****************************转发flit**************************
            if(OutBuffer[0] != nullptr){ //下一个节点有buffer接受此flit
                FatTreeMsg* current_forward_msg = OutBuffer[0];
                forwardMessage(current_forward_msg);

                numFlitSent++;

                for(int i=0; i < ProcessorBufferDepth - 1; i++){
                    OutBuffer[i] = OutBuffer[i+1];
                }

                OutBuffer[ProcessorBufferDepth-1] = nullptr;
            }

            scheduleAt(simTime()+CLK_CYCLE,selfMsgSendMsg);

        }else if(msg == selfMsgGenMsg){

            if(getIndex() == 0){ //processor产生msg的模式,需要改进
            //if (true) {

                //**********************产生flit*****************************
                for(int i = 0;i<ProcessorBufferDepth;i++){
                    if(OutBuffer[i] == nullptr){
                        FatTreeMsg *newmsg = generateMessage();
                        OutBuffer[i] = newmsg;
                        break;
                    }
                }

                //**********************产生定时消息*****************************
                //package之间的时间间隔为泊松分布或自相似分布，同一个package的flit之间间隔为CLK_CYCLE


#if defined POISSON_DIST //泊松分布
                double expTime = Poisson();

                if (Verbose >= VERBOSE_DETAIL_DEBUG_MESSAGES) {
                    EV << "Poisson interval: "<<expTime<<"\n";
                }

                scheduleAt(simTime()+expTime,selfMsgGenMsg);
#else //均匀分布

                scheduleAt(simTime()+CLK_CYCLE,selfMsgGenMsg);
#endif



            }

        }


    }else{
        //************************非self message*********************

        //***********************收到FatTreeMsg消息*******************
        FatTreeMsg *ftmsg = check_and_cast<FatTreeMsg *>(msg);

        // Message arrived
        int current_ppid=getIndex();
        int hopcount = ftmsg->getHopCount();
        if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
            EV << ">>>>>>>>>>Message {" << ftmsg << " } arrived after " << hopcount <<
                    " hops at node "<<current_ppid<<"("<<ppid2plid(current_ppid)<<")<<<<<<<<<<\n";
        }

        // update statistics.
        numFlitReceived++;
        flitByHop += hopcount + 1; //包含最后一跳路由器到processor

        flitDelayTime.record(simTime().dbl() - ftmsg->getFlitGenTime());
        hopCountVector.record(hopcount);

        delete ftmsg;


    }
}

FatTreeMsg* Processor::generateMessage()
{


    // Produce source and destination address
    int current_ppid = getIndex();
    int n = getVectorSize();//processor的数量
    //EV<<n<<"\n";
#ifdef UNIFORM //均匀分布
    int dst_ppid = intuniform(0, n-2); //均匀流量模型
    //EV<<dst_ppid<<"\n";
    if (dst_ppid >= current_ppid)
        dst_ppid++;//保证不取到current_ppid
#endif

    int current_plid = ppid2plid(current_ppid);
    int dst_plid = ppid2plid(dst_ppid);

    char msgname[200];//初始分配的空间太小导致数据被改变!!!!!!!
    sprintf(msgname, "Head Flit, From processor node %d(%d) to node %d(%d)", current_ppid,current_plid,dst_ppid,dst_plid);

    // Create message object and set source and destination field.
    FatTreeMsg *msg = new FatTreeMsg(msgname);
    msg->setSrc_ppid(current_ppid);//设置发出的processor编号
    msg->setDst_ppid(dst_ppid);//设置接收的processor编号
    msg->setFrom_router_port(getNextRouterPortP());//设置收到该msg的Router端口
    msg->setFlitGenTime(simTime().dbl());

    return msg;

}

//processor转发的路由算法,processor只有一个port,直接转发出去即可
void Processor::forwardMessage(FatTreeMsg *msg)
{

    msg->setFrom_router_port(getNextRouterPortP());// 设置收到该信息路由器的端口号
    send(msg,"port$o");
    if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
        EV << "Forwarding message { " << msg << " } from processor to router\n";
    }

}

int Processor::getNextRouterPortP(){
    int current_ppid=getIndex();
    int plid=ppid2plid(current_ppid);
    int port=plid%10;
    return port; //返回和processor相连的router的端口
}


//从ppid计算plid
int Processor::ppid2plid(int ppid){
    int idtmp=ppid;
    int idfinal=0;
    int mul=1;
    for(int i=0;i<LevelNum-1;i++){
        idfinal=idfinal+idtmp%(PortNum/2)*mul;
        mul=mul*10;
        idtmp=(int)(idtmp/(PortNum/2));
    }
    idfinal=idfinal+idtmp*mul;
    return idfinal;
}
//从plid计算ppid
int Processor::plid2ppid(int plid){
    int tmp=plid;
    int mul=1;
    int IDtmp=0;
    for(int i=0;i<LevelNum;i++){
        IDtmp=IDtmp+mul*(tmp%10);
        mul=mul*(PortNum/2);
        tmp=tmp/10;
    }
    return IDtmp;
}



double Processor::Poisson() {
    double exp_time = exponential((double)1/LAMBDA);
    exp_time = round(exp_time / TimeScale) * CLK_CYCLE;
    if(exp_time < CLK_CYCLE) {
        exp_time = CLK_CYCLE;
    }
    return exp_time;
}

void Processor::finish()
{
    // This function is called by OMNeT++ at the end of the simulation.
    EV << "Flit Sent: " << numFlitSent << endl;
    EV << "Flit Received: " << numFlitReceived << endl;
    EV << "Dropped:  " << numDropped << endl;


    recordScalar("#flit sent", numFlitSent);
    recordScalar("#flit received", numFlitReceived);
    recordScalar("#flit dropped", numDropped);
    recordScalar("#flitByHop", flitByHop);


    if(getIndex() == 0) {
        double timeCount = simTime().dbl() - Sim_Start_Time;
        recordScalar("#timeCount", timeCount);
    }

}







