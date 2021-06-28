
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

static Persistent<String> shmid_symbol;
static Persistent<String> delete_symbol;
static Persistent<String> buffer_symbol;

#if (NODE_MODULE_VERSION < NODE_0_12_MODULE_VERSION)
NAN_INLINE v8::Local<v8::Value> NanThrowErrno(int errorno,
                                              const char *syscall = NULL,
                                              const char *msg = "",
                                              const char *path = NULL) {
  do {
    NanScope();
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
  Local<Value> shmid = args.This()->Get(Nan::New(shmid_symbol));
  if (shmctl(shmid->Int32Value(), IPC_RMID, NULL)) {
    return NanThrowErrno(errno, "shmctl", "can't mark segment for deletion (are you the owner?)");
  }

  Nan::ReturnUndefined();
}

/* {{{ proto int shmop_open (int key, string flags, int mode, int size)
gets and attaches a shared memory segment */
NAN_METHOD(shmop_open) {

  if (args.Length() != 4 ||
      !args[0]->IsInt32() || 
      !args[1]->IsString() || 
      !args[2]->IsInt32() || 
      !args[3]->IsInt32()) {
    return NanThrowTypeError("open() takes 4 arguments: int key, string flags, int mode, int size.");
  }

  int key      = args[0]->Int32Value();
  uint16_t flag  = **(String::Value(args[1]));
  int shmflg   = args[2]->Int32Value();
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
      size = args[3]->Int32Value();;
      break;
    case 'n':
      shmflg |= (IPC_CREAT | IPC_EXCL);
      size = args[3]->Int32Value();
      break;
    case 'w':
      /* noop
        shm segment is being opened for read & write
        will fail if segment does not exist
      */
      break;
    default:
      return NanThrowTypeError("invalid access mode");
  }

  if (shmflg & IPC_CREAT && size < 1) {
    return NanThrowTypeError("Shared memory segment size must be greater than zero");
  }

  int shmid = shmget(key, size, shmflg);
  if (shmid == -1) {
    return NanThrowErrno(errno, "shmget", "unable to attach or create shared memory segment");
  }

  if (shmctl(shmid, IPC_STAT, &shm)) {
    return NanThrowErrno(errno, "shmctl", "unable to get shared memory segment information");
  }

  void *addr = shmat(shmid, 0, shmatflg);
  if (addr == (char*) -1) {
    return NanThrowErrno(errno, "shmat", "unable to attach to shared memory segment");
  }

  Handle<Object> slowBuffer = NanNewBufferHandle((char*)addr, shm.shm_segsz);    

  Handle<Object> globalObj = NanGetCurrentContext()->Global();
  Handle<Function> bufferConstructor = Handle<Function>::Cast(
      globalObj->Get(NanNew(buffer_symbol)));
  Handle<Value> consArgs[3] = {
    slowBuffer,
    NanNew<Number>(::node::Buffer::Length(slowBuffer)),
    NanNew<Number>(0)
  };
  Handle<Object> fastBuffer = bufferConstructor->NewInstance(3, consArgs);
  fastBuffer->Set(NanNew(shmid_symbol), NanNew(shmid));
  fastBuffer->Set(NanNew(delete_symbol ), NanNew<FunctionTemplate>(shmop_delete)->GetFunction());

  NanReturnValue(fastBuffer);
}


void init(Handle<Object> exports) {
  NanScope();

  NanAssignPersistent(shmid_symbol, NanNew<String>("shmid"));
  NanAssignPersistent(delete_symbol, NanNew<String>("delete"));
  NanAssignPersistent(buffer_symbol, NanNew<String>("Buffer"));

  exports->Set(NanNew("open"), NanNew<FunctionTemplate>(shmop_open)->GetFunction());
}

NODE_MODULE(shm_buffer, init)
