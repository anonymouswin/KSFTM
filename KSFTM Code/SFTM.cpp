#include "SFTM.h"
#define OK true
#define ABORTED false
#define NIL -1
#define TRUE true
#define FALSE false
#define ZERO 0
#define ONE 1
#define INFINITE 99999999;
/**************************** CONSTRUCTORS *****************************/
/*
 * Global Transaction(GTransaction) class constructor
 * */
GTransaction::GTransaction()
{
	g_valid = TRUE;
	g_lock = new mutex;
}

/*
 * Transaction object(Tobj) class constructor
 * */
Tobj::Tobj()
{
	tobj_lock = new mutex;
}

/*
 * Constructor of the class SFTM which performs the initialize operation. 
 * Invoked at the start of the STM system. Initializes all the tobjs used by the STM System.
 * */
SFTM::SFTM(int INITIAL_objs)
{
	g_tCntr.store(ONE);
	
	// For all the tobjs used by the STM System
	for(int i=ZERO;i<INITIAL_objs;i++) {
		
		/*Create transaction object and push it in the 
			transaction object set*/	
		Tobj *tobj = new Tobj;
		tobj->val = ZERO;
			
		//Push the transaction object created on the transaction objects list	
		tobjs->push_back(*tobj); 
	}
}

/************************ SFTM::PRIVATE METHODS ***********************/

/*
 * Method to search for a transaction object in the 'set' passes 
 * as an argument to the function. 
 * */	
bool SFTM::find_set(vector<TobIdValPair> *set, TobIdValPair* tobj_id_val_pair)
{
	if(tobj_id_val_pair != NULL && set != NULL) {
		for(int i = ZERO;i<set->size();i++) {
			//Check if transaction object of 'tobj_id_val_pair->id' present in the set
			if(set->at(i).id == tobj_id_val_pair->id) {
				//set the value corresponding to the tobject in the 'tobj_id_val_pair' instance pointer.
				tobj_id_val_pair->val = set->at(i).val;
				//return TRUE
				return TRUE;
			}
		}
	}
	//else return FALSE
	return FALSE;
}

/*
 * Insert a transaction in the reader's list of a transaction object.
 * */
void SFTM::insertAndSortRL(list<GTransaction*> *RL, GTransaction* gtran)
{
	GTransaction *gtran_iterator;
	list<GTransaction*>::iterator gtran_list_iterator;
	
	if(gtran != NULL && RL != NULL) {
		//If the reader's list is empty push the gtrans to the reader's list
		if(RL->size() == ZERO) {
			RL->push_back(gtran);
			return;
		}
		gtran_list_iterator = RL->begin();
		while(gtran_list_iterator != RL->end())
		{
			gtran_iterator = *gtran_list_iterator;
			/*if reader list's transaction cts value is greater than current 
				transaction's cts value, then insert the transaction*/
			if(gtran->g_cts < gtran_iterator->g_cts) {
				RL->insert(gtran_list_iterator,gtran);
				return;
			} else if(gtran->g_cts == gtran_iterator->g_cts) {
				return;
			}
			gtran_list_iterator++;
		}
		//if not yet inserted in the reader's list, then insert it in the last of the list
		RL->push_back(gtran);
	}
}

/*
 * Find the lowest its value amongst all the trasaction of the 
 * list passed as an argument to the function
 * */
long int SFTM::findLLTS(list<GTransaction*> *TSet)
{
	//holds the minimum its
	long int min_its;
	GTransaction *gtran_iterator;
	list<GTransaction*>::iterator gtran_list_iterator;
	
	gtran_list_iterator = TSet->begin();
	min_its = INFINITE;													//Not necessarily be LIVE ###########
		while(gtran_list_iterator != TSet->end())
		{
			gtran_iterator = *gtran_list_iterator;
			if(gtran_iterator->g_its < min_its && gtran_iterator->g_state == LIVE) {
				min_its = gtran_iterator->g_its;
			}
			gtran_list_iterator++;
		}
	return min_its;
}

/*
 * Method used to release all the locks acquired by the transaction on any,
 * transaction objects or other transactions. 
 * */
void SFTM::unlockAll(LTransaction *ltrans)
{
	GTransaction *gtran_iterator;
	list<GTransaction*>::iterator gtran_list_iterator;
	long int tranObj_iterator;
	list<long int>::iterator transObj_list_iterator;
	
	/*Check if the transaction has attain lock on any other transaction,
		if yes then release those locks*/
	if(ltrans->trans_locked->size() != ZERO) {
		gtran_list_iterator = ltrans->trans_locked->begin();
		while(gtran_list_iterator != ltrans->trans_locked->end())
		{
			gtran_iterator = *gtran_list_iterator;
			gtran_iterator->g_lock->unlock();
			++gtran_list_iterator;
		}
	}
	
	/*Check if the transaction has attain lock on any of the transaction objects,
		if yes then realse all those locks too.*/
	if(ltrans->tobjs_locked->size() != ZERO) {
		transObj_list_iterator = ltrans->tobjs_locked->begin();
		while(transObj_list_iterator != ltrans->tobjs_locked->end())
		{
			tranObj_iterator = *transObj_list_iterator;
			tobjs->at(tranObj_iterator).tobj_lock->unlock();
			++transObj_list_iterator;
		}
	}
	//clear the transaction and transactions object's lists
	ltrans->trans_locked->clear();
	ltrans->tobjs_locked->clear();
}

/************************ SFTM::PUBLIC METHODS ***********************/
/*
 * Invoked by a thread to start a new transaction. Thread can pass a parameter 'its'
 * which is the initial timestamp when this transaction was invoked for the 
 * first time. If this is the first invocation then 'its' is NIL.
 * */
LTransaction* SFTM::tbegin(long int its) {
	LTransaction *ltrans = new LTransaction;
	ltrans->id = g_tCntr.fetch_add(ONE);
	
	// If this is the first invocation		
	if(its == NIL)	{
			ltrans->g_its = ltrans->g_cts = ltrans->id;
	} 
	// Invocation by an aborted transaction
	else {
		ltrans->g_its = its;
		ltrans->g_cts = ltrans->id;						
	}
	//set status and valid of the transaction		
	ltrans->g_state = LIVE;
	ltrans->g_valid = TRUE;
	
	//return local transaction pointer reference		
	return ltrans;
}

/*
 * Invoked by a transaction T to read transaction object value.
 * ltrans - local transaction object
 * return status of the read transaction(OK/ABORTED)
 * 
 * */
bool SFTM::stmRead(LTransaction* ltrans, TobIdValPair *tobj_id_val_pair)														
{	
	GTransaction *gtrans = ltrans;
	
	
	/*To check whether transaction object with tobj_id 
	  is present in the writer's set of the transaction*/	
	if(find_set(ltrans->write_set, tobj_id_val_pair) == TRUE) {
		return OK;
	} 
	
	/*To check whether transaction object with tobj_id 
	  is present in the reader's set of the transaction*/
	if(find_set(ltrans->read_set,tobj_id_val_pair) == TRUE) {
		return OK;
	}
	
	//Attain lock on transaction object
	tobjs->at(tobj_id_val_pair->id).tobj_lock->lock();
	ltrans->tobjs_locked->push_back(tobj_id_val_pair->id);
	//Attain lock on current transaction
	ltrans->g_lock->lock();
	ltrans->trans_locked->push_back(gtrans);
	
	//Abort the transaction is transaction's valid value is FALSE
	if(ltrans->g_valid == FALSE) {
	if(stmAbort(ltrans) == OK) {
			return ABORTED;
		}
	}
	
	//Find available value of the transaction object
	tobj_id_val_pair->val = tobjs->at(tobj_id_val_pair->id).val;
		
	//Add the transaction object id and value pair to the reader's list	
	ltrans->read_set->push_back(*tobj_id_val_pair);
	
	//Add transaction to transaction object's reader's list
	insertAndSortRL(tobjs->at(tobj_id_val_pair->id).rl,gtrans);
		
	//Unlock the transaction and unlock the transaction object
	unlockAll(ltrans);
	
	//return OK
	return OK;
}	

/*
 * A Transaction T writes into its local memory - 'write_set'
 * ltrans - local transaction object
 * tobj_id - transaction object id
 * val - value to be updated
 * transaction_status - denotes status of the read transaction(SUCCESS/ABOTED)
 * */
bool SFTM::stmWrite(LTransaction* ltrans, TobIdValPair* tobj_id_val_pair)
{
	//Flag to check weather the new transaction <id,val> pair is inserted or not
	bool flag = FALSE;
	
	//if write set of the transaction is empty insert the T<id,val> pair
	if(ltrans->write_set->size() == ZERO) {
		ltrans->write_set->push_back(*tobj_id_val_pair);
		return OK;
	}
	//insert the T<id,val> pair and maintain the sorted order of ids of the transaction objects <id,val> pairs
	
	vector<TobIdValPair>::iterator it;
	for(it=ltrans->write_set->begin();it<ltrans->write_set->end();it++)
	{
		if((*it).id == tobj_id_val_pair->id) {
			it = ltrans->write_set->erase(it);
			flag = TRUE;
			break;
		} else if((*it).id > tobj_id_val_pair->id) {
			flag = TRUE;
			break;
		}
	}
	/*If the transaction object id is the lartgest amongst the transaction 
	 objects already present in the writer's set of the transaction, insert the TobjIdValPair in the last.*/
	if(flag == FALSE) {
		ltrans->write_set->push_back(*tobj_id_val_pair);
	} else {
		ltrans->write_set->insert(it,*tobj_id_val_pair);
	}
	return OK;
}

/*
 * Returns OK on commit else return ABORTED.
 * Method try to commit all the write operation stored in the, 
 * write set of the transaction.
 * */
bool SFTM::stmTryCommit(LTransaction* ltrans)
{
	list<GTransaction*> *TSet = new list<GTransaction*>;
	//list<GTransaction*>::iterator itr;
	
	GTransaction *gtrans = ltrans;
	GTransaction *gtran_iterator;
	list<GTransaction*>::iterator gtran_list_iterator;
	long int objId, wSet_size;
	list<long int>::iterator ver_iterator;
	
	//Optimization check for validaity of the transaction	
	ltrans->g_lock->lock();
	ltrans->trans_locked->push_back(gtrans);
	if(ltrans->g_valid == FALSE) {
		if(stmAbort(ltrans) == OK) {
			return ABORTED;
		}
	}
	ltrans->trans_locked->clear();
	ltrans->g_lock->unlock();
		
	//lock all transaction objects:x belongs to write_set of the transaction in pre-defined order.
	wSet_size = ltrans->write_set->size();
	for(int i = ZERO;i < wSet_size;i++) {
		objId = ltrans->write_set->at(i).id;
		tobjs->at(objId).tobj_lock->lock();
		ltrans->tobjs_locked->push_back(objId);
		
		/*store all the reader transaction present in the reader's list 
		 * 	of the transaction objects in the local list TSet.*/
		gtran_list_iterator = tobjs->at(objId).rl->begin();
		while(gtran_list_iterator != tobjs->at(objId).rl->end())
		{
			gtran_iterator = *gtran_list_iterator;
			if(gtran_iterator->g_valid == ABORTED) {
				gtran_list_iterator = tobjs->at(objId).rl->erase(gtran_list_iterator);
				//Remove transaction from reader's list
			} else {
				//insert all the reader transactions in the TSet in a sorted order
				insertAndSortRL(TSet,gtran_iterator);
			}
			gtran_list_iterator++;
		}				
	}
	//insert current transaction in the TSET without disturbing the sorted order
	insertAndSortRL(TSet,gtrans);
	
	/*Attain lock on all the transactions present in the reader's list of 
	 * all the transaction objects, present in the write_set of current transaction*/
	gtran_list_iterator = TSet->begin();
	while(gtran_list_iterator != TSet->end())
	{
		gtran_iterator = *gtran_list_iterator;
		gtran_iterator->g_lock->lock();
		ltrans->trans_locked->push_back(gtran_iterator);
		gtran_list_iterator++;
	}
	
	//Abort the transaction if its g_valid is FALSE
	if(ltrans->g_valid == FALSE) {
		if(stmAbort(ltrans) == OK) {
			return ABORTED;
		}
	} else {
		//if current transaction holds the minimum its value amongst all the reader transaction in TSet
		if(ltrans->g_its == findLLTS(TSet)) {
			//if yes, then set g_valid = FALSE of each transaction and release the lock
			gtran_list_iterator = TSet->begin();
			while(gtran_list_iterator != TSet->end())
			{
				gtran_iterator = *gtran_list_iterator;
				if(gtran_iterator->g_cts != ltrans->g_cts) {
					if(gtran_iterator->g_state == LIVE) {
						gtran_iterator->g_valid = FALSE;
					}
					gtran_iterator->g_lock->unlock();
				}
				gtran_list_iterator++;
			}
			/*clear the trans_locked list and just add current transaction
				to it as all other transaction's locks are released*/
			ltrans->trans_locked->clear();
			ltrans->trans_locked->push_back(gtrans);
		} else {
			//Abort the current transaction
			if(stmAbort(ltrans) == OK) {
				return ABORTED;
			}
		}
	}
	//Replace all the values of the transaction objects which are in write list with new values.
	for(int i = ZERO;i<ltrans->write_set->size();i++) {
		long int objId = ltrans->write_set->at(i).id;
		tobjs->at(objId).val = ltrans->write_set->at(i).val;
		tobjs->at(objId).rl->clear();
	}
	//change the state of the transaction to COMMIT
	ltrans->g_state = COMMIT;
	
	//unlock all the variables
	unlockAll(ltrans);
	
	//return OK
	return OK;
}

/*
 * Invoked by various STM methods to abort transaction 'trans', passed as an 
 * argument to the function.;
 * */
bool SFTM::stmAbort(LTransaction* ltrans)
{
	if(ltrans != NULL) {
		//set the transaction's valid value as false and state as abort
		ltrans->g_valid = FALSE;
		ltrans->g_state = ABORT;
		
		//unlock all the variables
		unlockAll(ltrans);
		//Return OK status		
		return OK;
	}
	return ABORTED;
}
