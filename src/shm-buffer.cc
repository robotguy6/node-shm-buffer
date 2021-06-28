#include <v8.h>
#include <node.h>
#include <nan.h>
#include <node_buffer.h>
#include <iostream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string>
#include <errno.h>

using namespace v8;

static Nan::Persistent<String> shmid_symbol;
static Nan::Persistent<String> delete_symbol;
static Nan::Persistent<String> buffer_symbol;

#if (NODE_MODULE_VERSION < NODE_0_12_MODULE_VERSION)
NAN_INLINE v8::Local<v8::Value> NanThrowErrno(int errorno,
                                              const char *syscall = NULL,
                                              const char *msg = "",
                                              const char *path = NULL) {
  do {
    NanScope scope;
    return v8::Local<v8::Value>::New(node::ErrnoException(errorno, syscall, msg, path));
  } while (0);
}
#else
NAN_INLINE void NanThrowErrno(int errorno,
                              const char *syscall = NULL,
                              const char *msg = "",
                              const char *path = NULL) {
  do {
    v8::Isolate::GetCurrent()->ThrowException(node::ErrnoException(errorno, syscall, msg, path));
  } while (0);
}
#endif

/* {{{ proto bool shmop_delete ()
   mark segment for deletion */
NAN_METHOD(shmop_delete) {
  Local<Value> shmid = Nan::Get(info[0], Nan::New(shmid_symbol)).ToLocalChecked();
  if (shmctl(Nan::To<int32_t>(shmid).ToChecked(), IPC_RMID, NULL)) {
     Nan::ThrowError(Nan::ErrnoException(errno, "shmctl", "can't mark segment for deletion (are you the owner?)"));
  }
}

/* {{{ proto int shmop_open (int key, string flags, int mode, int size)
gets and attaches a shared memory segment */
NAN_METHOD(shmop_open) {

  if (info.Length() != 4 ||
      !info[0]->IsInt32() || 
      !info[1]->IsString() || 
      !info[2]->IsInt32() || 
      !info[3]->IsInt32()) {
      Nan::ThrowError("open() takes 4 arguments: int key, string flags, int mode, int size.");
  }

  int key      = Nan::To<int32_t>(info[0]).ToChecked();
  uint16_t flag  = **(String::Value(v8::Isolate::GetCurrent(), info[1]));
  int shmflg   = Nan::To<int32_t>(info[2]).ToChecked();
  int shmatflg = 0;
  int size     = 0;
  struct shmid_ds shm;

  switch (flag)
  {
    case 'a':
      shmatflg |= SHM_RDONLY;
      break;
    case 'c':
      shmflg |= IPC_CREAT;
      size = Nan::To<int32_t>(info[3]).ToChecked();
      break;
    case 'n':
      shmflg |= (IPC_CREAT | IPC_EXCL);
      size = Nan::To<int32_t>(info[3]).ToChecked();
      break;
    case 'w':
      /* noop
        shm segment is being opened for read & write
        will fail if segment does not exist
      */
      break;
    default:
      Nan::ThrowError("invalid access mode");
  }

  if (shmflg & IPC_CREAT && size < 1) {
     Nan::ThrowError("Shared memory segment size must be greater than zero");
  }

  int shmid = shmget(key, size, shmflg);
  if (shmid == -1) {
      Nan::ThrowError(Nan::ErrnoException(errno, "shmget", "unable to attach or create shared memory segment"));
  }
  
  if (shmctl(shmid, IPC_STAT, &shm)) {
      Nan::ThrowError(Nan::ErrnoException(errno, "shmctl", "unable to get shared memory segment information"));
  }

  void *addr = shmat(shmid, 0, shmatflg);
  if (addr == (char*) -1) {
      Nan::ThrowError(Nan::ErrnoException(errno, "shmat", "unable to attach to shared memory segment"));
  }

  /*
  Handle<Object> slowBuffer = NanNewBufferHandle((char*)addr, shm.shm_segsz);    

  Handle<Object> globalObj = Nan::GetCurrentContext()->Global();
  Handle<Function> bufferConstructor = Handle<Function>::Cast(Nan::Get(globalObj, Nan::New(buffer_symbol)));
  Handle<Value> consArgs[3] = {
    slowBuffer,
    Nan::New<Number>(::node::Buffer::Length(slowBuffer)),
    Nan::New<Number>(0)
  };
  Handle<Object> fastBuffer = Nan::NewInstance(bufferConstructor3, consArgs);
  Nan::Set(fastBuffer, Nan::New(shmid_symbol), Nan::New(shmid));
  Nan::Set(fastBuffer, Nan::New(delete_symbol ), Nan::GetFunction(Nan::New<FunctionTemplate>(shmop_delete)));

  info.GetReturnValue().Set(fastBuffer);
  */
}


void init(Handle<Object> exports) {
  NanScope scope;

  shmid_symbol.Reset(Nan::New<String>("shmid").ToLocalChecked());
  delete_symbol.Reset(Nan::New<String>("delete").ToLocalChecked());
  buffer_symbol.Reset(Nan::New<String>("Buffer").ToLocalChecked());

  Nan::Set(exports, Nan::New("open").ToLocalChecked(), Nan::GetFunction(Nan::New<FunctionTemplate>(shmop_open)));
}

NODE_MODULE(shm_buffer, init)

