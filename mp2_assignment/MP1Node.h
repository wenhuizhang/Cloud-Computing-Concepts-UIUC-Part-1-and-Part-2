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
//	MP1Node(Member *, Params *, Address *);
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
	void processUpdateRep(void *env, char *data, int size);
	void processUpdateReq(void *env, char *data, int size);
	void processJoinRep(void *env, char *data, int size);
	void processJoinReq(void *env, char *data, int size);
	void nodeLoopOps();
	void deleteTimeOutNodes();
	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void initMemberListTable(Member *memberNode);
	void cleanMemberListTable(Member *memberNode);
	vector<MemberListEntry>::iterator addEntryToMemberList(int id, short port, long heartbeat);
	//void deleteEntryFromMemberList(member *node, struct Address *addr);
	char* serialize(Member *node);
	char* deserializeAndUpdateTable(const char *msg);
	vector<MemberListEntry>::iterator searchList(int id, short port);
	char* encode(int id, short port, long heartbeat, long timestamp );
	void decodeToAddress(Address *addr, int id, short port);
	void printAddress(Address *addr);
	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
