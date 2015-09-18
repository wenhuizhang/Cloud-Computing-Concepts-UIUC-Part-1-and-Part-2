/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

#define CFG_GOSSIP_B 8
#define CFG_GOSSIP_ENTRY_EXPIRATION 10
/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINRSP,
    HEARTBEAT,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
class MessageHdr {
public:
    MessageHdr(MsgTypes msgtype)
    : msgType(msgtype), heartbeat(0)
    {
        addr.init();
    }
    enum MsgTypes msgType;
    Address addr; // sender's addr
    long heartbeat;
    
};

class JOINREQMsg : public MessageHdr {
public:
    JOINREQMsg()
    : MessageHdr(JOINREQ)
    {
    }
};
class HEARTBEATMsg : public MessageHdr {
public:
    HEARTBEATMsg()
    : MessageHdr(HEARTBEAT)
    {
    }
    vector<MemberListEntry> memberList;
};
class JOINRSPMsg : public HEARTBEATMsg {
public:
    JOINRSPMsg()
    {
        msgType = JOINRSP;
    }
};

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
    EmulNet *emulNet;
    Log *log;
    Params *par;
    Member *memberNode;
    char NULLADDR[6];
    
public:
    MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
    Member * getMemberNode() {
        return memberNode;
    }
    int recvLoop();
    static int enqueueWrapper(void *env, char *buff, int size);
    void nodeStart(char *servaddrstr, short serverport);
    int initThisNode(Address *joinaddr);
    int introduceSelfToGroup(Address *joinAddress);
    int finishUpThisNode();
    void nodeLoop();
    void checkMessages();
    bool recvCallBack(void *env, char *data, int size);
    void nodeLoopOps();
    int isNullAddress(Address *addr);
    Address getJoinAddress();
    void initMemberListTable(Member *memberNode);
    void printAddress(Address *addr);
    virtual ~MP1Node();
    
private:
    int getId();
    int getPort();
    
    void removeExpiredMembers();
    void doGossip();
    
    MemberListEntry* findMemberEntry(std::vector<MemberListEntry>& lst, int id);
    void updateMemberList(vector<MemberListEntry>& memberList);
    
    void onRcvJOINREQ(JOINREQMsg* msg);
    void onRcvJOINRSP(JOINRSPMsg* msg);
    void onRcvHEARTBEAT(HEARTBEATMsg* msg);
};

#endif /* _MP1NODE_H_ */
