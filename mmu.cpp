#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctype.h>
#include <unistd.h>
#include <sstream>
#include <deque>
#include <cstdio>
using namespace std;
ifstream myfile;
ifstream randfile;
int num_procs=0;
string buffer;
string buffer2;
string randfile_name;
int n_randoms;
int r_ofs = 0;
string algo ="";
string options="";
int num_frames=128;
char operation;
int vpage=0;
int oflag=0;
int pflag=0;
int fflag=0;
int sflag=0;
unsigned long inst_count=0;
unsigned long ctx_switches=0;
unsigned long process_exits=0;
unsigned long long cost=0;
int get_random(int mod){  //method reads random value from randfile and outputs value%mod, reopens file when 
  getline(randfile,buffer2); //ofs reaches limit
  int out = stoi(buffer2);
  r_ofs++;
  if(r_ofs ==n_randoms){
    randfile.close();
    randfile.open(randfile_name);
    getline(randfile,buffer2);
    r_ofs =0;
  }
  return out%mod;
}
bool get_next_instruction(char* op, int* vp){ //reads in character and page number
  while(getline(myfile,buffer)){
    if(buffer.empty()){return false;}
    if(buffer.at(0) == '#'){ continue;}
    char opchar;
    int vpint;
    istringstream mystream(buffer);
    mystream >>opchar >>vpint;
    *op = opchar;
    *vp = vpint;
    return true;
  }
  return false;
} 
struct VMA{   
  int start_vpage;
  int end_vpage;
  unsigned int write_protected;
  unsigned int file_mapped; 
};
struct PTE{
  unsigned int frame:7;
  unsigned int present:1;
  unsigned int write_protected:1;
  unsigned int modified:1;
  unsigned int referenced:1;
  unsigned int pagedout:1;
  unsigned int file_mapped:1;
};
class Process{ //contains page_table of PTE structs as well as recording various info
public: 
  PTE * page_table = new PTE[64];
  vector<VMA> vmas;
  unsigned int pid = 0;
  unsigned int exited = 0;
  long unsigned int unmaps=0;
  long unsigned int maps=0;
  long unsigned int ins=0;
  long unsigned int outs=0;
  long unsigned int fins=0;
  long unsigned int fouts=0;
  long unsigned int zeros=0;
  long unsigned int segv=0;
  long unsigned int segprot=0;
  void insert_vma(string s){
    int start;
    int end;
    unsigned int write_p;
    unsigned int file_m;
    istringstream mystream(s);
    mystream >> start >> end >> write_p >> file_m;
    struct VMA vma = {start,end,write_p,file_m};
    vmas.push_back(vma);
  }
  bool contains_page(int page){
    for(int i = 0; i<vmas.size(); i++){
      if(page>=vmas.at(i).start_vpage && page<=vmas.at(i).end_vpage){
	return true;
      }
    }
    return false;
  }
  void set_bits_wf(int page){
    for(int i = 0; i<vmas.size();i++){
      if(page>=vmas.at(i).start_vpage && page<=vmas.at(i).end_vpage){
	page_table[page].write_protected = vmas.at(i).write_protected;
	page_table[page].file_mapped = vmas.at(i).file_mapped;
      }
    }
  }
};
vector<Process> processes;
Process* current_process = nullptr;
struct FTE{
  unsigned int present:1; //denotes if occupied
  unsigned int index:7;   //index within frame_table, one of 128
  unsigned int processID:4; //entry into one of at most 10 processes
  unsigned int virtual_page:6; //entry into one of 64 virtual pages
  unsigned int age:32;
  unsigned int time_used:32;
};
FTE *frame_table = new FTE[num_frames];
deque<int> * free_pool = new deque<int>;
class Pager{
public:
  virtual FTE* select_victim_frame() = 0;
};
class FIFO:public Pager{
public:
  int hand = 0;
  FTE* select_victim_frame(){
    FTE* victim = &(frame_table[hand]);
    hand = (hand+1)%num_frames;
    return victim;
  }
};
class CLOCK:public Pager{
public:
  int hand = 0;
  FTE* select_victim_frame(){
    while(true){
      FTE* victim = &(frame_table[hand]);
      int pid = victim->processID;
      int virtPage = victim->virtual_page;
      PTE* pte = &processes.at(pid).page_table[virtPage];
      if(!pte->referenced){
	hand = (hand+1)%num_frames;
	return victim;
      }
      else{
	pte->referenced = 0;
      }
      hand = (hand+1)%num_frames;
    }
  }
};
class NRU:public Pager{
public:
  int hand = 0;
  int last_inst_count = 0;
  void set_r_bits(){
    for(int i = 0; i<num_frames; i++){
      FTE* fte = &frame_table[i];
      if(!fte->present){ continue;}
      int pid = fte->processID;
      int virtPage = fte->virtual_page;
      processes.at(pid).page_table[virtPage].referenced = 0;
    }
  }
  FTE* select_victim_frame(){
    bool resetbits = false;
    if(inst_count - last_inst_count >=50){
      resetbits = true;
      last_inst_count = inst_count;
    }
    int init_hand = hand;
    int f0_hand = -1;
    int f1_hand = -1;
    int f2_hand = -1;
    int f3_hand = -1;
    do{
      FTE* fte = &frame_table[hand];
      if(!fte->present){
	hand = (hand+1)%num_frames;
	continue;
      }
      int pid = fte->processID;
      int virtPage = fte->virtual_page;
      int R = processes.at(pid).page_table[virtPage].referenced;
      int M = processes.at(pid).page_table[virtPage].modified;
      if(R==0 && M==0){
	f0_hand = hand;
	hand = (hand+1)%num_frames;
	if(!resetbits){
	  return &frame_table[f0_hand];
	}
	else{
	  set_r_bits();
	  return &frame_table[f0_hand];
	}
      }
      if(R==0 && M==1){
	if(f1_hand == -1){f1_hand = hand;}
      }
      if(R==1 && M==0){
	if(f2_hand ==-1){f2_hand = hand;}
      }
      if(R==1 && M==1){
	if(f3_hand == -1){f3_hand = hand;}
      }
      if(resetbits){processes.at(pid).page_table[virtPage].referenced = 0;}
      hand = (hand+1)%num_frames;
    }while(hand!= init_hand);
    if(f1_hand!=-1){
      hand = (f1_hand+1)%num_frames;
      return &frame_table[f1_hand];}
    if(f2_hand !=-1){
      hand = (f2_hand+1)%num_frames;
      return &frame_table[f2_hand];}
    if(f3_hand !=-1){
      hand = (f3_hand+1)%num_frames;
      return &frame_table[f3_hand];}
    else{
      hand = (hand+1)%num_frames;
      return &frame_table[init_hand];
    }
  }
};

class AGING: public Pager{
public:
  int hand = 0;
  FTE* select_victim_frame(){
    int victim = 0;
    int min = 0x7FFFFFFF;
    int init_hand = hand;
    do{
      FTE* ft = &frame_table[hand];
      int pid = ft->processID;
      int virtPage = ft->virtual_page;
      int R = processes.at(pid).page_table[virtPage].referenced;
      if(ft->age ==0x7FFFFFFF){
	continue;
      }
      ft->age = ft->age >> 1;
      if(R == 1){ 
	ft->age = (ft->age |0x80000000); //if R==1 set most significant bit to 1
	processes.at(pid).page_table[virtPage].referenced = 0; //reset referenced bit
      }
      if(ft->age < min){  //find the page with the min age (least used)
	min = ft->age;    
	victim = ft->index;
      }
      hand = (hand+1)%num_frames;
    }while(hand!=init_hand);
    hand = (victim+1)%num_frames;
    FTE* victim_frame = &frame_table[victim];
    victim_frame->age = 0; //since victim will be mapped we set its age to 0
    return victim_frame;
  }
};
class RANDOM: public Pager{
public:
  int hand =0 ;
  FTE* select_victim_frame(){
    hand = get_random(num_frames);
    FTE* fte = &frame_table[hand];
    return fte;
  }
};
class WORKING: public Pager{
public:
  int hand = 0;
  FTE* select_victim_frame(){
    int init_hand = hand;
    int oldest_frame=0;
    int oldest_time=1000000;
    do{
      FTE* fte = &frame_table[hand];
      int R = processes.at(fte->processID).page_table[fte->virtual_page].referenced;
      if(R==1){
	fte->time_used = inst_count;
        processes.at(fte->processID).page_table[fte->virtual_page].referenced = 0;
      }
      if((!R) && (inst_count-fte->time_used>=50 )){
	hand = (hand+1)%num_frames;
	fte->time_used = inst_count;
	return fte;
      }
      if((!R) && fte->time_used<oldest_time){
	oldest_frame = fte->index;
	oldest_time = fte->time_used;
      }
      hand = (hand+1)%num_frames;
      // processes.at(fte->processID).page_table[fte->virtual_page].referenced = 0;
    }while(init_hand!=hand);
    if(oldest_time!=1000000){
      hand = (oldest_frame+1)%num_frames;
      return &frame_table[oldest_frame];
    }
    else{
      hand = (hand+1)%num_frames;
      return &frame_table[init_hand];
    }
  }
};
Pager * THE_PAGER;
void init_pool(){
  for(int i = 0; i<num_frames;i++){
    free_pool->push_back(i);
    frame_table[i].index = i;
    frame_table[i].present = 0;
    frame_table[i].processID = 0;
    frame_table[i].virtual_page = 0;
  }
}
FTE* allocate_from_pool(){
  if(free_pool->size() == 0){
    return nullptr;
  }
  else{
    int i = free_pool->front();
    free_pool->pop_front();
    return &(frame_table[i]);
  }
}
FTE* get_frame(){
  FTE* frame = allocate_from_pool();
  if(frame == nullptr){
    frame = THE_PAGER->select_victim_frame();
  }
  return frame;
}
int get_num(){
  if(buffer.at(0) == '#'){
    getline(myfile,buffer);
    return get_num();
  }
  return stoi(buffer);
}
string get_vma(){
  if(buffer.at(0) == '#'){
    getline(myfile,buffer);
    return get_vma();
  }
  return buffer;
}
void context_switch(int proc_num){
  Process* proc = &(processes.at(proc_num));
  cout<<"EXIT current process "<<proc_num<<endl;
  for(int i =0 ;i <64;i++){
    PTE* pte = &proc->page_table[i];
      if(pte->present){
	FTE* fte = &frame_table[pte->frame];
	int old_procID = fte->processID;
	int old_vpage = fte->virtual_page;
	if(oflag){cout<<" UNMAP "<<old_procID<<":"<<old_vpage<<endl;}
	proc->unmaps++;
	if(pte->file_mapped && pte->modified){
	  if(oflag){cout<<" FOUT "<<endl;}
	  proc->fouts++;
	}
	//return to free pool
	proc->exited = 1;
	free_pool->push_back(fte->index);
	fte->present = 0;
	fte->processID = 0;
	fte->virtual_page = 0;
	fte->age = 0;
	pte->present = 0;
	pte->frame = 0;
	pte->referenced = 0;
	pte->file_mapped = 0; //?
	pte->pagedout = 0;
	pte->modified = 0;
	//do we zero out modified,referenced,pagedout bits on process exit
      }
  }
}
int main(int argc, char *argv[]){
  int c; 
  while((c = getopt(argc,argv,"a:o:f:")) != -1){
    switch(c){
    case 'a':
      algo = string(optarg);
      break;
    case 'o':
      options = string(optarg);
      break;
    case 'f':
      num_frames = stoi(string(optarg));
      break;
    default:
      abort();
    }
  }
  //  cout<<algo<<" "<<options<<" "<<num_frames<<endl;
  for(int i =0; i<options.size();i++){
    if(options.at(i) == 'O'){
      oflag = 1;
    }
    if(options.at(i) == 'P'){
      pflag = 1;
    }
    if(options.at(i)=='F'){
      fflag = 1;
    }
    if(options.at(i) =='S'){
      sflag = 1;
    }
  }
  myfile.open(argv[argc-2]);
  randfile_name = argv[argc-1];
  randfile.open(randfile_name);
  getline(randfile,buffer2); 
  n_randoms = stoi(buffer2);
  if(algo =="c"){
    CLOCK cc;
    THE_PAGER = &cc;
  }
  if(algo == "e"){
    NRU nr;
    THE_PAGER = &nr;
  }
  if(algo == "a"){
    AGING ag;
    THE_PAGER = &ag;
  }
  if(algo == "f"){
    FIFO ff;
    THE_PAGER = &ff;
  }
  if(algo == "r"){
    RANDOM rn;
    THE_PAGER = &rn;
  }
  if(algo == "w"){
    WORKING wr;
    THE_PAGER = &wr;
  }
  init_pool();
  getline(myfile,buffer);
  num_procs = get_num();
  getline(myfile,buffer);
  for(int i =0; i<num_procs;i++){
     Process p;
     p.pid = i;
     int num_vmas = get_num();
     getline(myfile,buffer);
     for(int j = 0; j<num_vmas; j++){
       string vma_spec = get_vma();
       getline(myfile,buffer);
       p.insert_vma(vma_spec);
	//make vma object and add to vma vector for given proc
      }
      processes.push_back(p);
  }
 for(int k = 0; k<processes.size();k++){
   vector<VMA> vmaVec = processes.at(k).vmas;
   for(int j = 0; j<vmaVec.size();j++){
      VMA v = vmaVec.at(j);
      //     cout<<v.start_vpage<<" "<<v.end_vpage<<" "<<v.write_protected<<" "<<v.file_mapped<<endl;
   }
 }
 while(get_next_instruction(&operation,&vpage)){
   if(oflag){cout<<inst_count<<": ==> "<<operation<<" "<<vpage<<endl;}
   inst_count++;
   if(operation == 'c'){
     current_process = &processes.at(vpage);
     ctx_switches++;
     continue;
   }
   if(operation == 'e'){
     context_switch(vpage);
     process_exits++;
     continue;
   }
   if(!current_process->contains_page(vpage)){
     if(oflag){cout<<" SEGV"<<endl;}
     current_process->segv++;
     continue;
   }//is a sigv
   PTE* pte = &current_process->page_table[vpage];//
   if(!pte->present){
     FTE* new_frame = get_frame();//allocate from free pool or call pager
     if(new_frame->present == 1){ //there is occupying proc (so we didnt get from free pool)
       int old_procID = new_frame->processID;
       int old_vpage = new_frame->virtual_page;
       if(oflag){cout<<" UNMAP "<<old_procID<<":"<<old_vpage<<endl;}
       processes.at(old_procID).unmaps++;  
       PTE* pte2 = &processes.at(old_procID).page_table[old_vpage];
       if(pte2->modified == 1){
	 if(pte2->file_mapped){
      	   if(oflag){cout<<" FOUT"<<endl;}
	   processes.at(old_procID).fouts++;
	 }
	 else{
	   if(oflag){cout<<" OUT"<<endl;}
	   processes.at(old_procID).outs++;
	   pte2->pagedout = 1;
	 }
       }
       //whipe out pte2
       pte2->frame = 0;
       pte2->present = 0;
       pte2->modified = 0;
       pte2->referenced = 0;
     }
     current_process->set_bits_wf(vpage);//
       if(pte->file_mapped){
	 if(oflag){cout<<" FIN"<<endl;}
	 current_process->fins++;
       }
       if(!pte->file_mapped && pte->pagedout){
	 if(oflag){cout<<" IN"<<endl;}
	 current_process->ins++;
       }
       if(!pte->file_mapped && !pte->pagedout){
	 if(oflag){cout<<" ZERO"<<endl;}
	 current_process->zeros++;
       }
       pte->frame = new_frame->index;
       pte->present = 1;
       new_frame->present = 1;
       new_frame->processID = current_process->pid;
       new_frame->virtual_page = vpage;
       new_frame->time_used = inst_count;
       if(oflag){cout<<" MAP "<<pte->frame<<endl;}
       current_process->maps++;
   }
     //now we check operations
     if(operation == 'r'){
       pte->referenced = 1;
     }
     if(operation == 'w'){
       pte->referenced = 1;
       if(pte->write_protected){
	 if(oflag){cout<<" SEGPROT"<<endl;}
	 current_process->segprot++;
       }
       else{
	 pte->modified = 1;
       }
     }
 }
 if(pflag){
 for(int i = 0; i<processes.size(); i++){
   cout<<"PT["<<i<<"]: ";
   for(int j =0; j<64; j++){
     PTE* pt = &processes.at(i).page_table[j];
     if(processes.at(i).exited){
       cout<<"* ";
       continue;
     }
     if(!pt->present){
       if(pt->file_mapped){
	 cout<<"* ";
       }
       if(!pt->file_mapped){
	 if(pt->pagedout){
	   cout<<"# ";
	 }
	 else{
	   cout<<"* ";
	 }
       }
     }
     else{
       cout<<j<<":";
       if(pt->referenced){
	 cout<<"R";
       }
       else{
	 cout<<"-";
       }
       if(pt->modified){
	 cout<<"M";
       }
       else{
	 cout<<"-";
       }
       if(!pt->file_mapped && pt->pagedout){
	 cout<<"S ";
       }
       else{
	 cout<<"- ";
       }
     }
   }
   cout<<"\n";
 }
 }
 if(fflag){
   cout<<"FT: ";
   for(int i = 0; i<num_frames;i++){
     if(!frame_table[i].present){
       cout<<"* ";
     }
     else{
       cout<<frame_table[i].processID<<":"<<frame_table[i].virtual_page<<" ";
     }
   }
   cout<<"\n";
 }
 if(sflag){
  for(int i = 0; i<processes.size();i++){
    printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
	   i,processes.at(i).unmaps,
	   processes.at(i).maps,
	   processes.at(i).ins,
	   processes.at(i).outs,
	   processes.at(i).fins,
	   processes.at(i).fouts,
	   processes.at(i).zeros,
	   processes.at(i).segv,
	   processes.at(i).segprot);
    cost+=400*(processes.at(i).unmaps + processes.at(i).maps);
    cost+=3000*(processes.at(i).ins + processes.at(i).outs);
    cost+=2500*(processes.at(i).fins + processes.at(i).fouts);
    cost+=150*(processes.at(i).zeros);
    cost+=240*(processes.at(i).segv);
    cost+=300*(processes.at(i).segprot);
  }
  unsigned long num_access = inst_count - ctx_switches-process_exits;
  cost+= num_access;
  cost+=121*ctx_switches;
  cost+=175*process_exits;
  printf("TOTALCOST %lu %lu %lu %llu\n",inst_count, ctx_switches, process_exits, cost);
 }
 
 myfile.close();
 randfile.close();
 return 0;
}
