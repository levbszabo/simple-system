#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
using namespace std;
char line[100];
int line_num = 0;
char* p = NULL;
int offset;
ifstream myfile;
string input_string;
int eofile = 0;
char delimit[]=" \t\r\v\f";
void __parseerror(int errcode){
  static string errstr[] = {
			   "NUM_EXPECTED",
			   "SYM_EXPECTED",
			   "ADDR_EXPECTED",
			   "SYM_TOO_LONG",
			   "TOO_MANY_DEF_IN_MODULE",
			   "TOO_MANY_USE_IN_MODULE",
			   "TOO_MANY_INSTR"};
  printf("Parse Error line %d offset %d: %s\n",line_num,offset,errstr[errcode].c_str());
}
void getToken(){
  if(p == NULL){
    if(myfile.eof()){
      return;
    }
    myfile.getline(line,100);
    p = strtok(line,delimit);
    offset = p-line+1;
    line_num++;
    return;
  }
  p = strtok(NULL,delimit);
  offset = p-line+1;
  return;
}
class Symbol{
public:
  string value;
  int duplicate = 0; // indicates if multiple such symbols are defined
  int module = -1;
  int wasUsed = 0;
  Symbol(string t){
    value = t;
  }
};
class Module{
public:
  int base_address;
  int code_count;
  vector<Symbol> useList;
  vector<int> symUsed;
  Module(){
    base_address = 0;
    code_count = 0;
  }
  void insert(Symbol sym){
    useList.push_back(sym);
    symUsed.push_back(0);
  }
};
int readInt(){
  getToken();
  if(p==NULL){
    if(myfile.eof()){
      __parseerror(0);
      exit(1);
    }
    return readInt();
  }
  int x;
  try{
    x = stoi(p);
  }
  catch(exception& e){
    __parseerror(0);
    exit(1);
  }
  return x;
}
Symbol readSym(){
  getToken();
  if(p==NULL){
    if(myfile.eof()){
      __parseerror(1);
      exit(1);
    }
    return readSym();
  }
  string s(p);
  if(s.length()>16){
    __parseerror(3);
    exit(1);
  }
  if(!isalpha(s[0])){
    __parseerror(1);
    exit(1);
  }
  for(int i = 1; i<s.length();i++){
    if(!isalnum(s[i])){
      __parseerror(1);
      exit(1);
    }
  }
  Symbol sym = Symbol(s);
  return sym;
}
char readIEAR(){
  getToken();
  if(p==NULL){
    if(myfile.eof()){
      cout<<"line_num"<<line_num<<endl;
      cout<<"offset"<<offset;
      __parseerror(2);
      exit(1);
    }
    return readIEAR();
  }
  string s(p);
  if(s.length()>1){
    __parseerror(2);
    exit(1);
  }
  char addressMode = *p;
  if(addressMode!='A' and addressMode!='E'
     and addressMode!='I' and addressMode!='R'){
    __parseerror(2);
    exit(1);
  }
  return addressMode;
}
int checkEOF(){
  int peek = myfile.peek();
  if(peek == -1){
    myfile.close();
    return -1;
  }
  while(peek == 10){
    int x = myfile.get();
    peek = myfile.peek();
    line_num++;
    if(peek == -1){
      myfile.close();
      return -1;
    }
  }
  return 1;
}
vector<Symbol> symbols;
vector<int> symbolAddress;
void printSymTable(){
  cout<<"Symbol Table"<<endl;
  for(int i = 0; i<symbols.size();i++){
    Symbol sym = symbols.at(i);
    int absAddress = symbolAddress.at(i);
    if(sym.duplicate == 1){
      cout<<sym.value<<"="<<absAddress<<"  Error: This variable is multiple times defined; first value used"<<endl;
    }
    else{
      cout<<sym.value<<"="<<absAddress<<endl;
    }
  }
  cout<<endl;
  
}
int symAddress(Symbol sym){
  for(int i = 0; i<symbols.size();i++){
    if(sym.value==symbols.at(i).value){
      return symbolAddress.at(i);
    }
  }
  return -1;
}
void pass_1(){
  vector<Module> modules;
  myfile.open(input_string);
  while(!myfile.eof()){
    int eofile = checkEOF();
    if(eofile == -1){return;}
    Module m;
    int n = modules.size();
    if(n>0){
      int base = modules.at(n-1).base_address+modules.at(n-1).code_count;
      m.base_address = base;
    }
    int defcount = readInt();
    if(defcount>16){
      __parseerror(4);
      exit(1);
    }
    for(int i = 0; i<defcount;i++){
      Symbol sym = readSym();
      int rel_address = readInt();
      int abs_address = rel_address + m.base_address;
      int address = symAddress(sym);
      for(int i =0; i<symbols.size();i++){
	if(symbols.at(i).value == sym.value){//we already have in sym table
	  Symbol symb = symbols.at(i);
	  symb.duplicate = 1;
	  symbols.at(i) = symb;
	}
      }
      if(symAddress(sym)==-1){
	sym.module = n+1;
	symbols.push_back(sym);        //insert symbol into table
	symbolAddress.push_back(abs_address);  //insert abs adress of symbol
      }
    }
    int usecount = readInt();
    if(usecount>16){
      __parseerror(5);
      exit(1);
    }
    for(int i = 0; i<usecount;i++){
      Symbol sym = readSym();
      m.insert(sym); //insert symbol into modules useList
    }
    int code_count = readInt();
    if(code_count+m.base_address>=512){
      __parseerror(6);
      exit(1);
    }
    m.code_count = code_count;
    for(int i=0; i<code_count;i++){
      char addressMode = readIEAR();
      int instr = readInt();
      // error checking
      int opcode = instr/1000;
      int operand = instr%1000;
    }
    for(int i =0; i<symbols.size();i++){
      if(symbols.at(i).module==n+1){
	int absAddress = symbolAddress.at(i);
	if(absAddress>=m.base_address+code_count){
	  cout<<"Warning: Module "<<n+1<<": "<<symbols.at(i).value<< " too big "<<absAddress<< " (max="<<m.base_address+code_count-1<<") assume zero relative"<<endl;
	  symbolAddress.at(i)=m.base_address;
	}
      }
    }
    modules.push_back(m);
  }
  myfile.close();
}
void pass_2(){
  vector<Module> modules;
  int instruction_id = 0;
  myfile.open(input_string);
  cout<<"Memory Map"<<endl;
  while(!myfile.eof()){
    int eofile = checkEOF();
    if(eofile == -1){return;}
    Module m;
    int n = modules.size();
    if(n>0){
      int base = modules.at(n-1).base_address+modules.at(n-1).code_count;
      m.base_address = base;
    }
    int defcount = readInt();
    for(int i = 0; i<defcount;i++){
      Symbol sym = readSym();
      int rel_address = readInt();
      int abs_address = rel_address + m.base_address;
    }
    int usecount = readInt();
    for(int i = 0; i<usecount;i++){
      Symbol sym = readSym();
      m.insert(sym); //insert symbol into modules useList
    }
    int code_count = readInt();
    m.code_count = code_count;
    for(int i=0; i<code_count;i++){
      char addressMode = readIEAR();
      int instr = readInt();
      // error checking
      int opcode = instr/1000;
      int operand = instr%1000;
      string error_msg = "";
      if(addressMode=='R'){
	instr = instr+m.base_address;
	if(instr%1000>=m.base_address+m.code_count){
	  error_msg = "Error: Relative address exceeds module size; zero used";
	  instr = opcode*1000+m.base_address;
	}
      }
      if(addressMode=='E'){
	if(operand>=usecount){
	  error_msg = "Error: External address exceeds length of uselist; treated as immediate";

	}
	else{
	  Symbol sym = m.useList.at(operand);
	  int address = symAddress(sym);
	  if(address==-1){  
	    error_msg = "Error: "+sym.value + " is not defined; zero used";
	    address = 0;
	    m.symUsed.at(operand)=1;
	  }
	  else{
	    m.symUsed.at(operand)=1; //denotes uselist member was used
	    for(int i =0 ;i<symbols.size();i++){
	      if(symbols.at(i).value==sym.value){
		Symbol sym = symbols.at(i);
		sym.wasUsed = 1;
		symbols.at(i) = sym;
		//denotes symbol def in some module was used
	      }
	    }
	    //find symbol in symbols at set sym.wasUsed =1
	  }
	  instr = 1000*opcode+address;
	}
      }
      if(addressMode=='A'){
	if(operand>=512){
	  instr = 1000*opcode;
	  error_msg = "Error: Absolute address exceeds machine size; zero used";
	}
      }
      if(addressMode=='I'){
	if(instr >=10000){
	  instr = 9999;
	  error_msg = "Error: Illegal immediate value; treated as 9999"; 
	}
      }
      if(instr>9999){
	instr = 9999;
	error_msg = "Error: Illegal opcode; treated as 9999";
      }
      string out_string = to_string(instruction_id);
      if(instruction_id<10){
	out_string = "00"+to_string(instruction_id);
      }
      else if(instruction_id<100){
	out_string = "0"+to_string(instruction_id);
      }
      if(instr<=999){
	if(instr<=9){
	  cout<<out_string<<": "<<"000"<<instr<<" "<<error_msg<<endl;
	}
	else if(instr<=99){
	  cout<<out_string<<": "<<"00"<<instr<<" "<<error_msg<<endl;
	}
	else{
          cout<<out_string<<": "<<"0"<<instr<<" "<<error_msg<<endl;
	}
      }
      else{
	cout<<out_string<<": "<<instr<<" "<<error_msg<<endl;
      }
      instruction_id++;
    }
    for(int i =0 ;i<m.symUsed.size();i++){
      if(m.symUsed.at(i)==0){
	cout<<"Warning: Module "<<n+1<<": "<<m.useList.at(i).value<<" appeared in the uselist "<<
	  "but was not actually used"<<endl;
      }

    }
    modules.push_back(m);
  }
  
  myfile.close();
}
int main(int argc, char *argv[]){
  input_string = argv[1];
  pass_1();
  printSymTable();
  pass_2();
  for(int i =0; i<symbols.size();i++){
    if(symbols.at(i).wasUsed==0){
      Symbol sym = symbols.at(i);
      cout<<endl;
      cout<<"Warning: Module "<<sym.module<<": "<<sym.value<<" was defined but never used";       
    }
  }
  cout<<"\n\n";
  return 0;
}


