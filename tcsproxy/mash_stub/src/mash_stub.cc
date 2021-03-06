#include <tk.h>
#include <tcl.h>
#include "distinterface.h"
#include "threads.h"

extern "C" int Smash_AppInit(Tcl_Interp *interp);


/*extern "C" {
  struct OTclClass;
  extern struct OTclClass* OTclGetClass(Tcl_Interp* in, char* name);

  extern struct OTclClass* OTclCreateClass(Tcl_Interp* in, char* name, 
					   struct OTclClass* cl);
}*/


class MashStub {
public:
  MashStub();
  DistillerStatus DistillerInit_tacc(C_DistillerType distType, 
				     int argc, const char * const * argv);

  DistillerStatus DistillerMain_tacc(Argument *args, int numberOfArgs,
				     DistillerInput *input, 
				     DistillerOutput *output);

  void Signal(int fd, const char *string);
  void Wait(int fd, char *string=NULL, int len=0);

  static MashStub *this_;

private:
  void DistillerInit_mash(char *retval);
  void DistillerMain_mash(char *retval);
  void DistillerMain_create_args();


  static void  Abort();
  static void* TclMain_static(void *dummy);
  static int   Tcl_AppInit_static(Tcl_Interp *interp);
  static void  Tcl_ReadHandler_static(ClientData cd, int mask);

  void CreateListElement(Tcl_DString *bufPtr, char *string, int len);

  void MakeCopy(char *&var, const char *str) {
    var = new char [strlen(str)+1];
    strcpy(var, str);
  }

  Thread thread_;
  int taccReadFD_, taccWriteFD_, mashReadFD_, mashWriteFD_;
  C_DistillerType distType_;
  Tcl_Interp *interp_;
  //OTclClass  *otclClass_;

  int argc_;
  char **argv_;

  Argument *args_;
  int numberOfArgs_;
  DistillerInput *input_;
  DistillerOutput *output_;

  char *distMainResult_;
  int distMainResultLen_;
};


static MashStub stub;
MashStub *MashStub::this_;


MashStub::MashStub()
  : taccReadFD_(-1), taccWriteFD_(-1), mashReadFD_(-1), mashWriteFD_(-1), 
    interp_(NULL), /*otclClass_(NULL),*/ argc_(0), argv_(NULL), 
    input_(NULL), output_(NULL),
    distMainResult_(NULL), distMainResultLen_(0)
{
  this_ = this;
}


MashStub::~MashStub()
{
  if (distMainResult_!=NULL) Tcl_Free(distMainResult_);
}


void
MashStub::Abort()
{
  fprintf(stderr, "%s: ", this_->interp_->result);
  fprintf(stderr, "%s\n", Tcl_GetVar(this_->interp_, "errorInfo", 
				     TCL_GLOBAL_ONLY));
  Tcl_Exit(-1);
}


void
MashStub::Signal(int fd, const char *string)
{
  write(fd, string, strlen(string)+1);
}


void
MashStub::Wait(int fd, char *string, int len)
{
  char *ptr = string, dummy;
  int bytesRead=0;

  if (string==NULL || len<=0) ptr = &dummy;

  while( read(fd, ptr, 1)!=-1 || errno==EBADF || errno==EINTR ) {
    if (*ptr=='\0') break;
    if (ptr != &dummy) {
      ptr++;
      bytesRead++;
      if (bytesRead >= len) ptr = &dummy;
    }
  }
}


DistillerStatus
MashStub::DistillerInit_tacc(C_DistillerType distType, 
			     int argc, const char * const * argv)
{
  int fd[2];

  // create the pipes for communication with the tcl thread
  if (pipe(fd)==-1) return distFatalError;
  taccReadFD_  = fd[0];
  mashWriteFD_ = fd[1];

  if (pipe(fd)==-1) {
    close(taccReadFD_);
    close(mashWriteFD_);
    return distFatalError;
  }
  mashReadFD_  = fd[0];
  taccWriteFD_ = fd[1];

  distType_ = distType;

  argv_ = new char * [argc+5];
  argc_ = 0;
  memset(argv_, 0, (argc+5)*sizeof(char*));
  MakeCopy(argv_[argc_++], "smash");
  for (int i=0; i < argc; i++) {
    MakeCopy(argv_[argc_++], argv[i]);
  }

  // fork off the tcl thread
  thread_.Fork(TclMain_static, NULL);

  char retval[256], *end;
  int result;

  Signal(taccWriteFD_, "DistillerInit");
  Wait(taccReadFD_, retval, sizeof(retval));

  result = strtoul(retval, &end, 10);
  if (*end!='\0') {
    return distFatalError;
  } else {
    return (DistillerStatus) result;
  }
}


void 
MashStub::DistillerInit_mash(char *retval)
{
  static char string[1024];
  sprintf(string, "DistillerInit %s {%s}", 
	  GET_DISTILLER_TYPE(distType_),
	  Tcl_GetVar(interp_, "argv", TCL_GLOBAL_ONLY));
  if (Tcl_GlobalEval(interp_, string)!=TCL_OK) {
    Abort();
  }

  if (strcmp(interp_->result, "")==0) {
    sprintf(retval, "%d", distOk);
  }
  else {
    strcpy(retval, interp_->result);
  }
}


void*
MashStub::TclMain_static(void */*dummy*/)
{
  Tcl_Main(this_->argc_, this_->argv_, Tcl_AppInit_static);
  return NULL;
}


int 
MashStub::Tcl_AppInit_static(Tcl_Interp *interp)
{
  if (Smash_AppInit(interp)==TCL_ERROR) return TCL_ERROR;

  // create Tcl channels for the communication pipes with tacc
  Tcl_File tclFD  = Tcl_GetFile((void*)this_->mashReadFD_,  TCL_UNIX_FD);
  Tcl_CreateFileHandler(tclFD, TCL_READABLE, Tcl_ReadHandler_static, 
			NULL);

  /* // create the OTcl class for MashStub
  struct OTclClass* class_obj = OTclGetClass(interp, "Class");
 
  if (!class_obj) return TCL_ERROR;
  this_->otclClass_ = OTclCreateClass(interp, "MashStub", class_obj);
  if (!this_->otclClass_) return TCL_ERROR;*/

  this_->interp_ = interp;
  return TCL_OK;

  //OTclAddPMethod(this_->otclClass_, "DistillerInit_Done", DistillerInit_Done,
  //0, 0);
  //return Tcl_GlobalEval(interp, "after idle {MashStub DistillerInit_Done}");
}


void 
MashStub::Tcl_ReadHandler_static(ClientData /*cd*/, int /*mask*/)
{
  static char string[256], retval[256];
  this_->Wait(this_->mashReadFD_, string, sizeof(string));

  if (strcmp(string, "DistillerInit")==0) {
    this_->DistillerInit_mash(retval);
  }
  else if (strcmp(string, "DistillerMain")==0) {
    this_->DistillerMain_mash(retval);
  }
  else {
    static char errorStr[256];
    sprintf(errorStr, "Cannot understand message from TACC worker: %s", 
	    string);
    Tcl_SetResult(this_->interp_, errorStr, TCL_STATIC);
    Abort();
  }

  this_->Signal(this_->mashWriteFD_, retval);
}


DistillerStatus 
MashStub::DistillerMain_tacc(Argument *args, int numberOfArgs,
			     DistillerInput *input, 
			     DistillerOutput *output)
{
  args_ = args;
  numberOfArgs_ = numberOfArgs;
  input_  = input;
  output_ = output;

  char retval[256], *end;
  int result;

  Signal(taccWriteFD_, "DistillerMain");
  Wait(taccReadFD_, retval, sizeof(retval));

  result = strtoul(retval, &end, 10);
  if (*end!='\0') {
    return distFatalError;
  } else {
    return (DistillerStatus) result;
  }
}


void
MashStub::DistillerMain_create_args()
{
  int idx;
  Argument *argPtr;
  char id[256], value[256], argsName[]="DistillerMain_Args__";

  for (idx=0, argPtr = args_; idx < numberOfArgs_; idx++, argPtr++) {
    switch(ARG_TYPE(*argPtr)) {
    case typeInt:
      sprintf(id, "i%ld", ARG_ID(*argPtr));
      sprintf(value, "%ld", ARG_INT(*argPtr));
      Tcl_SetVar2(interp_, argsName, id, value, TCL_GLOBAL_ONLY);
      break;
    case typeString:
      sprintf(id, "s%ld", ARG_ID(*argPtr));
      Tcl_SetVar2(interp_, argsName, id, ARG_STRING(*argPtr), TCL_GLOBAL_ONLY);
      break;
    case typeDouble:
      sprintf(id, "f%ld", ARG_ID(*argPtr));
      sprintf(value, "%g", ARG_DOUBLE(*argPtr));
      Tcl_SetVar2(interp_, argsName, id, value, TCL_GLOBAL_ONLY);
      break;
    }
  }
}


void
MashStub::CreateListElement(Tcl_DString *bufPtr, char *string, int len)
{
  char *ptr = string, *start = string, special[3] = "\\ ";

  Tcl_DStringStartSublist(bufPtr);

  while ((ptr-string) < len && *ptr!='\0') {
    if (*ptr=='{') {
      if (start!=ptr) {
	Tcl_DStringAppend(bufPtr, start, (ptr-start));
	start = ptr+1;
      }

      special[1] = *ptr;
      Tcl_DStringAppend(bufPtr, special, 1);
    }
    ptr++;
  }

  if (start!=ptr) Tcl_DStringAppend(bufPtr, start, (ptr-start));
  Tcl_DStringEndSublist(bufPtr);
}



void 
MashStub::DistillerMain_mash(char *retval)
{
  Tcl_DString buf;

  DistillerMain_create_args();

  Tcl_DStringInit(&buf);
  Tcl_DStringAppendElement(&buf, "DistillerMain");

  CreateListElement(&buf, (char*) DataPtr(input_), DataLength(input_));
  CreateListElement(&buf, (char*) MetadataPtr(input_), MetadataLength(input_));

  Tcl_DStringAppendElement(&buf, (char*) MimeType(input_));
  Tcl_DStringAppendElement(&buf, "DistillerMain_Args__");

  if (Tcl_GlobalEval(interp_, Tcl_DStringValue(&buf)) != TCL_OK) {
    Abort();
    //Tcl_DStringFree(&buf);
    //return;
  }

  Tcl_DStringFree(&buf);

  /*if (strcmp(interp_->result, "")==0) {
    sprintf(retval, "%d", distOk);

    SetData(output_, DataPtr(input_));
    SetDataLength(output_, DataLength(input_));
    DataNeedsFree(output_, gm_False);

    SetMetadata(output_, MetadataPtr(input_));
    SetMetadataLength(output_, MetadataLength(input_));
    MetadataNeedsFree(output_, gm_False);

    SetMimeType(output_, MimeType(input_));
  }
  else {*/
    int argc, len;
    char **argv, *str;

    len = strlen(interp_->result) + 1;
    if (len > distMainResultLen_) {
      distMainResult_ = Tcl_Realloc(distMainResult_, len);
      distMainResultLen_ = len;
    }

    strcpy(distMainResult_, interp_->result);
    Tcl_SplitList(interp_, distMainResult_, &argc, &argv);
    if (argc!=4) {
      static char errorStr[256];
      sprintf(errorStr, "MashStub::DistillerMain should return a list "
	      "of 4 elements. got %d elements", argc);
      Tcl_SetResult(this_->interp_, errorStr, TCL_STATIC);
      Abort();
      return;
    }

    strcpy(retval, argv[3]);

    len = strlen(argv[0]) + 1;
    str = (char*) DistillerMalloc(len);
    strcpy(str, argv[0]);
    SetData(output_, str);
    SetDataLength(output_, len);
    DataNeedsFree(output_, gm_True);

    len = strlen(argv[1]) + 1;
    str = (char*) DistillerMalloc(len);
    strcpy(str, argv[1]);
    SetMetadata(output_, str);
    SetMetadataLength(output_, len);
    MetadataNeedsFree(output_, gm_True);

    SetMimeType(output_, argv[2]);

    Tcl_Free((char *) argv);
    //}
}




DistillerStatus 
DistillerInit(C_DistillerType distType, 
	      int argc, const char * const * argv)
{
  return stub.DistillerInit_tacc(distType, argc, argv);
}


DistillerStatus
DistillerMain(Argument *args, int numberOfArgs,
	      DistillerInput *input, DistillerOutput *output)
{
  return MashStub::this_->DistillerMain_tacc(args, numberOfArgs, input, 
					     output);
}
