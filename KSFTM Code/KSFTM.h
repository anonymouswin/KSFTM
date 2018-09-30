#include <vector>
#include <list>
#include <atomic>
#include <cstring>
#include <mutex>
#include <algorithm>
#include <iterator>
#include <iostream>

using namespace std;

/*
 * Enum that defines the transaction states that can be - ABORT/LIVE/COMMIT
 * */
enum Transactionstate{LIVE,COMMIT,ABORT};

/*
 * Class that encapsulates transaction object id and its value for a transaction 
 * */
class TobIdValPair
{
	public:
	long int id;
	long int val;
};


/*
 * Global Transaction class : instances of this class class are stored in the
 * readers list of the versions of the transaction objects.
 * */
class GTransaction
{
	//Public members of the class accessible to all.
	public:
	//Transaction ID
	int id;	
	//initial timestamp
	long int g_its;
	//current timestamp
	long int g_cts;
	//working timestamp
	long int g_wts;
	//transaction lower time limit
	long int g_tltl;
	//transaction upper timelimit
	long int g_tutl;
	//Flag which is initially true and is false when transaction is aborted
	bool g_valid;
	//transaction objects locked by the current transaction
	list<long int> *tobjs_locked = new list<long int>;
	//transactions locked by the current transaction
	list<GTransaction*> *trans_locked = new list<GTransaction*>;
	//transation state - ABORT/LIVE/COMMIT
	Transactionstate g_state;
	//transaction specific lock
	mutex* g_lock;
	//Constructor
	GTransaction();
	//Private member of the not class.
	private:
	//transaction commit time
	long int comTime;
	//readers list local to transaction
	vector<TobIdValPair> *r_set = new vector<TobIdValPair>;
	//writers list local to transaction
	vector<TobIdValPair> *w_set = new vector<TobIdValPair>;
	//friend class local transaction to acces the private members of this class
	friend class LTransaction;
	
};

/*
 * Local Transaction class inheriting Global transation class in a friendly way
 * */
class LTransaction : public GTransaction
{
	//public members of the class
	public:
	//transaction commit time
	long int comTime = comTime;
	//Transaction's local readers list
	vector<TobIdValPair> *read_set = r_set;
	//Transactiobn's local writers list
	vector<TobIdValPair> *write_set = w_set;
};
/*
 * class that define structure of a Version of a transaction object
 * */
class Version
{
	//public members of the class
	public:
	//working timestamp of the transaction that creates the version
	long int wts;
	//current timestamp of the transaction that creates the version
	long int cts;
	//transaction object's value
	long int val;
	//transaction object's vrt
	long int vrt;
	//list of all transactions that have read value of the transaction object from this version
	list<GTransaction*> *rl = new list<GTransaction*>;							
};

/*
 * Stucture of a transaction object
 * */
class Tobj
{
	//public members of the class
	public:
	//maintains the count of K for the transaction object
	long int k;
	//List of versions of the transaction object
	list<Version*> *versionList = new list<Version*>;
	//transcation object lock
	mutex* tobj_lock;
	//constuctor
	Tobj();
};


/*
 * STM class that provides the shared memory to all the transactions
 * */
class STM
{
	//public member variables of the class
	public:
	//Atomic global transaction counter
	atomic<long int> g_tCntr;
	//list of all the transaction objects
	vector<Tobj> *tobjs = new vector<Tobj>(); 
	//Memeber Functions
	virtual Version* findLTS_STL(long int g_wts, long int g_cts, long int tobj_id, Version**) = 0;
};

/*
 * class KSFTM - K starvation freedom transaction management
 * inherits the STM class
 * 
 * 
 * */
class KSFTM : public virtual STM	
{
	public:
	//Constructor
	KSFTM(int INITIAL_objs);	
	
	//Private member functions
	private:
		bool find_set(vector<TobIdValPair> *set, TobIdValPair* tobj_id_val_pair);
		void insertAndSortRL(list<GTransaction*> *RL, GTransaction *gtrans);
		void insertAndSortVL(Version *version, long int objId);
		list<GTransaction*>* getLar(long int g_wts, long int g_cts, list<GTransaction*> *preVerRL);
		list<GTransaction*>* getSm(long int g_wts, long int g_cts, list<GTransaction*> *preVerRL);
		bool isAborted(GTransaction* gtrans);
		void unlockAll(LTransaction *ltrans);
		Version* findLTS_STL(long int g_wts, long int g_cts, long int tobj_id, Version**);
				
	//Public member functions	
	public:	
		LTransaction* tbegin(long int its);
		bool stmRead(LTransaction* trans, TobIdValPair *tobj_id_val_pair);
		bool stmWrite(LTransaction* trans, TobIdValPair *tobj_id_val_pair);
		bool stmTryCommit(LTransaction* trans);
		bool stmAbort(LTransaction* trans);
};
