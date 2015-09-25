/**********************************
 * FILE NAME: MP2Node.h
 *
 * DESCRIPTION: MP2Node class header file
 **********************************/

#ifndef MP2NODE_H_
#define MP2NODE_H_

/**
 * Header files
 */
#include "stdincludes.h"
#include "EmulNet.h"
#include "Node.h"
#include "HashTable.h"
#include "Log.h"
#include "Params.h"
#include "Message.h"
#include "Queue.h"

/**
 * CLASS NAME: MP2Node
 *
 * DESCRIPTION: This class encapsulates all the key-value store functionality
 * 				including:
 * 				1) Ring
 * 				2) Stabilization Protocol
 * 				3) Server side CRUD APIs
 * 				4) Client side CRUD APIs
 */
class MP2Node {

    
typedef struct
    {
        //Structure for storing transaction information
        
        int replies; //Keeps count of the number of replies received for a transaction
        string key; //Stores the Key for the transaction
        string value; //Stores the Value associated with the Key
        MessageType type; //Stores the type of transaction READ,CREATE,UPDATE,DELETE
        int timestamp; //Stores the global timestamp at which the transaction started
        int read_value_timestamp; //Stores the timestamp of the Read value, for READ operations
        
    }transaction_info;
    
private:
	
    // Vector holding the next two neighbors in the ring who have my replicas
	vector<Node> hasMyReplicas;
	
    // Vector holding the previous two neighbors in the ring whose replicas I have
	vector<Node> haveReplicasOf;
	
    // Ring
	vector<Node> ring;
	
    // Hash Table
	HashTable * ht;
	
    // Member representing this member
	Member *memberNode;
	
    // Params object
	Params *par;
	
    // Object of EmulNet
	EmulNet * emulNet;
	
    // Object of Log
	Log * log;
    
    //Pending Transactions for which node is the coordinator
    map<int,transaction_info> transactions;
    
    //Transaction id
    int transactionId;
    

public:
	MP2Node(Member *memberNode, Params *par, EmulNet *emulNet, Log *log, Address *addressOfMember);
	Member * getMemberNode() {
		return this->memberNode;
	}

	// ring functionalities
	void updateRing();
	vector<Node> getMembershipList();
	size_t hashFunction(string key);
	void findNeighbors();

	// client side CRUD APIs
	void clientCreate(string key, string value);
	void clientRead(string key);
	void clientUpdate(string key, string value);
	void clientDelete(string key);

	// receive messages from Emulnet
	bool recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);

	// handle messages from receiving queue
	void checkMessages();

	// coordinator dispatches messages to corresponding nodes
	void dispatchMessages(Message message);

	// find the addresses of nodes that are responsible for a key
	vector<Node> findNodes(string key);

	// server
	bool createKeyValue(string key, string value, ReplicaType replica);
	string readKey(string key);
	bool updateKeyValue(string key, string value, ReplicaType replica);
	bool deletekey(string key);

	// stabilization protocol - handle multiple failures
	void stabilizationProtocol();
    
    //checks if the ring structure has changed since the last run
    bool hasRingChanged(vector<Node> memList);
    
    //sets the information associated with the transaction
    void setTransaction(int transId, string key, string value, MessageType type);
    
    //function for handling READ messages
    void handleReadMessages(Message* msg);
    
    //function for handling READ/REPLY messages
    void handleReadReplyMessage(Message* msg);
    
    //function for handling UPDATE messages
    void handleUpdateMessage(Message* msg);
    
    //function for finding keys stored at the node depending on the Replica type passed
    vector<string> findMyKeys(ReplicaType type);
    
    //Checks if the passed node is still alive - present in the current ring
    bool isNodeAlive(Node node);
    
    //Checks if the node passed, is present in the nodes old Neighbours
    bool isOldNeighbour(vector<Node> nodes, Node node);

	~MP2Node();
};

#endif /* MP2NODE_H_ */
