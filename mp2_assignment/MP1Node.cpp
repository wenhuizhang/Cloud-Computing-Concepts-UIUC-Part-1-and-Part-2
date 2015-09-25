/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/**
 * Overloaded Constructor
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);
    memberNode->myPos = addEntryToMemberList(id, port, memberNode->heartbeat);
    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == strcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node
 */
int MP1Node::finishUpThisNode(){
    cleanMemberListTable(memberNode);
    return SUCCESS;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {

	Member *node = (Member *) env;
	MessageHdr *msg = (MessageHdr *)data;
	char *packetData = (char *)(msg + 1);

	//if ( (unsigned)size <  (strlen(packetData)+1) ) {
	if ( (unsigned)size < sizeof(MessageHdr) ) {
#ifdef DEBUGLOG
		log->LOG(&node->addr, "Faulty packet received - ignoring");
		return false;
#endif
	}

#ifdef DEBUGLOG
	log->LOG(&((Member *)env)->addr, "Received message type %d with %d B payload", msg->msgType, size - sizeof(MessageHdr));
#endif

	// If not yet in the group then accept only JOINREP
	if ( (memberNode->inGroup && msg->msgType >= 0 && msg->msgType <= DUMMYLASTMSGTYPE)
			|| (!memberNode->inGroup && msg->msgType == JOINREP) ) {

		switch(msg->msgType) {
		case JOINREQ:
			processJoinReq(env, packetData, size - sizeof(MessageHdr));
			break;
		case JOINREP:
			processJoinRep(env, packetData, size - sizeof(MessageHdr));
			break;
		case UPDATEREQ:
			processUpdateReq(env, packetData, size - sizeof(MessageHdr));
			break;
		case UPDATEREP:
			processUpdateRep(env, packetData, size - sizeof(MessageHdr));
			break;
		default:
			break;
		}
	}

    // Else ignore i.e., garbled message
    //free(data);
    return true;
}

/**
 * FUNCTION NAME: processJoinReq
 *
 * DESCRIPTION: This function is run by the coordinator to process JOIN requests
 */
void MP1Node::processJoinReq(void *env, char *data, int size) {
    Member *node = (Member*)env;

    //first get the new node's info by deserializing
    Address newaddr;
    long heartbeat;
    memcpy(newaddr.addr, data, sizeof(newaddr.addr));
    memcpy(&heartbeat, data+1+sizeof(newaddr.addr), sizeof(long));

    int id = *(int*)(&newaddr.addr);
    short port = *(short*)(&newaddr.addr[4]);

    //second serialize the table(put it in a string form)
    char *table = serialize(node);

    //next construct a message: type: JOINREPLY, content: table
    size_t sz = sizeof(MessageHdr) + 1 + strlen(table);
    MessageHdr *msg = (MessageHdr *) malloc(sz * sizeof(char));
    msg->msgType = JOINREP;
    char * ptr = (char *) (msg+1);
    memcpy(ptr, table, strlen(table)+1);

    emulNet->ENsend(&node->addr, &newaddr, (char *)msg, (int)sz);
    free(msg);
    free(table);

    vector<MemberListEntry>::iterator it;
    it = searchList(id, port);
    // If the iterator returned points to the end of the list, then it is not found
    if ( it == memberNode->memberList.end() ) {
    	addEntryToMemberList(id, port, heartbeat);
    }
    
    return;
}

/**
 * FUNCTION NAME: procesJoinRep
 *
 * DESCRIPTION: This function is the message handler for JOINREP. The list will be present in the message.
 * 				It needs to add the list to its own table. Data parameter contains the entire table
 */
void MP1Node::processJoinRep(void *env, char *data, int size) {
#ifdef DEBUGLOG
    static char s[1024];
    char *s1 = s;
    s1 += sprintf(s1, "Received neighbor list:");
#endif
    deserializeAndUpdateTable(data);
    memberNode->inGroup = true;
    return;
}

/**
 * FUNCTION NAME: processUpdateReq
 *
 * DESCRIPTION: This function is the message handler for UPDATEREQ
 * note: we dont need updatereq here; each node gossip its table to other nodes periodically
 * and the gossip action is done inside the nodeloopops
 */
void MP1Node::processUpdateReq(void *env, char *data, int size) {
    return;
}

/**
 * FUNCTION NAME: processUpdateRep
 *
 * DESCRIPTION: This function is the message handler for UPDATEREP
 * when each node receives a table, it needs to call the deserializeAndUpdate function to update its own table
 * data: the real message (the table without the type )
 */
void MP1Node::processUpdateRep(void *env, char *data, int size) {
    deserializeAndUpdateTable(data);
    return;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION:
 */
void MP1Node::nodeLoopOps() {

	// 1. check if any node hasn't responsed within PING_TIMEOUT then delete these nodes
	// 2. Send its own memberlist to a portion of nodes (using gossip protocol)

    // 1. first clean up time-out nodes
    deleteTimeOutNodes();

    // 2. next increment heartbeat
    memberNode->heartbeat++;
    // get your own id and update in table
    int id = *(int*)(&memberNode->addr.addr);
    for ( vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++ ) {
    		// This is your own node
            if( id == (*(it)).getid() ) {
            	(*(it)).setheartbeat(memberNode->heartbeat);
            }
    }

	if ( 0 == --(memberNode->pingCounter) ) {
		char *table = serialize(memberNode);

		size_t sz = sizeof(MessageHdr) + 1 + strlen(table);
        MessageHdr *msg = (MessageHdr *)malloc(sz * sizeof(char));
		msg->msgType = UPDATEREP;
		char *ptr = (char *)(msg + 1);
		memcpy(ptr, table, strlen(table)+1);

		for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it ) {
            Address addr;
            decodeToAddress(&addr, (*(it)).getid(), (*(it)).getport());
		    if ( memcmp(addr.addr, memberNode->addr.addr, sizeof(addr.addr)) != 0 ) {
		        emulNet->ENsend(&memberNode->addr, &addr, (char *)msg, sz);
		    }
		}
		free(msg);
		free(table);

		memberNode->pingCounter = TFAIL;
	}

    return;
}

/**
 * FUNCTION NAME: deleteTimeOutNodes
 *
 * DESCRIPTION: Delete all the nodes from the membership list who have timed out beyond TREMOVE
 */
void MP1Node::deleteTimeOutNodes() {
	// If the list is empty then return
	if ( memberNode->memberList.empty() ) {
		return;
	}

	// get your own id
    int id = *(int*)(&memberNode->addr.addr);
    for ( int i = 0; i < memberNode->memberList.size(); i++ ) {
    	if ( id != memberNode->memberList.at(i).id && ( par->getcurrtime() - (memberNode->memberList.at(i).timestamp) ) > TREMOVE ) {
            Address addr_to_delete;
            decodeToAddress(&addr_to_delete, memberNode->memberList.at(i).id, memberNode->memberList.at(i).port);
            memberNode->memberList.erase(memberNode->memberList.begin() + i);
            memberNode->nnb--;
            log->logNodeRemove(&memberNode->addr, &addr_to_delete);
        }
    }
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: cleanMemberListTable
 *
 * DESCRIPTION: Clear the membership list
 */
void MP1Node::cleanMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: addEntryToMemberList
 *
 * DESCRIPTION: Add a new entry to the membership list
 */
vector<MemberListEntry>::iterator MP1Node::addEntryToMemberList(int id, short port, long heartbeat) {
	vector<MemberListEntry>::iterator it;
    if( id > par->MAX_NNB ) {
    	cout<<"Unknown id " <<id<<endl;
 #ifdef DEBUGLOG
    	static char s[1024];
    	char *s1 = s;
    	s1 += sprintf(s1, "Unknown ID");
#endif
        return it;
    }
    MemberListEntry newEntry(id, port, heartbeat, par->getcurrtime());
    memberNode->memberList.emplace_back(newEntry);
    memberNode->nnb++;
    if ( memberNode->nnb > par->MAX_NNB ) {
    	cout <<memberNode->nnb<<" exceeds the capacity"<<endl;
 #ifdef DEBUGLOG
    	static char s[1024];
    	char *s1 = s;
    	s1 += sprintf(s1, "Exceeds membership capacity");
#endif
    }
 	Address addr;
    decodeToAddress(&addr, id, port);
    log->logNodeAdd(&memberNode->addr, &addr);

    it = memberNode->memberList.end();
    return --it;
}

/**
 * FUNCTION NAME: serialize
 *
 * DESCRIPTION: Serialize the membership list
 */
char* MP1Node::serialize(Member *node)
{
    char *buffer = NULL;
    for ( unsigned int i = 0; i < memberNode->memberList.size(); i++ ) {
    	char *entry = encode(memberNode->memberList.at(i).getid(), memberNode->memberList.at(i).getport(), memberNode->memberList.at(i).getheartbeat(), memberNode->memberList.at(i).gettimestamp());

        if(!buffer) {
            asprintf(&buffer, "%s|", entry);
        }
        else {
            asprintf(&buffer, "%s%s|", buffer, entry);
        }
        if(entry) {
            free(entry);
        }
    }
    return buffer;
}

/**
 * FUNCTION NAME: deserializeAndUpdateTable
 *
 * DESCRIPTION: Deserialize the message and update the membership list
 */
char* MP1Node::deserializeAndUpdateTable(const char *msg){
    char *buffer = NULL;
    asprintf(&buffer, "%s", msg);

    char * pch;
    pch = strtok (buffer,"|");
    while (pch != NULL) {
        char *row = NULL;
        asprintf(&row, "%s", pch);
        int id;
        short port;
        long heartbeat;
        long timestamp;

        sscanf(row,"%d:%hi~%ld~%ld", &id, &port, &heartbeat, &timestamp);
        vector<MemberListEntry>::iterator found = searchList(id, port);
    
        // If the entry already exists and the new heartbeat is bigger, do an update
        if( found != memberNode->memberList.end() ) {
            if( (*(found)).getheartbeat() < heartbeat ) {
            	(*(found)).setheartbeat(heartbeat);
            	(*(found)).settimestamp(par->getcurrtime());
            }
        }   
        else {
            addEntryToMemberList(id, port, heartbeat);
        }     
        pch = strtok (NULL, "|");

    }
    return NULL;
}

/**
 * FUNCTION NAME: searchList
 *
 * DESCRIPTION: Search the membership list
 */
vector<MemberListEntry>::iterator MP1Node::searchList(int id, short port) {
	vector<MemberListEntry>::iterator it;
	for ( it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it ) {
        if( (*(it)).getid() == id ) {
            return it;
        }
    }
	return it;
}

/**
 * FUNCTION NAME: encode
 *
 * DESCRIPTION: Encode the information
 */
char* MP1Node::encode(int id, short port, long heartbeat, long timestamp ) {
    char* buffer = NULL;
    asprintf(&buffer, "%d:%hi~%ld~%ld", id, port, heartbeat, timestamp );
    return buffer;
}

/**
 * FUNCTION NAME: decodeToAddress
 *
 * DESCRIPTION: Decode the information to an Address object
 */
void MP1Node::decodeToAddress(Address *addr, int id, short port) {
    memcpy(&addr->addr[0], &id,  sizeof(int));
    memcpy(&addr->addr[4], &port, sizeof(short));
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
