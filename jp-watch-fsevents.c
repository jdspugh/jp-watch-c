#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
// Debug
//   $ gcc -g -framework CoreServices jp-watch-fsevents.c
// Production
//   $ gcc -Oz -framework CoreServices jp-watch-fsevents.c

// see https://developer.apple.com/documentation/coreservices/fseventstreamcallback?language=c
char gPathBuffer[4096];
static void callback(
  ConstFSEventStreamRef streamRef,// The stream for which event(s) occurred.
  void *clientCallBackInfo,// The info field that was supplied in the context when this stream was created.
  size_t numEvents,// The number of events being reported in this callback. Each of the arrays (eventPaths, eventFlags, eventIds) will have this many elements.
  void *eventPaths,// An array of paths to the directories in which event(s) occurred. A CFArrayRef containing CFStringRef objects (per CFStringCreateWithFileSystemRepresentation()). Ownership follows the Get rule, and they will be released by the framework after your callback returns.
  const FSEventStreamEventFlags eventFlags[],// An array of flag words corresponding to the paths in the eventPaths parameter. If no flags are set, then there was some change in the directory at the specific path supplied in this event. See FSEventStreamEventFlags.
  const FSEventStreamEventId eventIds[]// An array of FSEventStreamEventIds corresponding to the paths in the eventPaths parameter. Each event ID comes from the most recent event being reported in the corresponding directory named in the eventPaths parameter. Event IDs all come from a single global source. They are guaranteed to always be increasing, usually in leaps and bounds, even across system reboots and moving drives from one machine to another. Just before invoking your callback your stream is updated so that calling the accessor FSEventStreamGetLatestEventId() will return the largest of the values passed in the eventIds parameter; if you were to stop processing events from this stream after this callback and resume processing them later from a newly-created FSEventStream, this is the value you would pass for the sinceWhen parameter to the FSEventStreamCreate...() function.
) {
  for (int i=numEvents;i--;) {
    CFStringGetCString(CFArrayGetValueAtIndex(eventPaths,i),gPathBuffer,sizeof(gPathBuffer),kCFStringEncodingUTF8);
    printf("%u %s%s\n",(unsigned int)eventFlags[i],gPathBuffer,eventFlags[i]&kFSEventStreamEventFlagItemIsDir?"/":"");
  }
  fflush(stdout);
}

void x(char *msg){puts(msg);exit(1);}// exit (with error msg)

int main(int argc, char *argv[]) {
    if(argc<2)x("Missing directory argument");

    // This will check the path recursively and also keep track of new files/folders created within it.
    CFStringRef path=CFStringCreateWithCString(NULL,argv[1],kCFStringEncodingUTF8);
    FSEventStreamRef stream=FSEventStreamCreate(
      NULL,// memory allocator (NULL=default)
      &callback,// FSEventStreamCallback
      NULL,// context
      CFArrayCreate(NULL,(const void **)&path,1,NULL),// paths to watch
      kFSEventStreamEventIdSinceNow,// since when
      0.1,// latency (seconds)
      kFSEventStreamCreateFlagUseCFTypes// The framework will invoke your callback function with CF types rather than raw C types (i.e., a CFArrayRef of CFStringRefs, rather than a raw C array of raw C string pointers). See FSEventStreamCallback.
      | kFSEventStreamCreateFlagFileEvents// Request file-level notifications. Your stream will receive events about individual files in the hierarchy you're watching instead of only receiving directory level notifications. Use this flag with care as it will generate significantly more events than without it.
    );
    FSEventStreamSetDispatchQueue(stream,dispatch_queue_create("jp-watch",NULL));
    FSEventStreamStart(stream);

    pause();// The pause function suspends program execution until a signal arrives whose action is either to execute a handler function, or to terminate the process.
}

// 1
//   kFSEventStreamEventFlagNone   = 0x00000000,
//   kFSEventStreamEventFlagMustScanSubDirs = 0x00000001,
//   kFSEventStreamEventFlagUserDropped = 0x00000002,
//   kFSEventStreamEventFlagKernelDropped = 0x00000004,
//   kFSEventStreamEventFlagEventIdsWrapped = 0x00000008,
// 2
//   kFSEventStreamEventFlagHistoryDone = 0x00000010,
//   kFSEventStreamEventFlagRootChanged = 0x00000020,
//   kFSEventStreamEventFlagMount  = 0x00000040,
//   kFSEventStreamEventFlagUnmount = 0x00000080,
// 3
//   kFSEventStreamEventFlagItemCreated __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00000100,
//   kFSEventStreamEventFlagItemRemoved __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00000200,
//   kFSEventStreamEventFlagItemInodeMetaMod __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00000400,
//   kFSEventStreamEventFlagItemRenamed __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00000800,
// 4
//   kFSEventStreamEventFlagItemModified __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00001000,
//   kFSEventStreamEventFlagItemFinderInfoMod __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00002000,
//   kFSEventStreamEventFlagItemChangeOwner __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00004000,
//   kFSEventStreamEventFlagItemXattrMod __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00008000,
// 5
//   kFSEventStreamEventFlagItemIsFile __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00010000,
//   kFSEventStreamEventFlagItemIsDir __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00020000,
//   kFSEventStreamEventFlagItemIsSymlink __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_6_0) = 0x00040000,
//   kFSEventStreamEventFlagOwnEvent __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0) = 0x00080000,
// 6
//   kFSEventStreamEventFlagItemIsHardlink __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_9_0) = 0x00100000,
//   kFSEventStreamEventFlagItemIsLastHardlink __OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_9_0) = 0x00200000,
//   kFSEventStreamEventFlagItemCloned __OSX_AVAILABLE_STARTING(__MAC_10_13, __IPHONE_11_0) = 0x00400000
