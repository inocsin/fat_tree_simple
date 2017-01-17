/*
 * router.cc
 *
 *  Created on: 2017��1��16��
 *      Author: Vincent
 */

#include <stdio.h>
#include <string.h>
#include <omnetpp.h>
#include <math.h>
#include "fat_tree_m.h"
#include "fat_tree.h"

using namespace omnetpp;


/*************************************************
 * ʵ�ֹ��ܣ�
 *
 * 1. ֻʵ��·�ɹ���
 * 2. ÿ���˿ڵ����������Ϊ��������gap����ʱ���������ⷢ�����ݡ�
 * 3. ���ݴ�����˿ڵ�����˿���Ҫoverhead��ʱ������
 * 4. �ڲ�����ÿ������˿ڵ�������棨��Ϊ���޴󣬻�100���������ݶ�����
 * 5. ����������棬���뻺���������棬���ݽ���·�����ȱ��浽���뻺�棬���overhead��ʱ�����ں����ݴ����뻺��ת�Ƶ��������
 *
 * ***********************************************
 */


// ��Router���н�ģ
class Router : public cSimpleModule
{
  private:

    cMessage *selfMsgAlloc; //message�ٲö�ʱ�ź�
    cMessage *selfMsgForwardGap; //·�������ⷢ�����ݵļ��

    //Input Buffer, Routing
    //FatTreeMsg* VCMsgBuffer[PortNum][VC][BufferDepth]; //virtual channel��buffer,�������յ���Flit��Ϣ
    FatTreeMsg* InputBuffer[PortNum][RouterBufferDepth]; //���뻺��
    FatTreeMsg* OutputBuffer[PortNum][RouterBufferDepth]; //�������
    //Խ�絽�����ݷ���IDС���Ǳߣ��涨��,0��ʾbuffer�е�һ����ȥ������



  public:
    Router();
    virtual ~Router();
  protected:
    //virtual FatTreeMsg *generateMessage();
    virtual void forwardMessage(FatTreeMsg *msg, int out_port_id);
    //virtual void forwardBufferInfoMsg(BufferInfoMsg *msg, int out_port_id);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual int moveMsgIntoOutput(FatTreeMsg* msg, int port); //��packet�ƶ�����������У��ɹ�����0
    virtual int moveMsgIntoInput(FatTreeMsg* msg, int port); //��packet�ƶ������뻺���У��ɹ�����0
    virtual void shiftOutMsgInInput(int port); //��packet�����뻺�����Ƴ�
    virtual void shiftOutMsgInOutput(int port); //��packet������������Ƴ�

    virtual int ppid2plid(int ppid);
    virtual int plid2ppid(int plid);
    virtual int swpid2swlid(int swpid);
    virtual int swlid2swpid(int swlid);
    virtual int calRoutePort(FatTreeMsg *msg);
    virtual int getNextRouterPort(int current_out_port); //������һ��������router�Ķ˿�

    // The finish() function is called by OMNeT++ at the end of the simulation:
    virtual void finish() override;
};

Define_Module(Router);

Router::Router(){
    selfMsgAlloc = nullptr;
    selfMsgForwardGap = nullptr;
}

Router::~Router(){
    cancelAndDelete(selfMsgAlloc);
    cancelAndDelete(selfMsgForwardGap);
}

void Router::initialize()
{

    for(int i = 0; i < PortNum; i++) {
        for(int j = 0; j < RouterBufferDepth; j++) {
            InputBuffer[i][j] = nullptr;
            OutputBuffer[i][j] = nullptr;
        }
    }

    //��selfMsg���г�ʼ��
    selfMsgAlloc = new cMessage("selfMsgAlloc");
    scheduleAt(Sim_Start_Time, selfMsgAlloc);
    selfMsgForwardGap = new cMessage("selfMsgForwardGap");
    scheduleAt(Sim_Start_Time, selfMsgForwardGap);


}

void Router::handleMessage(cMessage *msg)
{

    if (msg->isSelfMessage()) {

        if(msg==selfMsgAlloc){//����ϢΪ�ٲö�ʱ��Ϣ
            //****************�ٲö�ʱ****************************
            //�ٲü��ΪRouterOverhead * CLK_CYCLE
            scheduleAt(simTime() + RouterOverhead * CLK_CYCLE, selfMsgAlloc);
            //��ÿ�����뻺����ǰ������ݽ���ת�Ƶ��������
            for(int i = 0; i < PortNum; i++) {
                if(InputBuffer[i][0] != nullptr) {
                    FatTreeMsg* ftmsg = InputBuffer[i][0];
                    shiftOutMsgInInput(i);
                    int outport = calRoutePort(ftmsg);
                    if(moveMsgIntoOutput(ftmsg, outport) == 0) {
                        if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
                            EV<<"Allocation & Routing >> ROUTER: "<<getIndex()<<"("<<swpid2swlid(getIndex())<<"), INPORT: "<<i<<" , To OUTPUT: "
                                <<outport<<", Allocation Succeeded" <<", Received MSG: { "<<ftmsg<<" }\n";
                        }
                    } else {
                        if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
                            EV<<"Allocation & Routing >> ROUTER: "<<getIndex()<<"("<<swpid2swlid(getIndex())<<"), INPORT: "<<i<<" , To OUTPUT: "
                                <<outport<<", Allocation Failed" <<", Received MSG: { "<<ftmsg<<" }\n";
                        }
                    }
                }
            }
        } else if(msg == selfMsgForwardGap){
            //*******************·�������ⷢ�����ݶ�ʱ**************************
            scheduleAt(simTime() + RouterForwardGap * CLK_CYCLE, selfMsgForwardGap);
            //��ÿһ����������������ݽ��з�������
            for(int i = 0; i < PortNum; i++) {
                if(OutputBuffer[i][0] != nullptr) {
                    FatTreeMsg* ftmsg = OutputBuffer[i][0];
                    forwardMessage(ftmsg, i);
                    shiftOutMsgInInput(i);
                }
            }

        }

    }else{ //������Ϣ�����յ�����·��������Ϣ

        //**********************�յ������˿ڵ�fatTreeMsg������Ϣ*******************
        //�յ�����ϢΪFatTreeMsg������Ϣ

        FatTreeMsg *ftmsg = check_and_cast<FatTreeMsg *>(msg);

        int input_port = ftmsg->getFrom_router_port();
        if(moveMsgIntoInput(ftmsg, input_port) == 0) {
            if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
                EV<<"Input Buffer >> ROUTER: "<<getIndex()<<"("<<swpid2swlid(getIndex())<<"), INPORT: "<<input_port<<", Store Succeeded" <<", Received MSG: { "<<ftmsg<<" }\n";
            }
        } else {
            if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
                EV<<"Input Buffer >> ROUTER: "<<getIndex()<<"("<<swpid2swlid(getIndex())<<"), INPORT: "<<input_port<<", Store Failed" <<", Received MSG: { "<<ftmsg<<" }\n";
            }
        }


    }

}


//����...����·�ɶ˿�
void Router::forwardMessage(FatTreeMsg *msg, int out_port_id)
{

    // Increment hop count.
    msg->setHopCount(msg->getHopCount()+1);


    int k = out_port_id;
    char str1[20]="port_";
    char str2[20];
    sprintf(str2, "%d", k);
    strcat(str1,str2);
    strcat(str1,"$o");
    //EV<<"k="<<k<<" str1="<<str1<<" str2="<<str2<<"\n";
    msg->setFrom_router_port(getNextRouterPort(k));//���ý��ܸ�msg��Router��port�˿ں�
    send(msg,str1);
    int cur_swpid=getIndex();//��ǰ·������id
    int cur_swlid=swpid2swlid(cur_swpid);
    if (Verbose >= VERBOSE_DEBUG_MESSAGES) {
        EV << "Forwarding message { " << msg << " } from router "<<cur_swpid<<"("<<cur_swlid<<")"<< " through port "<<k<<"\n";
    }

}



int Router::moveMsgIntoOutput(FatTreeMsg* msg, int port) {
    for(int i = 0; i < RouterBufferDepth; i++) {
        if(OutputBuffer[port][i] == nullptr) {
            OutputBuffer[port][i] = msg;
            return 0; //success
        }
    }
    return -1; //failed

}

int Router::moveMsgIntoInput(FatTreeMsg* msg, int port) {
    for(int i = 0; i < RouterBufferDepth; i++) {
        if(InputBuffer[port][i] == nullptr) {
            InputBuffer[port][i] = msg;
            return 0;
        }
    }
    return -1;
}
void Router::shiftOutMsgInInput(int port) {
    for(int i = 1; i < RouterBufferDepth; i++) {
        InputBuffer[port][i-1] = InputBuffer[port][i];
        if(InputBuffer[port][i] == nullptr)
            break;

    }
    InputBuffer[port][RouterBufferDepth-1] = nullptr;
}

void Router::shiftOutMsgInOutput(int port) {
    for(int i = 1; i < RouterBufferDepth; i++) {
        OutputBuffer[port][i-1] = OutputBuffer[port][i];
        if(OutputBuffer[port][i] == nullptr)
            break;

    }
    OutputBuffer[port][RouterBufferDepth-1] = nullptr;
}


//��ppid����plid
int Router::ppid2plid(int ppid){
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
int Router::plid2ppid(int plid){
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

//��swpid����swlid
int Router::swpid2swlid(int swpid){
    //�����ж�swpid����һ��
    int level=0;
    bool find_level=false;
    for(int i=0;i<LevelNum-1;i++){
        if(swpid>=i*SwLowEach and swpid<(i+1)*SwLowEach){
            level=i;
            find_level=true;
            break;
        }
    }
    if(!find_level)
        level=LevelNum-1;
    //�Ѿ��ҵ�switch���ڲ㣬������������б���
    //�ȶԷǶ����switch���б���
    if(level<LevelNum-1){
        int tmp=swpid-level*SwLowEach;
        int IDtmp=0;
        int mul=1;
        for(int i=0;i<LevelNum-2;i++){
            IDtmp=mul*(tmp%(PortNum/2))+IDtmp;
            tmp=(int)(tmp/(PortNum/2));
            mul=mul*10;
        }
        IDtmp=IDtmp+mul*tmp;
        mul=mul*10;
        IDtmp=mul*level+IDtmp;//��ǰ��������Ĳ���
        return IDtmp;
    }
    //�������Զ����switch���в���
    else{
        int tmp=swpid;
        int IDtmp=0;
        int mul=1;
        for(int i=0;i<LevelNum-1;i++){
            IDtmp=mul*(tmp%(PortNum/2))+IDtmp;
            tmp=(int)(tmp/(PortNum/2));
            mul=mul*10;
        }
        IDtmp=mul*level+IDtmp;
        return IDtmp;
    }
}

//swlidתswpid
int Router::swlid2swpid(int swlid){
    int tmp=swlid;
    int level=tmp/(pow(10,(LevelNum-1)));
    tmp=tmp%((int)pow(10,(LevelNum-1)));
    int IDtmp=level*SwLowEach;
    int mul=1;
    for(int i=0;i<LevelNum-1;i++){
        IDtmp=IDtmp+mul*(tmp%10);
        mul=mul*(PortNum/2);
        tmp=tmp/10;
    }
    return IDtmp;
}

//���ݵ�ǰrouter��swpid��msg��dst_ppid������ת���Ķ˿�
int Router::calRoutePort(FatTreeMsg *msg){
    int cur_swpid=getIndex();//��ǰ·������id
    int cur_swlid=swpid2swlid(cur_swpid);
    int level=cur_swlid/pow(10,LevelNum-1);//Router��level
    int dst_ppid=msg->getDst_ppid();
    int dst_plid=ppid2plid(dst_ppid);
    //EV<<dst_ppid<<" "<<dst_plid<<"\n";
    //�ж�switch�Ƿ�Ϊ����
    int ptmp=dst_plid/pow(10,level+1);//
    int ctmp=(cur_swlid%((int)pow(10,LevelNum-1)))/pow(10,level);
    bool isAncestor=(ptmp==ctmp);
    int k;//ת���Ķ˿�
    //EV<<cur_swpid<<" "<<cur_swlid<<" "<<level<<" "<<dst_ppid<<" "<<dst_plid<<" "<<ptmp<<" "<<ctmp<<"\n";
    //���switch��dst_ppid�����ȣ������¶˿�ת�����������϶˿�ת��
    if(isAncestor){
        //����ת��
        k=(dst_plid/((int)pow(10,level)))%10;//ͨ���˿�pl������ת��
        //EV<<"isAncestor, k="<<k<<"\n";
        return k;
    }else{
        //����ת��
        k=(dst_plid/((int)pow(10,level)))%10+PortNum/2;//k=pl��+m/2
        //EV<<"notAncestor, k="<<k<<" "<<dst_ppid<<" "<<dst_plid<<" "<<(int)pow(10,level)<<" "<<(dst_plid/((int)pow(10,level)))%10<<"\n";
        return k;
    }

}

//�����յ���msg����һ��·�����Ķ˿ں�
int Router::getNextRouterPort(int current_out_port){


    int cur_swpid=getIndex();//��ǰ·������id
    int cur_swlid=swpid2swlid(cur_swpid);
    int level=cur_swlid/pow(10,LevelNum-1);//Router��level
    bool lowerRouter = (current_out_port>=(PortNum/2))&&(level!=LevelNum-1); //�ж��Ƿ�����ת��������ת����Ϊ�²�router

    int ctmp=(cur_swlid%((int)pow(10,LevelNum-1)));//ȥ����level��swlid
    int k;//��һ��Router�Ľ��ܶ˿�
    if(!lowerRouter){//�ϲ��Router
        if(level==0){
            k=0;//level==0ʱ��Ϊ�϶˿ڣ����msg����processor����processorֻ��һ���˿ڣ�Ĭ��processor�Ľ��ܶ˿�Ϊ0
        }else{
            int lowLevel=level-1;//�²��level
            k = (ctmp/((int) pow(10,lowLevel)))%10+PortNum/2;
        }

    }else{ //�²��Router
        k = (ctmp/((int) pow(10,level)))%10;
    }
    return k;
}




void Router::finish()
{
    // This function is called by OMNeT++ at the end of the simulation.
    //EV << "Sent:     " << numSent << endl;
    //EV << "Received: " << numReceived << endl;
    //EV << "Hop count, min:    " << hopCountStats.getMin() << endl;
    //EV << "Hop count, max:    " << hopCountStats.getMax() << endl;
    //EV << "Hop count, mean:   " << hopCountStats.getMean() << endl;
    //EV << "Hop count, stddev: " << hopCountStats.getStddev() << endl;

    //recordScalar("#sent", numSent);
    //recordScalar("#received", numReceived);

    //hopCountStats.recordAs("hop count");

}

