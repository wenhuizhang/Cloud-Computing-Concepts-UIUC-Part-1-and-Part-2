/**********************************
 * FILE NAME: MP2Node.cpp
 *
 * DESCRIPTION: MP2Node class definition
 *
 * WRITTEN BY: Udit Mehrotra
 *
 * NET ID: umehrot2
 **********************************/
#include "MP2Node.h"

/**
 * constructor
 */
MP2Node::MP2Node(Member *memberNode, Params *par, EmulNet * emulNet, Log * log, Address * address) {
	this->memberNode = memberNode;
	this->par = par;
	this->emulNet = emulNet;
	this->log = log;
	ht = new HashTable();
	this->memberNode->addr = *address;
    this->transactionId = 0;
}

/**
 * Destructor
 */
MP2Node::~MP2Node() {
	delete ht;
	delete memberNode;
}


/**
 * checks if the ring structure has changed since the last run
 */
bool MP2Node::hasRingChanged(vector<Node> memList)
{
    //Checks if the nodes membership list and ring are the same
    bool isChanged = false;
    
    if(memList.size() == ring.size())
    {
        for(int i = 0; i < memList.size(); i++)
        {
            Address* addr = ring[i].getAddress();
            Address* addr2 = memList[i].getAddress();
            if(!(*addr==*addr2))
            {
                continue;
            }
            else
            {
                isChanged = true;
                break;
            }
        }
    }
    else
    {
        isChanged = true;
    }
    
    return isChanged;
}


/**
 * FUNCTION NAME: updateRing
 *
 * DESCRIPTION: This function does the following:
 * 				1) Gets the current membership list from the Membership Protocol (MP1Node)
 * 				   The membership list is returned as a vector of Nodes. See Node class in Node.h
 * 				2) Constructs the ring based on the membership list
 * 				3) Calls the Stabilization Protocol
 */
void MP2Node::updateRing() {
	/*
	 * Implement this. Parts of it are already implemented
	 */
	vector<Node> curMemList;
	bool change = false;

	// Get the current membership list
    curMemList = getMembershipList();

	// Sort the list based on the hashCode
	sort(curMemList.begin(), curMemList.end());
    
    // Check if the ring is empty - running for first time
    if(ring.empty())
    {
        // Initialize the ring with nodes from the membership list
        vector<Node>::iterator it;
        for(it=curMemList.begin(); it != curMemList.end(); ++it)
        {
            ring.push_back(*it);
        }
    
        // Initialize the hasReplicasOf and haveReplicaOf vectors
        for(int loop = 0; loop < ring.size(); loop++)
        {
            if((memcmp(ring.at(loop).getAddress()->addr, &this->memberNode->addr,sizeof(Address)) == 0))
            {

                if(loop == 0)
                {
                    haveReplicasOf.push_back(ring.at(ring.size()-1));
                    haveReplicasOf.push_back(ring.at(ring.size()-2));
                }
                else if(loop == 1)
                {
                    haveReplicasOf.push_back(ring.at(0));
                    haveReplicasOf.push_back(ring.at(ring.size()-1));
                }
                else
                {
                    haveReplicasOf.push_back(ring.at(loop-1));
                    haveReplicasOf.push_back(ring.at(loop-2));
                }
            

                hasMyReplicas.push_back(ring.at((loop+1)%ring.size()));
                hasMyReplicas.push_back(ring.at((loop+2)%ring.size()));

            }
            
        }
    }
    
    // Check if the ring has changed in the previous global time
    bool isChanged = hasRingChanged(curMemList);
    
    if(isChanged && curMemList.size() > 0)
    {
        // Ring has changed
        
        // Update the ring - assign the new membership list
        ring = curMemList;
        
        // Run Stabilization Protocol
        stabilizationProtocol();
    
    }
    
}

/**
 * FUNCTION NAME: getMemberhipList
 *
 * DESCRIPTION: This function goes through the membership list from the Membership protocol/MP1 and
 * 				i) generates the hash code for each member
 * 				ii) populates the ring member in MP2Node class
 * 				It returns a vector of Nodes. Each element in the vector contain the following fields:
 * 				a) Address of the node
 * 				b) Hash code obtained by consistent hashing of the Address
 */
vector<Node> MP2Node::getMembershipList() {
	unsigned int i;
	vector<Node> curMemList;
	for ( i = 0 ; i < this->memberNode->memberList.size(); i++ ) {
		Address addressOfThisMember;
		int id = this->memberNode->memberList.at(i).getid();
		short port = this->memberNode->memberList.at(i).getport();
		memcpy(&addressOfThisMember.addr[0], &id, sizeof(int));
		memcpy(&addressOfThisMember.addr[4], &port, sizeof(short));
		curMemList.emplace_back(Node(addressOfThisMember));
	}
	return curMemList;
}

/**
 * FUNCTION NAME: hashFunction
 *
 * DESCRIPTION: This functions hashes the key and returns the position on the ring
 * 				HASH FUNCTION USED FOR CONSISTENT HASHING
 *
 * RETURNS:
 * size_t position on the ring
 */
size_t MP2Node::hashFunction(string key) {
	std::hash<string> hashFunc;
	size_t ret = hashFunc(key);
	return ret%RING_SIZE;
}


/*
 * Intitializes the transaction Information
 */
void MP2Node::setTransaction(int transId,string key,string value,MessageType type)
{
    map<int,transaction_info>::iterator it = transactions.find(transId);
    
    //Check if the transaction Id is already used/present
    if(it == transactions.end())
    {
        transactions[transId].replies = 0;
        transactions[transId].key = key;
        transactions[transId].value = value;
        transactions[transId].type = type;
        transactions[transId].timestamp = par->getcurrtime();
        transactions[transId].read_value_timestamp = 0;
    }
}

/**
 * FUNCTION NAME: clientCreate
 *
 * DESCRIPTION: client side CREATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientCreate(string key, string value) {

    //Find the replicas where the key has to be stored
    vector<Node> replicas = findNodes(key);
    
    
    //Construct and Send CREATE Message to PRIMARY replica
    Message* m = new Message(transactionId,this->memberNode->addr,CREATE,key,value, PRIMARY);
    string msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[0].getAddress(), msg);
    
    //Construct and Send CREATE Message to SECONDARY replica
    m = new Message(transactionId,this->memberNode->addr,CREATE,key,value, SECONDARY);
    msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[1].getAddress(), msg);
    
    //Construct and Send CREATE Message to TERTIARY replica
    m = new Message(transactionId,this->memberNode->addr,CREATE,key,value, TERTIARY);
    msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[2].getAddress(), msg);
    
    //Initialize Transaction
    setTransaction(transactionId,key,value,CREATE);
    
    //Increment transaction Id
    transactionId++;
    
}

/**
 * FUNCTION NAME: clientRead
 *
 * DESCRIPTION: client side READ API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientRead(string key){
    
    //Find the replicas where the key is stored
    vector<Node> replicas = findNodes(key);

    //Construct and Send READ Message to PRIMARY replica
    Message* m = new Message(transactionId, getMemberNode()->addr, READ, key);
    string msg = m->toString();
    emulNet->ENsend(&getMemberNode()->addr, replicas[0].getAddress(), msg);
    
    //Construct and Send READ Message to SECONDARY replica
    m = new Message(transactionId, this->memberNode->addr, READ, key);
    msg = m->toString();
    emulNet->ENsend(&getMemberNode()->addr, replicas[1].getAddress(), msg);
    
    //Construct and Send READ Message to TERTIARY replica
    m = new Message(transactionId, this->memberNode->addr, READ, key);
    msg = m->toString();
    emulNet->ENsend(&getMemberNode()->addr, replicas[2].getAddress(), msg);
    
    //Set transaction values
    setTransaction(transactionId,key,"",READ);
    
    //Increment transaction id
    transactionId++;
}

/**
 * FUNCTION NAME: clientUpdate
 *
 * DESCRIPTION: client side UPDATE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientUpdate(string key, string value){
	
    //Find the replicas where the key is stored
    vector<Node> replicas = findNodes(key);

    //Construct and Send UPDATE Message to PRIMARY replica
    Message* m = new Message(transactionId,this->memberNode->addr,UPDATE,key,value, PRIMARY);
    string msg = m->toString();
    int ret = emulNet->ENsend(&this->memberNode->addr, replicas[0].getAddress(), msg);
    
    //Construct and Send UPDATE Message to SECONDARY replica
    m = new Message(transactionId,this->memberNode->addr,UPDATE,key,value, SECONDARY);
    msg = m->toString();
    ret = emulNet->ENsend(&this->memberNode->addr, replicas[1].getAddress(), msg);
    
    //Construct and Send UPDATE Message to TERTIARY replica
    m = new Message(transactionId,this->memberNode->addr,UPDATE,key,value, TERTIARY);
    msg = m->toString();
    ret = emulNet->ENsend(&this->memberNode->addr, replicas[2].getAddress(), msg);
    
    //Initialize Transaction
    setTransaction(transactionId,key,value,UPDATE);
    
    //Increment transaction Id
    transactionId++;
    
}

/**
 * FUNCTION NAME: clientDelete
 *
 * DESCRIPTION: client side DELETE API
 * 				The function does the following:
 * 				1) Constructs the message
 * 				2) Finds the replicas of this key
 * 				3) Sends a message to the replica
 */
void MP2Node::clientDelete(string key){
    
    //Find the replicas where the key is stored
    vector<Node> replicas = findNodes(key);
    
    //Construct and Send DELETE Message to PRIMARY replica
    Message* m = new Message(transactionId, this->memberNode->addr, DELETE, key);
    string msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[0].getAddress(), msg);
    
    //Construct and Send DELETE Message to SECONDARY replica
    m = new Message(transactionId, this->memberNode->addr, DELETE, key);
    msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[1].getAddress(), msg);
    
    //Construct and Send DELETE Message to TERTIARY replica
    m = new Message(transactionId, this->memberNode->addr, DELETE, key);
    msg = m->toString();
    emulNet->ENsend(&this->memberNode->addr, replicas[2].getAddress(), msg);
    
    //Initialize transaction values
    setTransaction(transactionId,key,"",DELETE);
    
    //Increment transaction Id
    transactionId++;
}

/**
 * FUNCTION NAME: createKeyValue
 *
 * DESCRIPTION: Server side CREATE API
 * 			   	The function does the following:
 * 			   	1) Inserts key value into the local hash table
 * 			   	2) Return true or false based on success or failure
 */
bool MP2Node::createKeyValue(string key, string value, ReplicaType replica) {

	// Insert key, value, replicaType into the hash table
    Entry* en = new Entry(value, par->getcurrtime(),replica);
    string entry = en->convertToString();
    bool isSuccess = ht->create(key,entry);
    free(en);
    return isSuccess;
}

/**
 * FUNCTION NAME: readKey
 *
 * DESCRIPTION: Server side READ API
 * 			    This function does the following:
 * 			    1) Read key from local hash table
 * 			    2) Return value
 */
string MP2Node::readKey(string key) {

	// Read key from local hash table and return value;
    string entry = ht->read(key);
    return entry;
}

/**
 * FUNCTION NAME: updateKeyValue
 *
 * DESCRIPTION: Server side UPDATE API
 * 				This function does the following:
 * 				1) Update the key to the new value in the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::updateKeyValue(string key, string value, ReplicaType replica) {

	// Update key in local hash table and return true or false
    Entry en(value, par->getcurrtime(),replica);
    string entry = en.convertToString();
    bool isSuccess = ht->update(key,entry);
    return isSuccess;
}

/**
 * FUNCTION NAME: deleteKey
 *
 * DESCRIPTION: Server side DELETE API
 * 				This function does the following:
 * 				1) Delete the key from the local hash table
 * 				2) Return true or false based on success or failure
 */
bool MP2Node::deletekey(string key) {

	// Delete the key from the local hash table
    bool isSuccess = ht->deleteKey(key);
    return isSuccess;
}

/*
 *This function handles Read Reply messages
 */
void MP2Node::handleReadReplyMessage(Message* msg)
{
    int transId = msg->transID;
    string value = msg->value;

    Address from_Addr(msg->fromAddr);
    
    //Check if Read was successful, by checking if the read value is non empty
    if(value.compare("") != 0)
    {
        transactions[transId].replies++;
        Entry* em = new Entry(value);
        
        //Check if the current read value has larger timestamp than previous value read for the key
        if(em->timestamp > transactions[transId].read_value_timestamp)
        {
            //Store the read value with latest timestamp
            transactions[transId].value = em->value;
            
            //Store the timestamp of the read value
            transactions[transId].read_value_timestamp = em->timestamp;
        }
        
        free(em);
    }

}

void MP2Node::handleUpdateMessage(Message* msg)
{
    //Update the hash table
    bool isSuccess = updateKeyValue(msg->key,msg->value,msg->replica);
    int tid = msg->transID;
    
    if(tid < 0)
    {
        //Handles cases when update was sent for stabilization protocol with transaction id of -100 < 0
        return;
    }
    
    if(isSuccess)
    {
        //log update success
        log->logUpdateSuccess(&getMemberNode()->addr, false, tid, msg->key, msg->value);
    }
    else
    {
        //log update failure
        log->logUpdateFail(&getMemberNode()->addr, false, tid, msg->key, msg->value);
    }
    
    //Construct and Send UPDATE success or failure message to the coordinator
    Message* m = new Message(msg->transID,this->memberNode->addr,REPLY,isSuccess);
    string msg1 = m->toString();
    Address to_address(msg->fromAddr);
    emulNet->ENsend(&memberNode->addr, &to_address, msg1);
    free(m);
}

void MP2Node::handleReadMessages(Message* msg)
{
    //Read the key's value from the hashtable
    string value = readKey(msg->key);
    int transId = msg->transID;
    
    if(transId < 0)
    {
        //Handles cases when read was sent for stabilization protocol with transaction id of -100 < 0
        return;
    }

    if(value.compare("") != 0)
    {
        // log READ success
        log->logReadSuccess(&getMemberNode()->addr, false, transId, msg->key, value);
    }
    else
    {
        // log READ failure
        log->logReadFail(&getMemberNode()->addr, false, transId, msg->key);
    }
    
    //Construct and Send READ success or failure message to the coordinator
    Message* m = new Message(msg->transID,this->memberNode->addr,value);
    string msg1 = m->toString();
    Address to_address(msg->fromAddr);
    emulNet->ENsend(&memberNode->addr, &to_address, msg1);
    free(m);
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: This function is the message handler of this node.
 * 				This function does the following:
 * 				1) Pops messages from the queue
 * 				2) Handles the messages according to message types
 */
void MP2Node::checkMessages() {

	char * data;
	int size;

	// dequeue all messages and handle them
	while ( !memberNode->mp2q.empty() ) {
		/*
		 * Pop a message from the queue
		 */
		data = (char *)memberNode->mp2q.front().elt;
		size = memberNode->mp2q.front().size;
		memberNode->mp2q.pop();

		//Construct the message received from the queue
        string message(data, data + size);
        Message* msg = new Message(message);
        
        if(msg->type == CREATE)
        {
            //Handle CREATE messages
            
            // Create the Key-Value pair
            bool isSuccess = createKeyValue(msg->key, msg->value, msg->replica);
            int tid = msg->transID;
            
            if(tid >= 0)
                {
                    if(isSuccess)
                    {
                        // log CREATE success
                        log->logCreateSuccess(&getMemberNode()->addr, false, tid, msg->key, msg->value);
                    }
                    else
                    {
                        // log CREATE failure
                        log->logCreateFail(&getMemberNode()->addr, false, tid, msg->key, msg->value);
                    }
            
                    //Construct and send REPLY message to the coordinator
                    Message* m = new Message(msg->transID,this->memberNode->addr,REPLY,isSuccess);
                    string msg1 = m->toString();
                    Address to_address(msg->fromAddr);
                    emulNet->ENsend(&memberNode->addr, &to_address, msg1);
                    free(m);
                }
        }
        else if(msg->type == REPLY)
        {
            //Handle REPLY messages
            
            int transId = msg->transID;
            bool success = msg->success;
            
            Address from_Addr(msg->fromAddr);
            
            if(success)
            {
                //Increment the number of replies received for the transaction - at Coordinator
                transactions[transId].replies++;
            }

        }
        else if(msg->type == DELETE)
        {
            //Handle DELETE messages
            
            //Delete the key
            bool isSuccess = deletekey(msg->key);
            int transId = msg->transID;
            
            if(transId >= 0)
            {
            
                if(isSuccess)
                {
                    // log DELETE success
                    log->logDeleteSuccess(&getMemberNode()->addr, false, transId, msg->key);
                }
                else
                {
                    // log DELETE failure
                    log->logDeleteFail(&getMemberNode()->addr, false, transId, msg->key);
                }
            
                //Construct and send REPLY message to the coordinator
                Message* m = new Message(msg->transID,this->memberNode->addr,REPLY,isSuccess);
                string msg1 = m->toString();
                Address to_address(msg->fromAddr);
                emulNet->ENsend(&memberNode->addr, &to_address, msg1);
                free(m);
            }
        }
        else if(msg->type == READ)
        {
            //Handle READ messages
            
            handleReadMessages(msg);
        }
        else if(msg->type == READREPLY)
        {
            //Handle READREPLY messages
            
            handleReadReplyMessage(msg);
        }
        else if(msg->type == UPDATE)
        {
            //Handle UPDATE messages
            
            handleUpdateMessage(msg);
        }
        
        free(msg);

	}

    map<int,transaction_info>::iterator it;
    vector<int> transactions_to_delete;
    int current_time = par->getcurrtime();
    for(it = transactions.begin(); it != transactions.end(); it++)
    {
            if(it->second.replies < 2)
            {
                //If Quorum is not received
                
                //Wait for a timeout of 5 global time before logging coordinator failure
                if ((current_time - it->second.timestamp) >= 5)
                {
                    if(this->transactions[it->first].type == DELETE)
                    {
                        // log DELETE failure
                        log->logDeleteFail(&getMemberNode()->addr, true, it->first, it->second.key);
                        transactions_to_delete.push_back(it->first);
                    }
                    else if(this->transactions[it->first].type == READ)
                    {
                        // log READ failure
                        log->logReadFail(&getMemberNode()->addr, true, it->first, it->second.key);
                        transactions_to_delete.push_back(it->first);
                    }
                    else if(this->transactions[it->first].type == UPDATE)
                    {
                        // log UPDATE failure
                        log->logUpdateFail(&getMemberNode()->addr, true, it->first, it->second.key, it->second.value);
                        transactions_to_delete.push_back(it->first);
                    }
                }
            }
            else
            {
                //If Quorum replies are received
                
                if(this->transactions[it->first].type == CREATE)
                {
                    // log CREATE success
                    log->logCreateSuccess(&getMemberNode()->addr, true, it->first, it->second.key, it->second.value);
                }
                else if(this->transactions[it->first].type == DELETE)
                {
                    // log DELETE success
                    log->logDeleteSuccess(&getMemberNode()->addr, true, it->first, it->second.key);
                }
                else if(this->transactions[it->first].type == READ)
                {
                    // log READ success
                    log->logReadSuccess(&getMemberNode()->addr, true, it->first, it->second.key, it->second.value);
                }
                else if(this->transactions[it->first].type == UPDATE)
                {
                    // log UPDATE success
                    log->logUpdateSuccess(&getMemberNode()->addr, true, it->first, it->second.key, it->second.value);
                }
                transactions_to_delete.push_back(it->first);
            }
    }
    
    for (int i = 0; i < transactions_to_delete.size(); i++)
    {
        
        //Delete the transactions which have failed or succeeded from the list
        transactions.erase(transactions_to_delete[i]);
        
    }

}

/**
 * FUNCTION NAME: findNodes
 *
 * DESCRIPTION: Find the replicas of the given keyfunction
 * 				This function is responsible for finding the replicas of a key
 */
vector<Node> MP2Node::findNodes(string key) {
	size_t pos = hashFunction(key);
	vector<Node> addr_vec;
	if (ring.size() >= 3) {
		// if pos <= min || pos > max, the leader is the min
		if (pos <= ring.at(0).getHashCode() || pos > ring.at(ring.size()-1).getHashCode()) {
			addr_vec.emplace_back(ring.at(0));
			addr_vec.emplace_back(ring.at(1));
			addr_vec.emplace_back(ring.at(2));
		}
		else {
			// go through the ring until pos <= node
			for (int i=1; i<ring.size(); i++){
				Node addr = ring.at(i);
				if (pos <= addr.getHashCode()) {
					addr_vec.emplace_back(addr);
					addr_vec.emplace_back(ring.at((i+1)%ring.size()));
					addr_vec.emplace_back(ring.at((i+2)%ring.size()));
					break;
				}
			}
		}
	}
	return addr_vec;
}


/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: Receive messages from EmulNet and push into the queue (mp2q)
 */
bool MP2Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), this->enqueueWrapper, NULL, 1, &(memberNode->mp2q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue of MP2Node
 */
int MP2Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: stabilizationProtocol
 *
 * DESCRIPTION: This runs the stabilization protocol in case of Node joins and leaves
 * 				It ensures that there always 3 copies of all keys in the DHT at all times
 * 				The function does the following:
 *				1) Ensures that there are three "CORRECT" replicas of all the keys in spite of failures and joins
 *				Note:- "CORRECT" replicas implies that every key is replicated in its two neighboring nodes in the ring
 */
void MP2Node::stabilizationProtocol() {
	
    vector<Node> newHaveReplicasOf;
    vector<Node> newHasMyReplicas;
    
    //Find the new Neighbours - HaveMyReplicas and HasReplicasOf
    for(int loop = 0; loop < ring.size(); loop++)
    {
        if((memcmp(ring.at(loop).getAddress()->addr, &this->memberNode->addr,sizeof(Address)) == 0))
        {
            
            if(loop == 0)
            {
                newHaveReplicasOf.push_back(ring.at(ring.size() - 1));
                newHaveReplicasOf.push_back(ring.at(ring.size() - 2));
            }
            else if(loop == 1)
            {
                newHaveReplicasOf.push_back(ring.at(0));
                newHaveReplicasOf.push_back(ring.at(ring.size() - 1));
            }
            else
            {
                newHaveReplicasOf.push_back(ring.at(loop-1));
                newHaveReplicasOf.push_back(ring.at(loop-2));
            }
            
            newHasMyReplicas.push_back(ring.at((loop+1)%ring.size()));
            newHasMyReplicas.push_back(ring.at((loop+2)%ring.size()));
        }
    }
    
    
    //Compare old and new HasReplicas
    for(int loop1 = 0; loop1 < hasMyReplicas.size(); loop1++)
    {
        if(memcmp(hasMyReplicas.at(loop1).getAddress()->addr, newHasMyReplicas.at(loop1).getAddress()->addr,sizeof(Address)) != 0)
        {

            //If hasMyReplica has changed
            
            // Fetch the keys for which the node is PRIMARY as it will have to replicate them again
            vector<string> primaryKeys = findMyKeys(PRIMARY);
            ReplicaType type;
            
            // Find out which Replica SECONDARY or TERTIARY for the node's primary key has failed
            type = loop1 == 0 ? SECONDARY : TERTIARY;

            //Check if the new node new neighbour is there in my old hasReplicas
            bool isNeighbour = isOldNeighbour(hasMyReplicas, newHasMyReplicas.at(loop1));
            
            if(!isNeighbour)
            {
             //If not there in old HasReplicas, this means it is a completely new neighbour and the nodes primary keys will have to be created on this replica node
                
                //Loop through all the primary keys and send CREATE message on the new neighbor
                for(int loop2 = 0; loop2 < primaryKeys.size(); loop2++)
                {
                    string key = primaryKeys.at(loop2);
                    string value = ht->hashTable[key];
                    Entry e(value);
                    value = e.value;
                
                    //Send CREATE message to the new Replica
                    Message* m = new Message(-999,this->memberNode->addr,CREATE,key,value, type);
                    string msg = m->toString();
                    emulNet->ENsend(&this->memberNode->addr, newHasMyReplicas.at(loop1).getAddress(), msg);
                    free(m);
                }
            }
            else
            {
                //If there in new HasReplicas, this means it is not a new neighbor, but its type has changed from SECONDARY -> TERTIARY, OR TERTIARY -> SECONDARY
                
                //Loop through all the primary keys and send UPDATE message on the new neighbor to update replica TYPE for the neighbor
                for(int loop2 = 0; loop2 < primaryKeys.size(); loop2++)
                {
                    string key = primaryKeys.at(loop2);
                    string value = ht->hashTable[key];
                    Entry e(value);
                    value = e.value;
                    
                    //Send UPDATE message to the new Replica
                    Message* m = new Message(-999,this->memberNode->addr,UPDATE,key,value, type);
                    string msg = m->toString();
                    emulNet->ENsend(&this->memberNode->addr, newHasMyReplicas.at(loop1).getAddress(), msg);
                    free(m);
                }
            }
            
        }
    }
    
    //Re-assign the new neighbors
    hasMyReplicas  = newHasMyReplicas;
    haveReplicasOf = newHaveReplicasOf;
}

vector<string> MP2Node::findMyKeys(ReplicaType type)
{
    //Return those keys from the hashtable for which nodes REPLICATYPE is 'type'
    map<string,string>::iterator it = ht->hashTable.begin();
    vector<string> myKeys;
    
    while(it != ht->hashTable.end())
    {
        string value = it->second;
        Entry e(value);
        
        if(e.replica == type)
        {
            myKeys.push_back(it->first);
        }
        it++;
    }
    
    return myKeys;
}

//Check if the node is present in the vector<Node> passed
bool MP2Node::isOldNeighbour(vector<Node> nodes, Node node)
{
    vector<Node>::iterator it = nodes.begin();
    bool nodeExists = false;
    while(it != nodes.end())
    {
        if(memcmp(it->getAddress()->addr,node.getAddress()->addr,sizeof(Address)) == 0)
        {
            nodeExists = true;
            break;
        }
        it++;
    }
    
    return nodeExists;
}

//Check if the node is alive
bool MP2Node::isNodeAlive(Node node)
{
    vector<Node>::iterator it = ring.begin();
    bool nodeAlive = false;
    
    while(it != ring.end())
    {
        if(memcmp(it->getAddress()->addr,node.getAddress()->addr,sizeof(Address)) == 0)
        {
            nodeAlive = true;
            break;
        }
        it++;
    }
    
    return nodeAlive;
    
}

