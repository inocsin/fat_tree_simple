/*
 * router.cc
 *
 *  Created on: 2017年1月16日
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
 * 实现功能：
 *
 * 1. 只实现路由功能
 * 2. 每个端口的输出带宽设为参数，以gap’个时钟周期向外发送数据。
 * 3. 数据从输入端口到输出端口需要overhead个时钟周期
 * 4. 内部包含每个输出端口的输出缓存（设为无限大，或100，多余数据丢弃）
 * 5. 设计两个缓存，输入缓存和输出缓存，数据进入路由器先保存到输入缓存，间隔overhead个时钟周期后，数据从输入缓存转移到输出缓存
 *
 * ***********************************************
 */


// 对Router进行建模
class Router : public cSimpleModule
{
  private:

    cMessage *selfMsgAlloc; //message仲裁定时信号
    cMessage *selfMsgForwardGap; //路由器向外发送数据的间隔

    //Input Buffer, Routing
    //FatTreeMsg* VCMsgBuffer[PortNum][VC][BufferDepth]; //virtual channel的buffer,里面存放收到的Flit信息
    FatTreeMsg* InputBuffer[PortNum][RouterBufferDepth]; //输入缓冲
    FatTreeMsg* OutputBuffer[PortNum][RouterBufferDepth]; //输出缓冲
    //越早到的数据放在ID小的那边，规定好,0表示buffer中第一个出去的数据



  public:
    Router();
    virtual ~Router();
  protected:
    //virtual FatTreeMsg *generateMessage();
    virtual void forwardMessage(FatTreeMsg *msg, int out_port_id);
    //virtual void forwardBufferInfoMsg(BufferInfoMsg *msg, int out_port_id);
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual int moveMsgIntoOutput(FatTreeMsg* msg, int port); //将packet移动到输出缓存中，成功返回0
    virtual int moveMsgIntoInput(FatTreeMsg* msg, int port); //将packet移动到输入缓存中，成功返回0
    virtual void shiftOutMsgInInput(int port); //将packet从输入缓存中移除
    virtual void shiftOutMsgInOutput(int port); //将packet从输出缓存中移除

    virtual int ppid2plid(int ppid);
    virtual int plid2ppid(int plid);
    virtual int swpid2swlid(int swpid);
    virtual int swlid2swpid(int swlid);
    virtual int calRoutePort(FatTreeMsg *msg);
    virtual int getNextRouterPort(int current_out_port); //计算下一个相连的router的端口

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

    //对selfMsg进行初始化
    selfMsgAlloc = new cMessage("selfMsgAlloc");
    scheduleAt(Sim_Start_Time, selfMsgAlloc);
    selfMsgForwardGap = new cMessage("selfMsgForwardGap");
    scheduleAt(Sim_Start_Time, selfMsgForwardGap);


}

void Router::handleMessage(cMessage *msg)
{

    if (msg->isSelfMessage()) {

        if(msg==selfMsgAlloc){//自消息为仲裁定时消息
            //****************仲裁定时****************************
            //仲裁间隔为RouterOverhead * CLK_CYCLE
            scheduleAt(simTime() + RouterOverhead * CLK_CYCLE, selfMsgAlloc);
            //对每个输入缓存最前面的数据进行转移到输出缓存
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
            //*******************路由器向外发送数据定时**************************
            scheduleAt(simTime() + RouterForwardGap * CLK_CYCLE, selfMsgForwardGap);
            //对每一个输出输出缓冲的数据进行发送数据
            for(int i = 0; i < PortNum; i++) {
                if(OutputBuffer[i][0] != nullptr) {
                    FatTreeMsg* ftmsg = OutputBuffer[i][0];
                    forwardMessage(ftmsg, i);
                    shiftOutMsgInInput(i);
                }
            }

        }

    }else{ //非自消息，即收到其他路由器的消息

        //**********************收到其他端口的fatTreeMsg数据消息*******************
        //收到的消息为FatTreeMsg数据消息

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


//调用...计算路由端口
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
    msg->setFrom_router_port(getNextRouterPort(k));//设置接受该msg的Router的port端口号
    send(msg,str1);
    int cur_swpid=getIndex();//当前路由器的id
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


//从ppid计算plid
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
//从plid计算ppid
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

//从swpid计算swlid
int Router::swpid2swlid(int swpid){
    //首先判断swpid在哪一层
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
    //已经找到switch所在层，接下来对其进行编码
    //先对非顶层的switch进行编码
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
        IDtmp=mul*level+IDtmp;//最前面加上它的层数
        return IDtmp;
    }
    //接下来对顶层的switch进行操作
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

//swlid转swpid
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

//根据当前router的swpid和msg的dst_ppid来计算转发的端口
int Router::calRoutePort(FatTreeMsg *msg){
    int cur_swpid=getIndex();//当前路由器的id
    int cur_swlid=swpid2swlid(cur_swpid);
    int level=cur_swlid/pow(10,LevelNum-1);//Router的level
    int dst_ppid=msg->getDst_ppid();
    int dst_plid=ppid2plid(dst_ppid);
    //EV<<dst_ppid<<" "<<dst_plid<<"\n";
    //判断switch是否为祖先
    int ptmp=dst_plid/pow(10,level+1);//
    int ctmp=(cur_swlid%((int)pow(10,LevelNum-1)))/pow(10,level);
    bool isAncestor=(ptmp==ctmp);
    int k;//转发的端口
    //EV<<cur_swpid<<" "<<cur_swlid<<" "<<level<<" "<<dst_ppid<<" "<<dst_plid<<" "<<ptmp<<" "<<ctmp<<"\n";
    //如果switch是dst_ppid的祖先，则向下端口转发，否则向上端口转发
    if(isAncestor){
        //向下转发
        k=(dst_plid/((int)pow(10,level)))%10;//通过端口pl’进行转发
        //EV<<"isAncestor, k="<<k<<"\n";
        return k;
    }else{
        //向上转发
        k=(dst_plid/((int)pow(10,level)))%10+PortNum/2;//k=pl’+m/2
        //EV<<"notAncestor, k="<<k<<" "<<dst_ppid<<" "<<dst_plid<<" "<<(int)pow(10,level)<<" "<<(dst_plid/((int)pow(10,level)))%10<<"\n";
        return k;
    }

}

//计算收到该msg的下一跳路由器的端口号
int Router::getNextRouterPort(int current_out_port){


    int cur_swpid=getIndex();//当前路由器的id
    int cur_swlid=swpid2swlid(cur_swpid);
    int level=cur_swlid/pow(10,LevelNum-1);//Router的level
    bool lowerRouter = (current_out_port>=(PortNum/2))&&(level!=LevelNum-1); //判断是否向上转发，向上转发则为下层router

    int ctmp=(cur_swlid%((int)pow(10,LevelNum-1)));//去除掉level的swlid
    int k;//下一个Router的接受端口
    if(!lowerRouter){//上层的Router
        if(level==0){
            k=0;//level==0时，为上端口，因此msg发到processor，而processor只有一个端口，默认processor的接受端口为0
        }else{
            int lowLevel=level-1;//下层的level
            k = (ctmp/((int) pow(10,lowLevel)))%10+PortNum/2;
        }

    }else{ //下层的Router
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

