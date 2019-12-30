#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctype.h>
#include <unistd.h>
#include <queue>
#include <sstream>
#include <cstdio>
using namespace std;
ifstream myfile;
string buffer;
string algo ="";
int sflag = 0;
int vflag = 0;
int qflag = 0;
int fflag = 0;
int global_time = 0;       //indicates time during simulation
int global_completed = 0;  //indicates number of completed IO requests
int global_track = 0;      //indicates current track number of simulation
int tot_movement = 0;      //indicates number of track shifts occuring
int direction = 0;         //0 indicates ascending and 1 indicates descending track movement
class IO{                  //each IO request has a time, track, service start & service end times
public:
  int timestamp;
  int track; 
  int start_time = 0;
  int end_time = 0;
  IO(int time, int trk){
    timestamp = time;
    track = trk;
  }
};
class Scheduler{                         //scheduler interface supplies get_next, insert and empty methods
public:  
  virtual IO* get_next() = 0;
  virtual void insert(IO* io) = 0;
  virtual bool empty() = 0;
};
class FIFO: public Scheduler{               
public:
  queue<IO*>* ioqueue = new queue<IO*>();   //next io request is simply front of the queue
  IO* get_next(){
    IO* next = ioqueue->front();
    ioqueue->pop();
    return next;
  }
  void insert(IO* io){
    ioqueue->push(io);
  }
  bool empty(){
    return ioqueue->empty();
  }
};
class SSTF: public Scheduler{                
public:
  vector<IO*> * ioqueue  = new vector<IO*>(0);
  IO* get_next(){                                  //search through all the ioqueue and find min distance to
    if(ioqueue->size() == 0){                      //global_track
      return nullptr;
    }
    int min_distance = 10000;
    int min_i = 0;
    for(int i =0; i<ioqueue->size(); i++){
      IO* io = ioqueue->at(i);
      int dist = io->track - global_track;
      if(dist<0){ dist = dist*-1;}
      if(dist<min_distance){
	min_distance = dist;
	min_i = i;
      }
    }
    IO* io_vict = ioqueue->at(min_i);
    ioqueue->erase(ioqueue->begin()+min_i);
    return io_vict;
  }
  void insert(IO* io){
    ioqueue->push_back(io);
  }
  bool empty(){
    return ioqueue->empty();
  }
};
class LOOK: public Scheduler{                        //search through ioqueue in given direction, if fail
public:                                              //then change direction 
  vector<IO*> * ioqueue = new vector<IO*>(0);
  IO* get_next(){
    if(ioqueue->size() == 0){
      return nullptr;
    }
    int victim_i = 0;
    int min_dist = 100000;
    for(int i = 0; i<ioqueue->size(); i++){
      IO* io = ioqueue->at(i);
      if(direction == 0){
	if(io->track >= global_track){
	  int dist = io->track -global_track;
	  if(dist < min_dist){ 
	    min_dist = dist;
	    victim_i = i;
	  }
	}
      }
      else{
	if(io->track <= global_track){
	  int dist = global_track - io->track;
	  if(dist<min_dist){
	    min_dist = dist;
	    victim_i = i;
	  }
	}
      }
    }
    if(min_dist == 100000){
      direction = (direction+1)%2;
      return get_next();
    }
    
    IO* io_victim = ioqueue->at(victim_i);
    ioqueue->erase(ioqueue->begin()+victim_i);
    return io_victim;
  }
  void insert(IO* io){
    ioqueue->push_back(io);
  }
  bool empty(){
    return ioqueue->empty();
  }
};
class CLOOK: public Scheduler{                        //always moves in ascending direction and then resets at 
public:                                               //track = 0 once it exhausts the ioqueue
  vector<IO*> * ioqueue = new vector<IO*>(0);
  IO* get_next(){
    if(ioqueue->size() == 0){
      return nullptr;
    }
    int victim_i = 0;
    int min_dist = 100000;
    for(int i = 0; i<ioqueue->size(); i++){
      IO* io = ioqueue->at(i);
      if(io->track >= global_track){
	  int dist = io->track -global_track;
	  if(dist < min_dist){ 
	    min_dist = dist;
	    victim_i = i;
	  }
      }
    }
    if(min_dist == 100000){
      for(int i =0 ;i<ioqueue->size(); i++){
	IO* io = ioqueue->at(i);
	if(io->track <= global_track){
	  int dist = io->track;
	  if(dist< min_dist){
	    min_dist = dist;
	    victim_i = i;	    
	  }
	}
      }
    }
    IO* io_victim = ioqueue->at(victim_i);
    ioqueue->erase(ioqueue->begin()+victim_i);
    return io_victim;
  }
  void insert(IO* io){
    ioqueue->push_back(io);
  }
  bool empty(){
    return ioqueue->empty();
  }
};
class FLOOK: public Scheduler{                      //maintain active and add queue. when finding victim 
public:                                             //simply scan in direction order until exhausted and then
  vector<IO*> * activequeue = new vector<IO*>(0);   //change direction. if activequeue empty then swap(active,add)
  vector<IO*> * addqueue = new vector<IO*>(0);
  IO* get_next(){
    if(activequeue->empty()){
      vector<IO*> * temp;
      temp = activequeue;
      activequeue = addqueue;
      addqueue = temp;
    }
    if(activequeue->empty()){
      return nullptr;
    }
    int victim_i = 0;
    int min_dist = 100000;
    for(int i = 0; i<activequeue->size(); i++){
      IO* io = activequeue->at(i);
      if(direction == 0){
	if(io->track >= global_track){
	  int dist = io->track -global_track;
	  if(dist < min_dist){ 
	    min_dist = dist;
	    victim_i = i;
	  }
	}
      }
      else{
	if(io->track <= global_track){
	  int dist = global_track - io->track;
	  if(dist<min_dist){
	    min_dist = dist;
	    victim_i = i;
	  }
	}
      }
    }
    if(min_dist == 100000){
      direction = (direction+1)%2;
      return get_next();
    }
    IO* io_victim = activequeue->at(victim_i);
    activequeue->erase(activequeue->begin() + victim_i);
    return io_victim;
  }
  void insert(IO* io){
    addqueue->push_back(io);
  }
  bool empty(){
    if(addqueue->empty()){
      if(activequeue->empty()){
	return true;
      }
    } 
    return false;
  }
};
Scheduler * sched;
vector<IO> iostack;                              //maintains the actual IO requests given in file.
IO* activeIO = nullptr;                          //activeIO is current running IO request 
int main(int argc, char *argv[]){
  int c; 
  while((c = getopt(argc,argv,"s:vqf")) != -1){
    switch(c){
    case 's':
      algo = string(optarg);
      break;
    case 'v':
      vflag = 1;
      break;
    case 'q':
      qflag = 1;
      break;
    case 'f':
      fflag = 1;
      break;
    default:
      abort();
    }
  }
  if(algo == "i"){
    FIFO ff;
    sched = &ff;
  }
  if(algo =="j"){
    SSTF ss;
    sched = &ss;
  }
  if(algo=="s"){
    LOOK ll;
    sched = &ll;
  }
  if(algo=="c"){
    CLOOK cl;
    sched = &cl;
  }
  if(algo=="f"){
    FLOOK fl;
    sched = &fl;
  }
  myfile.open(argv[argc-1]);
  while(!myfile.eof()){
    getline(myfile,buffer);
    if(buffer.size() == 0){continue;}
    if(buffer.at(0) == '#'){continue;}
    istringstream ss(buffer);
    int timestamp;
    int track;
    ss >> timestamp >> track;
    IO io = IO(timestamp,track);
    iostack.push_back(io);
  }
  int n = iostack.size();
  while(global_completed < n){                       //continue until all requests have completed
    for(int i =0; i<iostack.size();i++){             //at each time step check if there are new IO requests
      if(iostack.at(i).timestamp == global_time){    //to add to the sched queue
	IO* new_io = &iostack.at(i);
	sched->insert(new_io);
      }
    }    

    if((activeIO != nullptr) && (activeIO->track != global_track)){ //if IO running and we havent reached it yet
	if(global_track<activeIO->track){ 
	  global_track++;
	  direction = 0;       //ascending direction is set
	}
	else{
	  global_track--;
	  direction = 1;       //descending direction is set
	}
	tot_movement++;
    }
    if((activeIO != nullptr) && (activeIO->track == global_track)){ //if previous step caused us to be completed
      activeIO->end_time = global_time;                           //then mark completion time and reset activeIO
	global_completed++;
	activeIO = nullptr;
    }
    if(activeIO == nullptr){                                           //if we need a new IO req
      if(!sched->empty()){               
	activeIO = sched->get_next();
	activeIO->start_time = global_time;
	while((activeIO != nullptr) && (activeIO->track == global_track)){ //this is here to ensure that if multiple
	  activeIO->end_time = global_time;                                //available IO reqs have same track num 
	  global_completed++;                                              //then they are marked completed at the 
	  activeIO = sched->get_next();                                    //same time
	  if(activeIO != nullptr){
	    activeIO->start_time = global_time;
	  }
	}
      }
      if(global_completed == n){
	break;
      }
    }
    global_time++;
  }
  double avg_turnaround = 0;
  double avg_waittime =0;
  int max_waittime = -1;
  for(int i =0; i<iostack.size(); i++){
    IO io = iostack.at(i);
    printf("%5d: %5d %5d %5d\n",i,io.timestamp,io.start_time,io.end_time);
    avg_turnaround += (io.end_time - io.timestamp);
    avg_waittime += (io.start_time - io.timestamp);
    if((io.start_time - io.timestamp)>max_waittime){
      max_waittime = io.start_time - io.timestamp;
    }
  }
  avg_turnaround = avg_turnaround/iostack.size();
  avg_waittime = avg_waittime/iostack.size();
  printf("SUM: %d %d %.2lf %.2lf %d\n",
	 global_time, tot_movement,avg_turnaround, avg_waittime,max_waittime);
  return 0;
}
