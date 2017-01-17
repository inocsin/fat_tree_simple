/*
 * processor.cc
 *
 *  Created on: 2017��1��16��
 *      Author: Vincent
 *
 *  ���ܣ�
 *  1. ���ͺͽ���packet��һ��packet��Ӧһ��flit
 *  2. ����������ģ�ͣ�ÿ�����Ӧʱ��ֱ�ӷ�������
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
    cMessage *selfMsgGenMsg; //����flit��ʱ��package���������ɷֲ�����ȷֲ�
    cMessage *selfMsgSendMsg; //����flit��ʱ��ÿ���ڶ����buffer���ٷ���

    long numFlitSent;
    long numFlitReceived;
    long numDropped;
    long flitByHop; //���ڼ�����·������

    cOutVector hopCountVector;
    cOutVector flitDelayTime;

    FatTreeMsg* OutBuffer[ProcessorBufferDepth]; //���ڴ��flit,���flit�Ļ���

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
    virtual int getNextRouterPortP(); //������processor������router�Ķ˿�
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

    selfMsgSendMsg = new cMessage("selfMsgSendMsg");//ע��˳���ȷ���buffer�����msg���ٲ����µ�msg������һ��flit��Ҫ2�����ڲŻᷢ��ȥ
    scheduleAt(Sim_Start_Time, selfMsgSendMsg);
    selfMsgGenMsg = new cMessage("selfMsgGenMsg");
    scheduleAt(Sim_Start_Time, selfMsgGenMsg);

    for(int i = 0; i < ProcessorBufferDepth; i++)
        OutBuffer[i] = nullptr;


}

void Processor::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        //********************���������ݵ��Զ�ʱ��Ϣ********************
        if(msg == selfMsgSendMsg){
            //****************************ת��flit**************************
            if(OutBuffer[0] != nullptr){ //��һ���ڵ���buffer���ܴ�flit
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

            if(getIndex() == 0){ //processor����msg��ģʽ,��Ҫ�Ľ�
            //if (true) {

                //**********************����flit*****************************
                for(int i = 0;i<ProcessorBufferDepth;i++){
                    if(OutBuffer[i] == nullptr){
                        FatTreeMsg *newmsg = generateMessage();
                        OutBuffer[i] = newmsg;
                        break;
                    }
                }

                //**********************������ʱ��Ϣ*****************************
                //package֮���ʱ����Ϊ���ɷֲ��������Ʒֲ���ͬһ��package��flit֮����ΪCLK_CYCLE


#if defined POISSON_DIST //���ɷֲ�
                double expTime = Poisson();

                if (Verbose >= VERBOSE_DETAIL_DEBUG_MESSAGES) {
                    EV << "Poisson interval: "<<expTime<<"\n";
                }

                scheduleAt(simTime()+expTime,selfMsgGenMsg);
#else //���ȷֲ�

                scheduleAt(simTime()+CLK_CYCLE,selfMsgGenMsg);
#endif



            }

        }


    }else{
        //************************��self message*********************

        //***********************�յ�FatTreeMsg��Ϣ*******************
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
        flitByHop += hopcount + 1; //�������һ��·������processor

        flitDelayTime.record(simTime().dbl() - ftmsg->getFlitGenTime());
        hopCountVector.record(hopcount);

        delete ftmsg;


    }
}

FatTreeMsg* Processor::generateMessage()
{


    // Produce source and destination address
    int current_ppid = getIndex();
    int n = getVectorSize();//processor������
    //EV<<n<<"\n";
#ifdef UNIFORM //���ȷֲ�
    int dst_ppid = intuniform(0, n-2); //��������ģ��
    //EV<<dst_ppid<<"\n";
    if (dst_ppid >= current_ppid)
        dst_ppid++;//��֤��ȡ��current_ppid
#endif

    int current_plid = ppid2plid(current_ppid);
    int dst_plid = ppid2plid(dst_ppid);

    char msgname[200];//��ʼ����Ŀռ�̫С�������ݱ��ı�!!!!!!!
    sprintf(msgname, "Head Flit, From processor node %d(%d) to node %d(%d)", current_ppid,current_plid,dst_ppid,dst_plid);

    // Create message object and set source and destination field.
    FatTreeMsg *msg = new FatTreeMsg(msgname);
    msg->setSrc_ppid(current_ppid);//���÷�����processor���
    msg->setDst_ppid(dst_ppid);//���ý��յ�processor���
    msg->setFrom_router_port(getNextRouterPortP());//�����յ���msg��Router�˿�
    msg->setFlitGenTime(simTime().dbl());

    return msg;

}

//processorת����·���㷨,processorֻ��һ��port,ֱ��ת����ȥ����
void Processor::forwardMessage(FatTreeMsg *msg)
{

    msg->setFrom_router_port(getNextRouterPortP());// �����յ�����Ϣ·�����Ķ˿ں�
    send(msg,"port$o");
    if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
        EV << "Forwarding message { " << msg << " } from processor to router\n";
    }

}

int Processor::getNextRouterPortP(){
    int current_ppid=getIndex();
    int plid=ppid2plid(current_ppid);
    int port=plid%10;
    return port; //���غ�processor������router�Ķ˿�
}


//��ppid����plid
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
//��plid����ppid
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







