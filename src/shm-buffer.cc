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
  Local<Value> shmid = Nan::Get(info[0], Nan::New(shmid_symbol)).ToLocal();
  if (shmctl(Nan::To<int32_t>(shmid), IPC_RMID, NULL)) {
    return Nan::ThrowErrno(errno, "shmctl", "can't mark segment for deletion (are you the owner?)");
  }

  Nan::ReturnUndefined();
}

/* {{{ proto int shmop_open (int key, string flags, int mode, int size)
gets and attaches a shared memory segment */
NAN_METHOD(shmop_open) {

  if (info.Length() != 4 ||
      !info[0]->IsInt32() || 
      !info[1]->IsString() || 
      !info[2]->IsInt32() || 
      !info[3]->IsInt32()) {
    return NanThrowTypeError("open() takes 4 arguments: int key, string flags, int mode, int size.");
  }

  int key      = NanTo<int32_t>(info[0]);
  uint16_t flag  = **(String::Value(info[1]));
  int shmflg   = NanTo<int32_t>(info[2]);
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
      size = NanTo<int32_t>(info[3]);;
      break;
    case 'n':
      shmflg |= (IPC_CREAT | IPC_EXCL);
      size = NanTo<int32_t>(info[3]);
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
      NanGet(globalObj, NanNew(buffer_symbol)));
  Handle<Value> consArgs[3] = {
    slowBuffer,
    NanNew<Number>(::node::Buffer::Length(slowBuffer)),
    NanNew<Number>(0)
  };
  Handle<Object> fastBuffer = NanNewInstance(bufferConstructor3, consArgs);
  NanSet(fastBuffer, NanNew(shmid_symbol), NanNew(shmid));
  NanSet(fastBuffer, NanNew(delete_symbol ), NanGetFunction(NanNew<FunctionTemplate>(shmop_delete)));

  info.GetReturnValue().Set(fastBuffer);
}


void init(Handle<Object> exports) {
  NanScope scope;

  shmid_symbol.Reset(NanNew<String>("shmid").ToLocalChecked());
  delete_symbol.Reset(NanNew<String>("delete").ToLocalChecked());
  buffer_symbol.Reset(NanNew<String>("Buffer").ToLocalChecked());

  NanSet(exports, NanNew("open").ToLocalChecked(), NanGetFunction(NanNew<FunctionTemplate>(shmop_open)));
}

NODE_MODULE(shm_buffer, init)

