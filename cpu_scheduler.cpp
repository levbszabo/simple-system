#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <list>
#include <iterator>
#include <queue>
#include <deque>
#include <ctype.h>
#include <unistd.h>
#include <string>
using namespace std;
ifstream myfile;
ifstream randfile;
class RAND{
public:
  int N;
  int OFFSET=0;
  long* randvals;
  void getRandVals(string randmfile){
    randfile.open(randmfile);
    randfile >> N;
    randvals = new long[N];
    for(int i=0; i<N;i++){
      long v;
      randfile >> v;
      randvals[i] = v;
    }
  }
  int myrandom(int burst){
    OFFSET = OFFSET%N;
    int out = 1+(randvals[OFFSET]%burst);
    OFFSET = OFFSET+1;
    return out;
  }
  RAND(){
    OFFSET = 0;
    N = 0;
  }
};
RAND randGen;
int vflag = 0;
typedef enum{
  STATE_CREATED,
  STATE_READY,
  STATE_RUNNING,
  STATE_BLOCKED,
  STATE_PREEMPT,
}process_state;
class Process{
public:
  int AT,TC,CB,IB;
  process_state state;
  int id = 0;
  int state_ts;
  int elapsed;
  int burst;
  int static_priority;
  int dynamic_priority;
  int expired = 0;
  int FT = 0;  //finishing time;
  int TT =0;
  int IT= 0;  //io time
  int CW = 0;  //cpu wait time
  Process(int at,int tc, int cb, int ib){
    AT = at;
    TC = tc;
    CB = cb;
    IB = ib;
    state = STATE_CREATED;
    state_ts = at;
    elapsed = 0;
    burst=0 ;
  }
};
char line[100]; 
char* p = NULL;
list<Process> processes;
vector<string> split(string str, char del){
  vector<string> ret;
  istringstream stm(str);
  string token = "";
  int i = 0;
  while(getline(stm,token,del)){
    if(token.size() > 0 && !isspace(token.at(0))){
      i++;
      ret.push_back(token);
    }
  }
  return ret;
}
void printProcesses(list<Process> lst){
  list<Process>::iterator it;
  for(it = lst.begin(); it!=lst.end();++it){
    cout<<(*it).AT<<" "<<(*it).TC<<" "<<(*it).CB<<" "<<(*it).IB<<" "<<(*it).state<<endl;
  }
}
Process* CURRENT_RUNNING_PROCESS = nullptr;
enum transState{
  TRANS_TO_READY,
  TRANS_TO_RUN,
  TRANS_TO_BLOCK,
  TRANS_TO_PREEMPT,
};
long event_count = 0;
class Event{
public:
  int timestamp;
  Process *process;
  process_state oldstate;
  process_state newstate;
  transState transition;
  long event_count_local = 0;
  Event(int tstamp,Process *p, process_state ostate, process_state nstate,transState t){
    timestamp = tstamp;
    process = p;
    oldstate = ostate;
    newstate = nstate;
    transition = t;
    event_count_local = event_count;
  }
};
struct LessThanByTimestep{
  bool operator()(const Event lhs, const Event rhs) const{
    if(lhs.timestamp > rhs.timestamp){ return true;}
    if(lhs.timestamp < rhs.timestamp){ return false;}
    if(lhs.timestamp == rhs.timestamp){
      if(lhs.event_count_local > rhs.event_count_local){
	return true;
      }
      else{
	return false;
      }
    }
  }
};
priority_queue<Event,vector<Event>,LessThanByTimestep> eventQ;
int CURRENT_TIME = 0;
const Event* get_event(){
    if(eventQ.size()==0){
      return nullptr;
    }
    const Event* e = &(eventQ.top());
    return e;
}
void deleteEvent(){
    if(eventQ.size()>0){
      eventQ.pop();
      event_count++;
    }
    return;
}
int get_next_event_time(){
    if(eventQ.size() == 0){
      return 0;
    }
    else{
      return eventQ.top().timestamp;
    }
}
class scheduler{
public:
  int quantum;
  int maxprio;
  scheduler(int q, int m){
    quantum = q;
    maxprio = m;
  }
  virtual void add_process(Process* p){ return;}
  virtual Process* get_next_process(){ return nullptr;}
  virtual void test_preempt(Process *p, int curtime){ return;}
};
class FIFO: public scheduler{
public:
  FIFO(int q, int m): scheduler(q,m){}
  queue<Process*> *RUN_QUEUE = new queue<Process*>[1];
  void add_process(Process *p){
    RUN_QUEUE[0].push(p);
    return;
  }
  Process* get_next_process(){
    if(RUN_QUEUE[0].size() == 0){
      return nullptr;
    }
    Process* out = RUN_QUEUE[0].front();
    RUN_QUEUE[0].pop();
    return out;
  }
  void test_preempt(Process *p, int curtime){ return;}
};
class LCFS: public scheduler{
public:
  LCFS(int q, int m): scheduler(q,m){}
  deque<Process*> *RUN_QUEUE = new deque<Process*>[1];
  void add_process(Process *p){
    RUN_QUEUE[0].push_back(p);
  }
  Process* get_next_process(){
    if(RUN_QUEUE[0].size()==0){
      return nullptr;
    }
    Process* out = RUN_QUEUE[0].back();
    RUN_QUEUE[0].pop_back();
    return out;
  }
  void test_preempt(Process *p, int curtime){ return;}
};
class RR: public scheduler{
public:
  RR(int q, int m): scheduler(q,m){}
  queue<Process*> *RUN_QUEUE = new queue<Process*>[1];
  Process* get_next_process(){
    if(RUN_QUEUE[0].size()==0){
      return nullptr;
    }
    Process* out = RUN_QUEUE[0].front();
    RUN_QUEUE[0].pop();
    return out;
  }
  void add_process(Process* p){
    RUN_QUEUE[0].push(p);
  }
  void test_preempt(Process *p, int curtime){ return;}
};
class SRTF: public scheduler{
public:
  SRTF(int q, int m): scheduler(q,m){}
  struct compareProcPtrs{
    bool operator()(const Process* lhs, const Process* rhs){
      long lhs_rem = (lhs->TC)-(lhs->elapsed);
      long rhs_rem = (rhs->TC)-(rhs->elapsed);
      if (lhs_rem > rhs_rem){ return true;}
      if (lhs_rem < rhs_rem){ return false;}
      if (lhs_rem == rhs_rem){
	if(lhs->id > rhs->id){
	  return true;
	}
	else{
	  return false;
	}
      }
      //return ((lhs->TC)-(lhs->elapsed)) > ((rhs->TC)-(rhs->elapsed));
    }
  };
  priority_queue<Process* , vector<Process*>, compareProcPtrs> *RUN_QUEUE = new priority_queue<Process*,vector<Process*>,compareProcPtrs>[1];
  void add_process(Process *p){
    RUN_QUEUE[0].push(p);
  }
  Process* get_next_process(){
    if(RUN_QUEUE[0].size() == 0){
      return nullptr;
    }
    Process* out = RUN_QUEUE[0].top();
    RUN_QUEUE[0].pop();
    return out;
  }
  void test_preempt(Process *p, int curtime){ return;}
};
class prio: public scheduler{
public:
  prio(int q, int m): scheduler(q,m){}
  queue<Process*> *activeQ = new queue<Process*>[maxprio];
  queue<Process*> *expiredQ = new queue<Process*>[maxprio];
  void swap(queue<Process*>*& ptr1, queue<Process*>*& ptr2){
    queue<Process*> * temp = ptr1;
    ptr1 = ptr2;
    ptr2 = temp;
  }
  void add_process(Process* p){
    if(p->expired == 1 ){  
      p->expired == 0;//either we ran out of time_quantum and are placed in expired queue
      expiredQ[p->dynamic_priority].push(p);
    }
    else{                                       //or we finished full process(coming from blocked) or just created (from created)
      activeQ[p->dynamic_priority].push(p);
    }
  }
  Process* get_next_process(){
    int level = maxprio-1;
    while(level>=0){
      if(activeQ[level].size()==0){
	level--;
      }
      else{
	Process* out = activeQ[level].front();
	activeQ[level].pop();
	return out;
      }
    }
    swap(activeQ,expiredQ);
    level = maxprio-1;
    while(level>=0){
      if(activeQ[level].size() == 0){
	level--;
      }
      else{
	Process* out = activeQ[level].front();
	activeQ[level].pop();
	return out;
      }
    }
    return nullptr;
  }
  void test_preempt(Process *p, int curtime){ return;}
};
class preprio: public scheduler{
public:
  preprio(int q, int m):scheduler(q,m){}
  deque<Process*> *activeQ = new deque<Process*>[maxprio+1];
  deque<Process*> *expiredQ = new deque<Process*>[maxprio+1];
  void swap(deque<Process*>*& ptr1, deque<Process*>*& ptr2){
    deque<Process*> * temp = ptr1;
    ptr1 = ptr2;
    ptr2 = temp;
  }
  void add_process(Process* p){
    if(p->state == STATE_PREEMPT){               //either we ran out of time_quantum and are placed in expired queue
      expiredQ[p->dynamic_priority].push_back(p);
    }
    else{                                       //or we finished full process(coming from blocked) or just created (from created)
      activeQ[p->dynamic_priority].push_back(p);
    }
  }
  Process* get_next_process(){
    int level = maxprio-1;
    while(level>=0){
      if(activeQ[level].size()==0){
	level--;
      }
      else{
	Process* out = activeQ[level].front();
	activeQ[level].pop_front();
	return out;
      }
    }
    swap(activeQ,expiredQ);
    level = maxprio-1;
    while(level>=0){
      if(activeQ[level].size() == 0){
	level--;
      }
      else{
	Process* out = activeQ[level].front();
	activeQ[level].pop_front();
	return out;
      }
    }
    return nullptr;
  }
void test_preempt(Process* p, int curtime){
    if(CURRENT_RUNNING_PROCESS != nullptr && p->dynamic_priority > CURRENT_RUNNING_PROCESS->dynamic_priority){
      //      eventQCopy=new priority_queue(&eventQ);
      priority_queue<Event,vector<Event>,LessThanByTimestep> eventQCopy(eventQ);
      Event top = eventQCopy.top();
      int hasEvent = 0;
      while(top.timestamp == curtime){
	if(top.process = CURRENT_RUNNING_PROCESS){
	  hasEvent = 0;
	  break;
	}
	eventQCopy.pop();
	top = eventQCopy.top(); 
      }
      // if there is preempt/block event  already  we return, otherwise we need to empty out the eventQ and remove
      //its future block/preemption event
      if(hasEvent == 1){
	return;
      }
      else{
	Event top = eventQ.top();
	vector<Event> tempEventQ;
	while(top.process != CURRENT_RUNNING_PROCESS){
	  eventQ.pop();
	  tempEventQ.push_back(top);
	  top = eventQ.top();          
	}
	eventQ.pop();
	//now reinsert back into eventQ;
        for(int i = tempEventQ.size()-1; i>=0; i--){
	  eventQ.push(tempEventQ.at(i));
	}
	event_count++;
	Event preemption(curtime,CURRENT_RUNNING_PROCESS,STATE_RUNNING,STATE_PREEMPT,TRANS_TO_PREEMPT); //add preempt event for current running process;
	eventQ.push(preemption);
	return;
      }
    }
}
};
bool CALL_SCHEDULER=false;
scheduler * THE_SCHEDULER = nullptr;
void init(){
  int pid = 0;
    for(list<Process>::iterator it = processes.begin(); it!=processes.end(); ++it){
      int tstamp = (*it).AT;
      Process* p = &(*it);
      p->id = pid;
      //int static_prio = randGen.myrandom(THE_SCHEDULER->maxprio);
      //int dynam_prio = static_prio-1;
      //p->static_priority = static_prio;
      //p->dynamic_priority = dynam_prio;
      process_state ostate = STATE_CREATED;
      process_state nstate = STATE_READY;
      transState t = TRANS_TO_READY;
      event_count++;
      Event e(tstamp,p,ostate,nstate,t);
      eventQ.push(e);
      pid++;
    }
}
class Timestamp{ //serves as a storage for 
public:
  int timestamp;
  int n_cpu; //indicates number of procs running at timestamp 
  int n_io; // indicates number of procs using IO at timestampo
  Timestamp(int t, int nc, int nio){
    timestamp = t;
    n_cpu = nc;
    n_io = nio;
  }
};
void simulation(){
    const Event* e = nullptr;
    vector<Timestamp> timeVector;
    Timestamp t(1,1,1);
    while(eventQ.size()!=0){
      e = get_event();
      if(vflag == 1){
	cout<<"process:"<<e->process->id<<" timestamp:"<<e->timestamp<<" transition:"<<e->transition<<" dyn_priority:"<<e->process->dynamic_priority<<endl;
      }
      Process* proc = e->process;
      CURRENT_TIME = e->timestamp;
      int n_cpu = 0;
      int n_io = 0;
      if(timeVector.size()>0){
	n_cpu = timeVector.at(timeVector.size()-1).n_cpu;
	n_io = timeVector.at(timeVector.size()-1).n_io;
      }
      t.timestamp = CURRENT_TIME;
      t.n_cpu = n_cpu;
      t.n_io = n_io;
      int timeInState = CURRENT_TIME-(proc->state_ts);
      switch(e->transition){
      case TRANS_TO_READY:{
	CALL_SCHEDULER = true;
        THE_SCHEDULER->test_preempt(proc,CURRENT_TIME);
        THE_SCHEDULER->add_process(proc);
       	proc->state = STATE_READY;
	proc->state_ts = CURRENT_TIME;
	if(e->oldstate == STATE_BLOCKED){
	  t.n_io--;
	}
	break;
      }
      case TRANS_TO_RUN:{
	CURRENT_RUNNING_PROCESS = proc;
	proc->state = STATE_RUNNING;
	proc->state_ts = CURRENT_TIME;
	t.n_cpu++;
	if(proc->burst == 0){                      //if no left over cpu burst from post preempt
	  int cpuBurst = randGen.myrandom(proc->CB);      //generate new cpu burst
	  proc->burst = cpuBurst;
	}
	if((proc->burst + proc->elapsed)>proc->TC){ //if burst is more then needed
	  int cpuBurst = ((proc->TC) - (proc->elapsed)); //then reduce so that elapsed + burst = tc
	  proc->burst = cpuBurst;
        }
        if(proc->burst > THE_SCHEDULER->quantum){    //if burst is more than quantum
	  event_count++;
	  Event e2(CURRENT_TIME+(THE_SCHEDULER->quantum),proc,STATE_RUNNING,STATE_PREEMPT,TRANS_TO_PREEMPT);
	  eventQ.push(e2);                        //add preempt event to EventQ
	}
	if(proc->burst <= THE_SCHEDULER->quantum){  //if burst is less than = quantum then add block event
	  event_count++;
	  Event e2(CURRENT_TIME + proc->burst,proc,STATE_RUNNING,STATE_BLOCKED,TRANS_TO_BLOCK); 
	  eventQ.push(e2);
	}
	break;
      }
      case TRANS_TO_BLOCK:{
	proc->state = STATE_BLOCKED;
	proc->elapsed = proc->elapsed + timeInState;        //increment elapsed time
	proc->state_ts = CURRENT_TIME;
	t.n_cpu--;
	if(CURRENT_RUNNING_PROCESS == proc){
	  CURRENT_RUNNING_PROCESS = nullptr;  
	}    
	if(proc->elapsed == proc->TC){   //if proc is done we dont gen new event and fix CPU
	  proc->FT = CURRENT_TIME;
	  proc->TT = proc->FT - proc->AT;
	  proc->CW = proc->TT - proc->IT - proc->TC;
	}
	if(proc->elapsed < (proc->TC)){                    //if process is not yet finished
	  int ioburst = randGen.myrandom(proc->IB);
	  proc->IT = (proc->IT) + ioburst;
	  proc->burst = 0; 
	  t.n_io++;
	  event_count++;
	  Event e2(CURRENT_TIME+ioburst,proc,STATE_BLOCKED,STATE_READY,TRANS_TO_READY);
	  eventQ.push(e2);
	}
	CALL_SCHEDULER = true;
	break;
      }
    case TRANS_TO_PREEMPT:{
      CALL_SCHEDULER = true;
      proc->dynamic_priority = (proc->dynamic_priority)-1;
      if(proc->dynamic_priority == -1){
	proc->dynamic_priority = (proc->static_priority)-1;	
	proc->expired = 1;
      }
      if(THE_SCHEDULER != nullptr){
	THE_SCHEDULER->add_process(proc);
      }                  //dynamic priority is decremented in scheduler
      CURRENT_RUNNING_PROCESS = nullptr;                 //reset running_process and call scheduler
      proc->elapsed = proc->elapsed + timeInState;       //incremenet total elapsed compute time
      proc->burst = proc->burst - timeInState;           //decrement the remaining cpu burst (to be used later)
      proc->state_ts = CURRENT_TIME;
      proc->state = STATE_PREEMPT;
      t.n_cpu--;
      break;
    }
      
      }
      timeVector.push_back(t);
      e = nullptr;   
      deleteEvent();
      if(CALL_SCHEDULER){
	if(get_next_event_time() == CURRENT_TIME){
	  continue;
	}
	CALL_SCHEDULER=false;
	if(CURRENT_RUNNING_PROCESS == nullptr){
	  CURRENT_RUNNING_PROCESS=(THE_SCHEDULER->get_next_process());
	  if(CURRENT_RUNNING_PROCESS == nullptr){
	    continue;
	  }
	  process_state oldstate = STATE_READY;
	  process_state newstate = STATE_RUNNING;
	  event_count++;
	  Event e2(CURRENT_TIME,CURRENT_RUNNING_PROCESS,oldstate,newstate,TRANS_TO_RUN);
	  eventQ.push(e2);
	  CURRENT_RUNNING_PROCESS->state_ts = CURRENT_TIME;
	}
      }
      
    }
    double timeCPU = 0;
    double timeIO = 0;
    int i = 0;  
    while(i<timeVector.size()){
      Timestamp t = timeVector.at(i);
      int start = t.timestamp;
      if(t.n_cpu>0){
	while(i< timeVector.size() && timeVector.at(i).n_cpu > 0){
	  i++;
	}
	timeCPU += timeVector.at(i).timestamp - start;
      }
      i++;
    }
    i =0;
    while(i<timeVector.size()){
      Timestamp t = timeVector.at(i);
      int start = t.timestamp;
      if(t.n_io>0){
	while(i<timeVector.size() && timeVector.at(i).n_io >0){
	  i++;
	}
	timeIO += timeVector.at(i).timestamp - start;
      }
      i++;
    }
    int maxfintime = timeVector.at(timeVector.size()-1).timestamp;
    double cpu_util = timeCPU/maxfintime;
    double io_util = 0;
    if(timeIO != 0){ io_util = timeIO/maxfintime;}
    cpu_util = 100*cpu_util;
    io_util = 100*io_util;
    double avgTT = 0;
    double avgCW = 0;
    for(list<Process>::iterator it = processes.begin();it!=processes.end();++it){
      Process* p = &(*(it)); 
      avgTT += p->TT;
      avgCW += p->CW;
    }
    int size = processes.size();
    avgTT = avgTT/size;
    avgCW = avgCW/size;
    double throughput = 100*((1.0 * processes.size()) / maxfintime);
    for(list<Process>::iterator it= processes.begin();it!=processes.end();++it){
      Process* p = &(*(it));
      int id = p->id;
      int arrival = p->AT;
      int totaltime = p->TC;
      int cpuburst = p->CB;
      int ioburst = p->IB;
      int static_prio = p->static_priority;
      int finish_time = p->FT;
      int turnaround_time=p->TT;
      int iowait = p->IT;
      int cpuwait = p->CW;
      printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
	     id,
	     arrival, totaltime, cpuburst, ioburst, static_prio,
	     finish_time,
	     turnaround_time,
	     iowait,
	     cpuwait);
    }
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
	   maxfintime,
	   cpu_util,
	   io_util,
	   avgTT,
	   avgCW,
	   throughput);
}
int main(int argc, char *argv[]){
  vflag = 0;
  int tflag = 0;
  int eflag = 0;
  char *svalue =nullptr;
  int c;
  while((c=getopt(argc,argv,"vtes::"))!=-1){
    switch(c){
    case 'v':
      vflag = 1;
      break;
    case 't':
      tflag = 1;
      break;
    case 'e':
      eflag = 1;
      break;
    case 's':
      svalue = optarg;
      break;
    default:
      abort();
    }
  }
  //good up to here
  myfile.open(argv[argc-2]);
  randGen.getRandVals(argv[argc-1]);
  char sched_type ='F';
  int quantum = 100000;
  int maxprio = 4;
  if(svalue!=nullptr){
    string s(svalue);
    sched_type = s.at(0);
    
    int colon_index = 0;
    for(int i =0; i<s.size();i++){
      if(s.at(i) == ':'){
	colon_index = i;
      }
    }
    if(colon_index != 0){
      string quantum_string = s.substr(1,colon_index-1);
      string maxprio_string = s.substr(colon_index+1,s.size());
      quantum = stoi(quantum_string);
      maxprio = stoi(maxprio_string);
      
    }
    else if(s.size()>1){
      quantum = stoi(s.substr(1,s.size()));
    }
  }
  char line[100];
  while(!myfile.eof()){
    myfile.getline(line,100);
    string linestring(line);
    vector<string> vec = split(linestring, ' ');
    if(vec.size()==4){
      int AT = stoi(vec.at(0),nullptr,10);
      int TC = stoi(vec.at(1),nullptr,10);
      int CB = stoi(vec.at(2),nullptr,10);
      int IB = stoi(vec.at(3),nullptr,10);
      Process p(AT,TC,CB,IB);
      int static_prio = randGen.myrandom(maxprio);
      int dynam_prio = static_prio-1;
      p.static_priority = static_prio;
      p.dynamic_priority = dynam_prio;
      if(processes.size() == 0){
	processes.push_back(p);
      }
      else{
	int arrival = AT;
	int inserted = 0;
	for(list<Process>::iterator it = processes.begin(); it!=processes.end();++it){
	  int currentAT = it->AT;
	  if(currentAT>arrival){
	    processes.insert(it,p);
	    inserted = 1;
	    break;
	  }
	}
	if(inserted==0){processes.push_back(p);}
      }
    }
    else{break;}
  }
  switch(sched_type){
  case 'F':{
    cout<<"FCFS"<<endl;
    FIFO fifo(quantum,maxprio);
    THE_SCHEDULER = &fifo;
    break;
  }
  case 'L':{
    cout<<"LCFS"<<endl;
    LCFS lcfs(quantum,maxprio);
    THE_SCHEDULER = &lcfs;
    break;
  }
  case 'S':{
    cout<<"SRTF"<<endl;
    SRTF srtf(quantum,maxprio);
    THE_SCHEDULER = &srtf;
    break;
  }
  case 'R':{
    cout<<"RR "<<quantum<<endl;
    RR roundrobin(quantum,maxprio);
    THE_SCHEDULER = &roundrobin;
    break;
  }
  case 'P':{
    cout<<"PRIO "<<quantum<<endl;
    prio priosched(quantum,maxprio);
    THE_SCHEDULER = &priosched;
    break;
  }
  case 'E':{
    cout<<"PREPRIO "<<quantum<<endl;
    preprio prepriosched(quantum,maxprio);
    THE_SCHEDULER = &prepriosched;
    break;
  }
  }


  init();
  simulation();
  //  printProcesses(processes);
  myfile.close();
  return 0;
}
