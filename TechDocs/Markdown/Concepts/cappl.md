## 6 Applications and Geodes

This chapter discusses the life of an application as well as several topics most 
application programmers will want to cover at one time or another. Before 
reading this chapter, you should have read each of the previous chapters; this 
chapter builds on the knowledge you gained in those chapters. 

This chapter details the components and features of a geode, a GEOS 
executable. The chapter is presented in the following five sections:

+ Geodes  
The Geodes section describes how libraries, drivers, and applications are 
launched, used, and shut down. It also discusses the general sections of 
a geode and the geode's executable file.

+ Creating Icons  
The Creating Icons section describes the structure of the Token 
Database, how icons are stored, managed, and created.

+ Saving User Options  
The Saving Options section describes how to work with the GEOS.INI file.

+ General System Utilities  
The General System Utilities section describes several different 
mechanisms provided by GEOS that you may find useful in your 
application or other geode.

+ The Error-Checking System Software  
The Error-Checking System Software describes the differences between 
the debugging and standard versions of the system software. There are 
two versions of nearly every part of the system software including the 
kernel, the UI, and most libraries and drivers.

+ IACP: Inter-Application Communication Protocol  
The IACP section describes how applications can communicate with each 
other and pass data back and forth. This protocol works for applications 
that are not necessarily loaded and running; thus, an application can 
change another application's data dynamically-the recipient 
application is automatically started if it is not already running. (This is 
an advanced topic; most programmers will not need to read this section.)

### 6.1 Geodes

Geode is the term used to describe a GEOS executable. Just as DOS has 
executables (programs) that reside in files on a disk, so too does GEOS. GEOS 
executables normally have the filename extension .GEO.

Each geode may have up to three different aspects:

+ process  
Most of your geodes will have this aspect. A geode that is a process has 
an initial event-driven thread started for it by the kernel. All applications 
and some libraries will have this aspect.

+ library  
This aspect indicates that the geode exports entry points for other geodes 
to use. Typically, these entry points describe where either object classes 
or code for routines is within the geode. A library has a special routine 
(the library's entry point) that is called by the system when the library or 
one of its clients is loaded or in the process of being unloaded.

+ driver  
This aspect indicates that the geode hides the details of some device or 
similarly changeable thing from the system. A driver has a special 
structure that defines its type, and it has a single entry point called a 
strategy routine. All calls to the driver pass through this strategy routine.

A geode can have any combination of these aspects. For example, the print 
spooler is a process-library (and therefore provides routines for other geodes 
while also having a thread of its own), but the sound library is actually a 
library-driver since it manipulates the machine's sound hardware.

Library and driver geodes that do not have the process aspect do not initially 
have event-driven threads. Therefore, typically they will not contain objects, 
just procedural code. They can contain objects, however, as any geode is free 
to create an event-driven thread for itself at any time. In fact, the parallel 
port driver does just that when it is printing to a port through DOS or BIOS.

Geodes are loaded either with a call to the kernel routine **GeodeLoad()** or 
as a side effect of a library's client being loaded (in that case, the library geode 
will be loaded as well). The generic UI supplies the special routine 
**UserLoadApplication()**, which you may use to load an application-a 
geode which has both a process aspect and its process class subclassed off of 
**GenProcessClass** (and therefore can put generic UI objects on the screen).

Once a geode has been loaded, it is identified by its geode handle, which is the 
memory handle of the block that holds all the geode's system-administrative 
data. This block is called the core block and should not be accessed by 
anything other than the kernel. The geode handle is also used to determine 
the owner of a particular block in memory; when queried for the owner of a 
particular block, the kernel will return the geode handle of the geode that 
owns that block. A geode is the only entity that may own a system resource. 
If the geode is a process, the geode handle may also be known as a process 
handle.

When a geode is loaded, its core block is connected to a linked list of the core 
blocks of other geodes running in the system. This linked list is chronological, 
with the first entry belonging to the first geode loaded and the last entry 
belonging to the most recent geode loaded. Each core block contains an entry 
for the handle of the next core block in the list; the kernel can follow these 
links to locate any geode in the system. (Only the kernel may do this.)

After the core block is appended to the list, GEOS scans the list for other 
instances of the same core block. If the geode has been loaded more than once, 
it will have multiple instances in the list (one instance of the core block for 
each time the geode is loaded; each core block references the same copy of the 
geode, however). GEOS then copies the shared-resource handles from an 
existing core block (if found) into the new core block, thus reducing the 
amount of work required to load a particular geode multiple times (the 
shared resources do not need to be reloaded or recreated). Non-shared 
resource handles are not copied; the resources are loaded or constructed as 
necessary.

Each geode's core block contains a reference count for that particular geode. 
When the geode is first loaded, the reference count is set to one. If the geode 
is a process, the act of initializing the process thread increments the 
reference count. Each time the geode is loaded again, the new core block will 
get its own reference count. If the geode is loaded implicitly (as a library, with 
**GeodeUseLibrary()**, or with **GeodeUseDriver()**), or if it spawns a new 
thread, it will receive yet another reference count.

The reference count is decremented when a thread owned by the geode exits. 
If a client of a library geode exits, the library's reference count goes down by 
one.

When a geode's reference count reaches zero, all the geode's non-sharable 
resources are freed along with all the file, event, and timer handles owned by 
the geode. If a sharable resource is co-owned by another instance of the 
geode, ownership is transferred to the geode's next-oldest instance. (Shared 
resources are always owned by the oldest instance of their geode.) Once the 
resources have been freed or transferred, the core block is removed from the 
linked list and is freed.

To make sure no synchronization problems occur while updating the core 
block list (e.g. a geode is being loaded while it has just been freed), GEOS 
maintains an internal semaphore. The geode loading and freeing routines 
automatically maintain this semaphore.

#### 6.1.1 Geode Components and Structures

A geode is simply a special type of GEOS file. It has a special file header that 
gets loaded in as the geode's core block. This file header contains the geode's 
type, attributes, release and protocol levels, and many other pieces of 
information necessary for GEOS to work with the geode. You never will have 
to know the exact structure of this header as the kernel provides routines 
necessary to access important portions of it.

Several important items contained in the header are listed below.

+ Core Block Handle  
The core block contains its own memory handle, filled in when the geode 
is loaded into memory.

+ Geode Attributes  
Each geode has a record of type **GeodeAttrs**. The geode attributes are 
described below in [section 6.1.1.1](#6111-geode-attributes).

+ Release and Protocol Levels  
Each geode can have release and protocol levels associated with it to 
ensure compatibility between different versions. Release and protocol 
levels are discussed in [section 6.1.8.2](#6182-protocol-numbers).

+ Geode Name
Each geode has a geode name and extension specified in the geode 
parameters (.gp) file.

+ Geode Token  
Every geode has a token associated with it in the token database. The 
token describes the geode's icon and is a structure of type **GeodeToken**. 
Tokens and icons are discussed in [section 6.2](#62-creating-icons).

+ Geode Reference Count  
The geode's reference count is stored in the core block. See above for a 
discussion of the reference count and how it's managed.

+ Geode File Handle  
The file handle of the geode identifies the open GEO file so the kernel can 
locate, load, and access its various resources.

+ Geode Process Handle  
Each geode that has a parent process will have the handle of the process 
in its core block. This is the handle of the parent process' core block. (The 
parent process is the owner of the thread that loaded the geode. It is 
notified when the geode exits, if the exiting geode is a process.)

+ Handle of the Next Geode  
Because geode core blocks are stored in a linked list, each core block must 
contain a reference to the next geode in the list.

+ Handle of Private Data Space  
Each geode that is not the kernel can have a "private data space." This 
private data space is discussed in [section 6.1.9](#619-temporary-geode-memory).

+ Resource, Library, and Driver Information  
Each geode that imports or exports library or driver information must 
keep the import and export specifics available. Additionally, each geode 
must keep track of the resources it owns. All this information is stored in 
tables referenced from within the core block.

##### 6.1.1.1 Geode Attributes

Each geode has in its core block a record of type **GeodeAttrs**. This record 
defines several things about the geode, including which aspects it uses and 
which of its aspects have been initialized. The **GeodeAttrs** record contains 
one bit for each of the following attributes.

GA_PROCESS  
This geode has a process aspect and therefore an initial 
event-driven thread.

GA_LIBRARY  
This geode has a library aspect and therefore exports routines 
(and possibly object classes).

GA_DRIVER  
This geode has a driver aspect and therefore has a driver table, 
in which the strategy routine is specified.

GA_KEEP_FILE_OPEN  
This geode must have its .GEO file kept open because its 
resources may be discardable or are initially discarded.

GA_SYSTEM  
This geode is a privileged geode and is almost certainly a 
system-used driver. These geodes have special exit 
requirements.

GA_MULTI_LAUNCHABLE  
This geode may be loaded more than once and therefore may 
have more than one instance of its core block in memory.

GA_APPLICATION  
This geode is a user-launchable application.

GA_DRIVER_INITIALIZED  
This flag is set if the geode has had its driver aspect initialized 
(if the driver's strategy routine has been initialized). This flag 
will be set dynamically by the kernel.

GA_LIBRARY_INITIALIZED  
This flag is set if the geode has had its library aspect initialized 
(if the library's entry routine has been called). This flag will be 
set dynamically by the kernel.

GA_GEODE_INITIALIZED  
This flag is set if all aspects of the geode have been initialized.

GA_USES_COPROC  
This geode uses a math coprocessor if one is available.

GA_REQUIRES_COPROC  
This geode requires the presence of a math coprocessor or a 
coprocessor emulator.

GA_HAS_GENERAL_CONSUMER_MODE  
This geode may be run in the General Consumer (appliance) 
Mode.

GA_ENTRY_POINTS_IN_C  
This geode has its library entry routine in C rather than 
assembly language.

##### 6.1.1.2 Geode Token

As stated above, every geode is associated with a token in the token database. 
This token is defined by the use of a **GeodeToken** structure. This structure 
and its uses are discussed in [section 6.2](#62-creating-icons).

#### 6.1.2 Launching an Application

GeodeLoad(), UserLoadApplication(), 
MSG_GEN_PROCESS_OPEN_APPLICATION

An application is a geode with its GA_APPLICATION attribute set. This type 
of geode may be launched by the user through GeoManager or some other 
means provided by the system or another application. For the most part, the 
system will invoke and carry out the launch; your responsibilities are 
limited.

An application may be loaded in essentially two ways: It may be launched, or 
it may be reloaded from a state file. In both cases, the kernel will load the 
proper resources and build out the UI properly according to the application.

Most of the procedure of launching is handled within **GenProcessClass**, a 
subclass of **ProcessClass**. An application should define its own subclass of 
**GenProcessClass** for its Process objects (event-driven threads not acting as 
Process objects should be subclassed from **ProcessClass**, not 
**GenProcessClass**). The launch procedure may be invoked by any thread 
and by any geode in either of the following ways:

+ **GeodeLoad()**  
This loads a geode from a given file and begins executing it based on the 
geode's type. **GeodeLoad()** takes the name of the geode's file as well as 
a priority to set for the new geode's process thread, if it has one. 
**GeodeLoad()** first creates the process thread of the application, then 
sends this thread a message. The process thread (a subclass of 
**GenProcessClass**) then creates a UI thread for the application.  
**GeodeLoad()** will have to create two threads for the application: one for 
the process and one for the UI of the geode.

+ **UserLoadApplication()**  
Used by most application launchers, this routine loads a geode from the 
standard GEOS application directory. (C programmers will generally use 
MSG_GEN_PROCESS_OPEN_APPLICATION instead.) This routine takes 
some additional parameters and can load a geode either in engine mode 
or from a state file as well as in the normal open mode (see below). The 
base functionality of opening and loading the geode is implemented in 
this routine by a call to **GeodeLoad()**. Note, however, that this routine 
may only open application geodes-geodes with the GA_APPLICATION 
attribute set.

Geodes may be launched in three modes:

+ Application mode launches the geode, loads all its active resources, 
builds its UI gadgetry, and sets it usable. The geode must be an 
application-that is, it must have its GA_APPLICATION attribute set.

+ Engine mode launches the geode but leaves its UI objects unusable (it 
never brings them on-screen). Engine mode is useful if you need to 
launch an application to grab information from it. GeoManager uses 
engine mode to launch an application, extract its icon, and put the icon 
in the token database. For efficiency, the application is never set usable, 
so its UI is never built.

+ Restore mode launches the geode from a saved state file, loading in 
resources and merging them with the default resources. This mode is 
invoked automatically by the UI if it is restoring the system or the geode 
from saved state. This mode is handled automatically by 
**GenProcessClass** (your Process object).

It is possible, however, for one application to launch another in a custom 
mode. If this is done, the application being launched is responsible for 
implementing the special mode.

When the launch process has been initiated with the above routines, a thread 
is created for the application and its Process object is loaded immediately. 
Also loaded is the application's GenApplication object. The Application and 
Process objects interact with the User Interface to load the objects on the 
application's active list and set them all usable, bringing them up on-screen.

Near the end of this procedure, just before the GenApplication is set usable, 
the Process object will receive a message based on the mode of launch. If the 
application must set up any special notification (such as for the 
quick-transfer mechanism) or must restore special state file data, it should 
intercept this message. Typically the message received will be 
MSG_GEN_PROCESS_OPEN_APPLICATION-two others are received (when 
restoring from state and when opening in engine mode), but they should not 
be intercepted.

#### 6.1.3 Shutting Down an Application

MSG_GEN_PROCESS_CLOSE_APPLICATION, 
MSG_GEN_PROCESS_CLOSE_ENGINE, 
MSG_GEN_PROCESS_CLOSE_CUSTOM, MSG_META_QUIT

Just as loading an application is handled almost entirely by the system and 
GEOS classes, application shutdown is also fairly automatic. If the 
application intercepted MSG_GEN_PROCESS_OPEN_APPLICATION for its 
own purposes on startup, it likely has to do a little cleanup; otherwise, it 
won't have to worry about shutting down. (See ["Saving and Restoring State"](#614-saving-and-restoring-state) 
for special information on using this message.)

Any object in the system may cause an application to shut down. Usually, 
shutdown occurs either when the system is being exited (when a user exits to 
DOS, for example) or when the user has closed the application. Therefore, the 
usual source of the shutdown directive is the User Interface.

An application begins shutting down when either its Process object or its 
Application object receives a MSG_META_DETACH. If you want to cause a 
shutdown manually, you should send MSG_META_QUIT to the application's 
GenApplication object; this will execute some default functions and then 
send the appropriate MSG_META_DETACH. Essentially, the same detach and 
destruction mechanisms used for any object are used for the entire 
application. The object receiving MSG_META_DETACH passes the message 
along to all of its children and to all the objects on its active list. (If a 
MSG_META_DETACH is used without MSG_META_QUIT, the application will 
create a state file.)

When they have all acknowledged the detach, the application acknowledges 
the detach and sets itself unusable. It automatically flushes its message 
queues before shutting down to avoid synchronization problems. You should 
not subclass the MSG_META_DETACH handler unless you have special needs 
for cleaning up or sending special detach messages to other objects or geodes. 
If you do subclass it, you must call the superclass at the end of your handler. 
Otherwise, the application will not finish detaching (see [section 5.4.6.5 of 
chapter 5](ccoding.md#5465-detaching-and-destroying-objects)).

Instead of intercepting MSG_META_DETACH, though, the application may 
intercept the mode-specific message it will also receive. Depending on the 
mode in which it was launched, the application will receive (via the Process 
object) either MSG_GEN_PROCESS_CLOSE_APPLICATION (for application 
mode) or MSG_GEN_PROCESS_CLOSE_ENGINE (for engine mode). There is 
no special shutdown message for shutting down to a state file; instead, 
MSG_GEN_PROCESS_CLOSE_APPLICATION is used.

When the system shuts down or task-switches, a different type of shutdown 
occurs. Applications (or other objects interested in this event) must register 
for notification on the notification list GCNSLT_SHUTDOWN_CONTROL 
(notification lists are described in ["General Change Notification," Chapter 9](cgcn.md)). 
When the system shuts down or task-switches, the object will then receive a 
MSG_META_CONFIRM_SHUTDOWN, at which time the object must call **SysShutdown()**.

#### 6.1.4 Saving and Restoring State

ObjMarkDirty(), ObjSaveBlock()

Nearly all applications will save and restore their state so the user may shut 
down and return to precisely the same configuration he or she left. Saving of 
state is almost entirely contained within the system software; for the most 
part, only UI objects are saved to state files. You can, however, mark other 
object blocks and data for saving to a state file.

The state file for an application is a VM file containing object blocks. Only 
object blocks may be saved to the state file, though you can save LMem data 
by setting up object blocks with only data chunks in them. (Create the blocks 
with **MemAllocLMem()**, passing type LMEM_TYPE_OBJ_BLOCK, then 
simply use **LMemAlloc()** to allocate data chunks.) You can also save an extra 
data block to the state file using MSG_GEN_PROCESS_CLOSE_APPLICATION 
and MSG_GEN_PROCESS_OPEN_APPLICATION. In the close message, you 
can return the handle of an extra block to be saved to the state file; in the 
open message, the handle of the extra block is given to you, and you can 
restore this data as necessary. See the reference information for these 
messages under **GenProcessClass** for more information.

When a state file is saved, the system recognizes and saves only the dirty 
(modified) objects and chunks. Later, when state is restored, the system 
merges the changes in the state file with the original object blocks, resulting 
in the state that was saved.

For individual objects or entire object blocks to be saved to the state file, they 
must be marked dirty. Generic objects automatically mark themselves dirty 
at the appropriate times, so you don't have to worry about them. To mark 
other objects dirty, use the routine **ObjMarkDirty()**. Each object which has 
been marked dirty will be saved to a state file when appropriate. If you want 
to save an entire object block to the state file, you can call **ObjSaveBlock()** 
on the block; the system will save the entire block, not just the dirty chunks.

State files are dealt with at only two times: First, when the system starts up, 
it will check for the existence of application state files. If a state file exists, 
the system will attempt to load the application belonging to it; after loading 
the application's resources, it will merge the state changes with the default 
settings to restore the original state.

The second time state files are used is when the system shuts down. A simple 
shutdown (called a "detach") is invoked only by the UI and is not abortable. 
When a detach occurs, the system shuts down all geodes as cleanly and 
quietly as possible, saving them to state files. Only certain geodes will 
respond in extreme cases, offering the user the option of delaying the detach 
or cancelling an operation in progress. An example of this is the GEOS 
spooler; if one or more jobs are actively printing or queued for printing, the 
spooler will ask the user whether the job should continue and the detach be 
delayed, or whether the job should be aborted or delayed until the next 
startup. The spooler can not abort the detach in any case.

Another type of detach is called a "quit." Any geode may invoke a quit, which 
is actually a two-step detach. A quit will first notify all other geodes that the 
system will soon be detaching; other geodes then have the chance to abort the 
quit if they want. For example, if a terminal program were downloading a file 
and received a quit notification, it could ask the user whether she wanted to 
abort the quit or the download. If the user wanted to finish the download, she 
would abort the quit; if she wanted to quit, she would abort the download. 
The system would then either shut down via a normal detach or stop the quit 
sequence.

When a geode is first launched, no state file exists for it. The state file is not 
created until the geode is actually detached to a state file. If a geode is 
restored from a state file, the file will exist until the geode is detached again. 
A geode that gets closed (not detached to state) will remove any state file it 
may have created during a previous detach. A geode that is detached to state 
will create or modify its state file as appropriate.

The state of an application (how it was launched) is reflected in the 
GAI_states field of the GenApplication object. To retrieve the application's 
state, send it MSG_GEN_APPLICATION_GET_STATE. It will return a value of 
**ApplicationStates**. The most frequent use of this message is by 
applications that need to know whether a "quit" is underway when their 
receive the MSG_GEN_PROCESS_CLOSE_APPLICATION message; the process 
object will query the GenApplication and see if it is in the AS_QUITTING 
state.

In addition to the above state-saving functionality, the kernel provides two 
routines that translate handles between the state file and memory. 
**ObjMapSavedToState()** takes the memory handle of an object block and 
returns its corresponding state file VM block handle. 
**ObjMapStateToSaved()** takes the state file VM block handle and returns 
the corresponding memory block handle, if any.

If your application's documents are VM files, it is a very simple matter to save 
document state. In fact, if you use the GenDocument and document control 
objects, they will take care of document state saving for you. Be sure that the 
VM file has the VMA_BACKUP flag set in its **VMAttributes**; then you can 
simply call **VMUpdate()** on the document file. (Note-do not use **VMSave()** 
instead; it will erase the backup and lock in the user's changes to the 
document.) If you are not using GEOS VM files, it is up to you how and if you 
will save the document's state.

#### 6.1.5 Using Other Geodes

Often, geodes will have to use other geodes. For example, a communications 
program will use the Serial Driver, and a draw application will use the 
Graphic Object Library. Normally, this is taken care of by the compiler and 
the linker when you include a library or driver in your .goc and .gp files.

Other times, however, an application will have to load libraries or drivers on 
the fly and then free them some time later. This section describes how to load, 
use, and free libraries and drivers.

##### 6.1.5.1 Using Libraries

GeodeUseLibrary(), GeodeFreeLibrary()

Libraries are always referenced by their file names or by their geode handles. 
It's easiest, however, to use the file name of the library when loading it-the 
system will locate the library for you. It's unusual to need to load a library for 
use with your geode; in almost all cases it's easiest to include the library in 
your .goc and .gp files and have the system load and link the library 
automatically. (To do this, include the library's interface definition file in your 
code file and list the library's geode name in your geode parameters file.)

If you need to load a library dynamically, though, use **GeodeUseLibrary()**. 
This routine takes the protocol numbers expected of the library (see [section 
6.1.8.2](#6182-protocol-numbers)) and the library geode's filename. It will locate and load 
the library if not already loaded. If the library is already loaded, it will 
increment the library's reference count. When you are done using a library 
loaded with **GeodeUseLibrary()**, you must free the library's instance with 
**GeodeFreeLibrary()**.

##### 6.1.5.2 Using Drivers

GeodeUseDriver(), GeodeInfoDriver(), 
GeodeGetDefaultDriver(), GeodeSetDefaultDriver(), GeodeFreeDriver()

Drivers are referenced by either their permanent names or their geode 
handles. Most drivers used by applications will be loaded automatically by 
the kernel; the application must have the driver's permanent name specified 
in its .gp file. Should an application need to use a driver not included in its 
parameters file, however, it can do so with the routines described below.

When you need to use a driver, the **GeodeUseDriver()** routine will locate 
and load it, adding it to the active geodes list. You must pass the desired 
driver geode's filename as well as the expected protocol levels of the driver. 
The routine will return the driver's geode handle. If you load a driver 
dynamically, you must free it with **GeodeFreeDriver()** when your geode 
shuts down or otherwise finishes using the driver.

If you know a driver's geode handle, you can easily retrieve information 
about it with the routine **GeodeInfoDriver()**. This returns a structure of 
type **DriverInfoStruct**, which contains the driver's type (**DriverType**), the 
driver's attributes, and a far pointer to the driver's strategy routine. Many 
driver types have an expanded information structure, of which 
DriverInfoStruct is just the first field. Video driver information structures, 
for example, also contain dimensions and color capabilities (among other 
things) of the particular devices they drive. The driver information structure 
is shown below.

~~~
typedef struct {
    void         (*DIS_strategy)();
    DriverAttrs  DIS_driverAttributes;
    DriverType   DIS_driverType;
} DriverInfoStruct;
~~~

The DIS_strategy field of the structure contains a pointer to the driver's 
strategy routine in fixed memory. After the driver has been loaded, its 
strategy routine is called directly with a driver function name.

The DIS_driverAttributes is an attribute record of type **DriverAttrs**, the 
flags of which are shown below:

DA_FILE_SYSTEM  
This flag indicates that the driver is used for file access.

DA_CHARACTER  
This flag indicates that the driver is used for a 
character-oriented device.

DA_HAS_EXTENDED_INFO  
This flag indicates that the driver has a **DriverExtendedInfo** structure.

The DIS_driverType contains the type of driver described by the information 
structure. The types that may be specified in this field are listed below:

DRIVER_TYPE_VIDEO  
This is used for video drivers.

DRIVER_TYPE_INPUT  
This is used for input (mouse, keyboard, pen, etc.) drivers.

DRIVER_TYPE_MASS_STORAGE  
This is used for storage device drivers.

DRIVER_TYPE_STREAM  
This is used for stream and port (parallel, serial) drivers.

DRIVER_TYPE_FONT  
This is used for font rasterizing drivers.

DRIVER_TYPE_OUTPUT  
This is used for output drivers other than video and printer 
drivers.

DRIVER_TYPE_LOCALIZATION  
This is used for drivers that facilitate internationalization.

DRIVER_TYPE_FILE_SYSTEM  
This is used for file system drivers.

DRIVER_TYPE_PRINTER  
This is used for printer drivers.

DRIVER_TYPE_SWAP  
This is used for the system's memory-swapping drivers.

DRIVER_TYPE_POWER_MANAGEMENT  
This is used for devices that have power management systems.

DRIVER_TYPE_TASK_SWITCH  
This is used for devices or systems that have task switchers.

DRIVER_TYPE_NETWORK  
This is used for special networks that require driver 
functionality.

When you want a driver to perform one of its functions, you must call its 
strategy routine. The strategy routine typically takes a number of 
parameters, one of which is the function the driver should perform. The 
**DriverInfoStruct** contains a far pointer to the strategy routine; your 
application should store this far pointer and call it directly any time one of 
the driver's functions is needed. However, because the driver may be put in 
a different location each time it's loaded, you should not save the pointer in a 
state file. Note that this scheme of accessing drivers directly can only be 
implemented in assembly language. Some drivers may provide library 
interfaces as well as their standard driver interface; this allows routines to 
be written in C. 

GEOS maintains default drivers for the entire system. The types of default 
drivers are described by **GeodeDefaultDriverType**; all the types are shown 
below. They are called default drivers because the default for each category 
of driver used by the system is stored in the GEOS.INI file. GEOS will, upon 
startup, load in the default driver of each category.

GDDT_FILE_SYSTEM  
GEOS may use several file system drivers during execution. A 
file system driver allows GEOS to work on a given DOS (or 
substitute file access system). The primary driver is considered 
the default driver.

GDDT_KEYBOARD  
The system may only use one keyboard driver during 
execution; typically, keyboards can differ from country to 
country. Not all systems will have a keyboard.

GDDT_MOUSE  
The mouse should be usable when the system comes up on the 
screen; the user does not have to manually load a mouse driver 
with each execution of GEOS. Not all systems will have a 
mouse.

GDDT_VIDEO  
GEOS must know what type of video driver to use prior to 
attempting to display itself.

GDDT_MEMORY_VIDEO  
The Vidmem driver is a video driver that draws to memory (i.e. 
to bitmaps). It is used primarily for printing but also for editing 
bitmaps.

GDDT_POWER_MANAGEMENT  
A few machines use hardware power management systems 
(most notably some palmtops and some notebook and laptop 
machines). In order for GEOS to handle the power management 
hardware properly, it must load the driver on startup. (Most 
machines will not use this type of driver.)

GDDT_TASK  
Some systems will use a task switcher; in order for GEOS to be 
a task in one of these, it must have a default task driver.

To retrieve the default that GEOS is using, call the routine 
**GeodeGetDefaultDriver()** with the appropriate driver type (a member of 
the type **GeodeDefaultDriverType**). This routine will return the geode 
handle of the default driver of that type. To set a new default driver for a 
specified driver type, use **GeodeSetDefaultDriver()**. This routine takes a 
geode handle and a driver type and sets the system default for that type. 
Typically, system defaults will be set only by the Preferences Manager 
application.

#### 6.1.6 Writing Your Own Libraries

Creating a library geode is a simple step beyond creating a normal 
application. Libraries can have their own process threads or not; most 
libraries do not, though some will. To create a library geode, you have to do 
four things:

+ Declare the geode to be a library.  
In the geode parameters (.gp) file, you must declare the type of the geode 
to be library.

+ Define an entry point routine.  
Create a routine that initializes the library; this routine is called the 
"entry point routine" and is called each time the library is loaded.

+ Export the entry point routine.  
Add another line in the geode parameters (.gp) file to export the library's 
entry point routine.

+ Set the geode's permanent name properly.  
Most libraries have the extension of their permanent name be "lib." This 
helps identify it as a library when using Swat. The permanent name is 
defined in the geode parameters (.gp) file.

Code Display 6-1 shows an example of the three changes you must make to 
the geode's parameters file to make it a library.

---
Code Display 6-1 Defining a Library-the sound.gp File
~~~
# The Sound Library has the following three lines in its geode parameters file
# (sound.gp) different from how they might appear in an application.

#
# The permanent name of the geode might normally be sound.app if it is an
# application. A library typically has "lib" as its name extension.
#
name sound.lib

#
# Declare the type of the geode to be a library. Applications typically have a
# "process" aspect; many libraries do not. Libraries must also be declared to
# have a "library" aspect. (Note that the Sound Library is exceptional in that
# it has both a driver aspect and a library aspect.)
#
type driver, library, single

#
# Export the geode's entry point routine so the kernel may call it when the
# library is loaded. This routine must be defined somewhere in the geode's code.
# Note that it is exported with the "entry" line rather than the standard
# "export" line; this is to distinguish the exported routine as the entry
# point rather than a typical routine.
#
entry SoundEntry
~~~

#### 6.1.7 Working with Geodes

The system provides a number of utility routines for getting information and 
setting attributes of geodes. These are loosely organized throughout the 
following sections.

#### 6.1.7.1 Accessing the Application Object

GeodeGetAppObject()

GEOS offers a routine for retrieving the optr of an application's 
GenApplication object. **GeodeGetAppObject()** takes the process handle of 
the Process object of the application. It returns the optr of the application's 
GenApplication object.

#### 6.1.7.2 General Geode Information

GeodeFind(), GeodeFindResource(), GeodeGetInfo(), 
GeodeGetProcessHandle(), GeodeGetCodeProcessHandle(), 
ProcInfo()

**GeodeFind()** returns a geode's handle when given a permanent name and 
attributes to search on. GEOS will search the active geode list for any geode 
with the given name and the proper attributes set or clear.

**GeodeFindResource()** locates a given resource within a geode's file. It 
must be passed the file handle of the geode, the number of the resource in the 
file, and an offset within the resource at which the file read/write position 
should be placed. This routine may only be used on open geode files, and it 
returns the base offset and size of the resource. You will probably not need to 
use this routine.

**GeodeGetInfo()** takes a geode handle, a **GeodeGetInfoType** parameter, 
and a buffer appropriate for the return information. It returns the 
appropriate values as specified in the **GeodeGetInfoType** parameter. (This 
parameter specifies what type of information is sought about the geode; the 
routine can return the geode's attributes, geode type, release level, protocol, 
token ID, or permanent name.) The possible values of **GeodeGetInfoType** 
are shown below:

GGIT_ATTRIBUTES  
This indicates the geode's attributes should be returned.

GGIT_TYPE  
This indicates the geode's type should be returned (type **GeodeType**).

GGIT_GEODE_RELEASE  
This indicates the geode's release numbers should be returned.

GGIT_GEODE_PROTOCOL  
This indicates the geode's protocol numbers should be 
returned.

GGIT_TOKEN_ID  
This indicates the geode's token information should be 
returned.

GGIT_PERM_NAME_AND_EXT  
This indicates the geode's permanent name and extender 
should be returned.

GGIT_PERM_NAME_ONLY  
This indicates the eight characters of the geode's permanent 
name only should be returned, without the extender 
characters.

**GeodeGetProcessHandle()** returns the geode handle of the current 
process (the owner of the current thread). Another routine, 
**GeodeGetCodeProcessHandle()**, returns the handle of the geode that 
owns the code block from which it was called.

**ProcInfo()** returns the thread handle of the first thread of a given process.

##### 6.1.7.3 Managing Geode Event Queues

GeodeAllocQueue(), GeodeFreeQueue(), GeodeInfoQueue(), 
GeodeFlushQueue(), ObjDispatchMessage(), QueueGetMessage(), 
QueuePostMessage(), GeodeDispatchFromQueue()

The following routines allocate and manage event queues. These routines are 
rarely called by applications as event queues are automatically managed for 
each thread and application.

**GeodeAllocQueue()** allocates an event queue and returns its handle. 
**GeodeInfoQueue()** returns the number of events in a given event queue. 
**GeodeFreeQueue()** frees an event queue allocated with the routine 
**GeodeAllocQueue()**. It must be passed the handle of the queue to be freed 
(unhandled events still in the queue will be discarded).

**GeodeFlushQueue()** flushes all events from one queue and synchronously 
places them all in another queue (events may not simply be tossed out).

**QueueGetMessage()** combined with **ObjDispatchMessage()** removes the 
first event from the given event queue and handles it via a callback routine. 
A far pointer to the callback routine in memory must be passed. Typically 
these will be used only by the assembly **ObjMessage()** routine used by the 
kernel; some other applications of this routine may be used, though. For 
example, the sound driver uses a note queue unassociated with objects and 
messages. The callback routine therefore gets the "event" (note) and pretends 
it's handling a message.

**QueuePostMessage()** adds an event to the specified queue. 

#### 6.1.8 Geode Protocols and Release Levels

Every GEOS geode and VM file has both a release level and a protocol level as 
extended attributes of the file. These two items help ease the transitions for 
both programmer and user when changes are made to applications, libraries, 
drivers, system software, etc. To control release and protocol numbers, use 
the GREV tool and a REV file as described in "Grev" on page 444 of "Using 
Tools," Chapter 10 of the Tools Reference Manual.

##### 6.1.8.1 Release Numbers

The release number is a **ReleaseNumber** structure which consists of four 
components: The RN_major and RN_minor numbers are the most significant. 
The RN_change and RN_engineering numbers are less significant and are 
used primarily to indicate non-released or running upgrade types of changes 
to the geode. The **ReleaseNumber** of a geode or VM file is stored in the file's 
FEA_RELEASE extended attribute, and its structure is shown below:

~~~
typedef struct {
    word    RN_major;
    word    RN_minor;
    word    RN_change;
    word    RN_engineering;
} ReleaseNumber;
~~~

The contents of the release number are up to the particular geode and are 
product-specific. Release numbers are not used by GEOS for compatibility 
checking or any other validation of files, though they are used during 
installation procedures.

To retrieve the release number of a given geode, use the routine 
**GeodeGetInfo()**. Release levels should be set at compile time and are not 
changeable at run-time.

##### 6.1.8.2 Protocol Numbers

The protocol number is a structure of type **ProtocolNumber** stored in the 
file's FEA_PROTOCOL extended attribute. Each GEOS geode and data file has 
a protocol level associated with it. The protocol level is used for compatibility 
checking for both geodes and documents.

The **ProtocolNumber** structure consists of two parts, the major protocol 
and the minor protocol. This structure is shown below:

~~~
typedef struct {
    word    PN_major;
    word    PN_minor;
} ProtocolNumber;
~~~

Differences in protocol levels indicate incompatibilities between two geodes, 
between a geode and its document format, or between a geode and its state 
file format. If the major protocols are different, the two items are not 
compatible at all (unless special provisions are made). If the minor protocol 
is greater than expected, some incompatibility may exist but should not 
affect the program. You should increment a geode's or document's protocol 
whenever a change is made.

If a change to a library is upward-compatible, only the minor protocol needs 
to be incremented. For example, if a library acquires a new function but the 
library's entry points are undisturbed, the minor protocol should be 
incremented and the major protocol left as is. If the new function causes 
relocation of the entry point numbers, however, the major protocol must also 
be incremented.

An application's protocol must be increased whenever a change will affect the 
application's state files. If you make a change to an application, for example, 
and the user has old state files, either the changes in the application can be 
replaced with the old information, or the state file will cause an 
incompatibility with unpredictable results. If the change to the application is 
simply functional, increment the minor protocol. If the change is to any part 
of a UI resource or to any other item saved to a state file, increment the major 
protocol. State files will be loaded if minor protocols are different and will be 
ignored if major protocols are different.

If an application's document format changes, you should make sure the new 
documents are not loaded by old applications or vice versa (unless you take 
the necessary conversion steps). When opening a document, you can check its 
protocol by checking the document file's extended attribute FEA_PROTOCOL. 
If the protocol level needs to be changed (after conversions have been done, 
of course), you can change them by setting FEA_PROTOCOL. (See ["File 
System", Chapter 17](cfile.md).)

A few examples of when minor and major protocols should be incremented 
follow. Keep in mind that this list is by no means exhaustive.

+ If you add a data structure to a document format and older versions of 
the application will still be able to open the document, increment the 
document's minor protocol only.

+ If you change the instance data structure of an object that gets saved to 
the document, increment the major protocol, as the new methods will be 
using the wrong offsets to access data in the old object.

+ If you add a new class to your application and don't disturb any of the 
other classes' entry points (i.e. add the class at the end), increment the 
minor protocol only.

+ If you add a new class, disturbing entry points, increment the major 
protocol.

+ If you add or delete a resource, increment the major protocol.

+ If you add a chunk or object to a resource that gets saved to state files, 
increment the major protocol.

+ If you change an object's flags (e.g. mark it ignore-dirty), increment the 
major protocol. Otherwise, the flags will be restored from the state file 
and will override the changes you made.

#### 6.1.9 Temporary Geode Memory

GeodePrivAlloc(), GeodePrivFree(), GeodePrivWrite(), 
GeodePrivRead()

Every geode in the system has a "private data" area, a space set aside in its 
core block. This private data is used primarily by library geodes, when each 
of the library's clients uses its own copy of a particular data structure that 
gets manipulated by the library. The private data mechanism is used in the 
GEOS implementation of **malloc()**, for example (though you need not know 
this to use **malloc()**).

Private memory may be allocated, written to, read from, and freed by the 
library. The library does not have to allocate a block for each geode and 
maintain its own handle table; the use of the **GeodePriv-()** routines 
automatically manages this.

**GeodePrivAlloc()** reserves a given number of contiguous words for the 
library within the private data of all geodes in the system. The memory space 
is reserved but is not actually allocated for a given geode until it is used 
(written to); this is done for optimization purposes. This routine will return 
a tag pointing to where the reserved words begin. This tag is used when 
reading, writing, or freeing the private data. If the memory could not be 
allocated, the routine will return zero.

**GeodePrivWrite()** and **GeodePrivRead()** write to and read from the 
private data space. They take similar parameters: a geode handle, the tag as 
returned by **GeodePrivAlloc()**, the total number of words to be written or 
read, and a pointer to a locked or fixed buffer. In **GeodePrivWrite()**, the 
buffer will be passed containing the words to be written; in 
**GeodePrivRead()**, the buffer will be passed empty and returned containing 
the words read.

Typically, the geode handle passed will be zero; this indicates that the current 
process (which will be the library's current client) will be the owner of the 
private data affected. Because the library code will be executing in the thread 
of a given application, the application geode will be the only one having its 
private data affected. Thus, a library can use the same code to store different 
data for each geode that uses it; neither the library nor the geode needs to 
know that other geodes are also using the same routines.

**GeodePrivFree()** frees a given number of words from all geodes' private 
data. It needs to be passed only the number of words to be freed and the tag 
as returned by **GeodePrivAlloc()**.

### 6.2 Creating Icons

Every geode can have an icon associated with it. Typically, only applications 
will have special icons; other geodes (libraries and drivers) generally use one 
of the system icons or the icon of the application they're primarily used by.

An application's icon is stored in two places: It is defined and stored within 
the application's extended attributes in its.GEO file. It is also stored in a 
database file maintained by the UI, called the token database.

The GEOS development kit includes an icon editor tool so you can easily 
create icons of various color and resolution characteristics and install them 
into your applications.

#### 6.2.1 The Token Database

TokenOpenLocalTokenDB(), TokenCloseLocalTokenDB()

The token database is stored in its own file. Each entry represents a single 
icon or series of icons that can be used with any number of files. The token 
database file is managed by the UI and the kernel; you should have no reason 
to access its internals directly, and most applications will never need to use 
any of the token routines except to install their document icons.

Some systems may have shared token database files; this is controlled by the 
INI file key sharedTokenDatabase in the paths category. Most often a shared 
database file exists on a network drive and may be supplemented with a local 
token database. By default, if a shared database file exists, it will be opened 
read-only in addition to the read/write local file. You can open and close only 
the local database file, however, with the routines 
**TokenOpenLocalTokenDB()** and **TokenCloseLocalTokenDB()**.

Every GEOS file has a token. The token is an index into the token database. 
When GeoManager scans a directory, it grabs the token from each file and 
searches through the token database file for it. If a match is found, 
GeoManager selects the proper icon and displays it; if no match is found or if 
the file's token is invalid, GeoManager will launch the application in engine 
mode and request that it install its token in the database.

For non-GEOS files, GeoManager uses the three extension characters of the 
file's name as a pseudo-index. For each extension (e.g. .COM, .EXE, .DOC, 
.BAT, etc.), GeoManager uses a single icon. Which icon is used can be set in 
the GEOS.INI file if a user wishes, but GeoManager will normally select the 
default DOS icon (of which there are two: one for executables and one for 
non-executables).

The index into the token database consists of two parts and is of type 
**GeodeToken**. This structure contains four text characters as well as the 
manufacturer ID number of the geode's manufacturer. The structure's 
definition is shown below:

~~~
typedef struct {
    TokenChars      GT_chars;
    ManufacturerID  GT_manufID;
} GeodeToken;

typedef char TokenChars[TOKEN_CHARS_LENGTH];
~~~

This structure is created and filled automatically in the geode's .geo file 
header by the Glue linker, which takes the values from the geode's geode 
parameters (.gp) file. The two fields used from the .gp file are tokenchars for 
the four characters and tokenid for the manufacturer ID.

A **GeodeToken** structure in the token database file can also be filled in when 
GeoManager scans a directory. If the header of a particular application's .geo 
file does not have a recognized token, GeoManager will launch the 
application in "engine" mode, loading its GenApplication object. It will 
request that the application object then install its icon into the token 
database file (**GenApplicationClass** knows how to do this).

The token database file contains one entry for each token that has been 
installed. Each time a new token and icon is encountered, a new entry is 
added by the UI. This happens automatically when GeoManager scans a directory.

### 6.2.2 Managing the Token Database File

TokenDefineToken(), TokenGetTokenInfo(), 
TokenLookupMoniker(), TokenLoadMonikerBlock(), 
TokenLoadMonikerChunk(), TokenLoadMonikerBuffer(), 
TokenRemoveToken(), TokenLoadTokenBlock(), 
TokenLoadTokenChunk(), TokenLoadTokenBuffer(), 
TokenLockTokenMoniker(), TokenUnlockTokenMoniker(), 
TokenGetTokenStats(), TokenListTokens()

In nearly all cases, you will create your application's icon with the icon tool 
and not worry about it again. However, the following routines allow you to 
add, change, access, and remove entries from the token database.

**TokenLoadTokenBlock()**, **TokenLoadTokenChunk()**, and 
**TokenLoadTokenBuffer()** load a **TokenEntry** structure into memory (a 
newly allocated block, a newly allocated chunk, or a locked buffer). The 
TokenEntry structure contains information about the token, the geode's 
release number, and the geode's protocol number. This structure does not 
actually contain the monikers used for the icon.

**TokenLookupMoniker()** gets the specific moniker of a token entry given 
the display type (CGA, EGA, VGA, etc.), the entry's **GeodeToken** structure, 
and search flags. If the moniker is found, the entry identifier (database group 
and ID numbers) of the moniker are returned. You can use these return 
values to lock the moniker into memory (see below).

**TokenLockTokenMoniker()** locks a moniker into memory given its entry 
identifier. This routine returns a pointer to a locked block and the chunk 
handle of the chunk containing the locked moniker. A moniker should always 
be locked before it is drawn; this keeps it from moving in memory while it is 
being accessed. The routine TokenUnlockTokenMoniker() unlocks a 
previously locked moniker when given the moniker's segment address. This 
unlocks the entire block, not just the individual moniker.

**TokenLoadMonikerBlock()**, **TokenLoadMonikerChunk()**, and 
**TokenLoadMonikerBuffer()** load a specific moniker into memory from the 
token database file (into a newly allocated block, a newly allocated chunk, or 
a locked buffer). It takes the same parameters as **TokenLookupMoniker()** 
but returns the handle and chunk handle of the loaded moniker. If using this 
routine, simply lock the memory block rather than using 
**TokenLockTokenMoniker()**.

**TokenGetTokenInfo()** finds a token when passed its tokenchars and 
tokenid and returns the token's flags. If no token exists with the passed 
characteristics, it will return an error flag.

**TokenDefineToken()** adds a new token and its moniker list to the token 
database. If the given token already exists, the new one will replace the old 
one. The token identifier (tokenchars, tokenid), handle and chunk handle of 
the moniker list, and flags of the new token must be passed.

**TokenRemoveToken()** removes a given token and its moniker list from the 
token database file. It returns only a flag indicating whether the token was 
successfully removed or not.

**TokenListTokens()** returns a list of the tokens in the token database. It is 
passed three arguments:

+ A set of TokenRangeFlags, which specifies which tokens should be 
returned. The following flags are available:

TRF_ONLY_GSTRING  
Return only those tokens which are defined with a GString.

TRF_ONLY_PASSED_MANUFID  
Return only those tokens which match the passed 
manufacturer's ID.

+ A number of bytes to reserve for a header.

+ A manufacturer ID. This field is ignored if TRF_ONLY_PASSED_MANUFID 
was not passed.

**TokenListTokens()** allocates a global memory block and copies all the 
specified tokens into that block. It leaves a blank space at the beginning of 
the block; this space is the size specified by the second argument. The rest of 
the block is an array of **GeodeToken** structures. **TokenListTokens()** 
returns a dword. The lower word of the return value is the handle of the 
global memory block; the upper word is the number of **GeodeToken** 
structures in that block.

### 6.3 Saving User Options

Almost all users enjoy configuring their systems to their own tastes, whether 
it's setting the background bitmap or choosing a default font. Most 
applications will, therefore, want to provide a way for the user to choose and 
save various options for the application.

Generic objects have this capability built in. If you set them up for saving 
options, they will automatically set and maintain the saved options, making 
sure the options are set properly whenever the application is launched. 
However, sometimes you will want to set additional options not managed by 
UI objects. This is not difficult to do in GEOS: You can have your application 
save its options directly to the local initialization file, GEOS.INI.

#### 6.3.1 Saving Generic Object Options

All appropriate generic UI objects have the ability to save their options. For 
example, a properties GenInteraction could save which of its options are on 
and which are off when the user presses a "save options" trigger.

To save generic object options, you have to do two basic things:

+ Create an Options menu  
Typically, you will create a menu for user options. This menu should be 
declared of type GIGT_OPTIONS_MENU as shown in Code Display 6-2.

+ Use a special GCN list  
All objects that want to save options must be put on a special GCN list in 
the application's GenApplication object. The list type should be either 
GAGCNLT_STARTUP_LOAD_OPTIONS (if the options should be loaded at 
the time of application startup) or GAGCNLT_SELF_LOAD_OPTIONS (if 
the options should be loaded when the object deems it necessary, typically 
when the object is first displayed).

Code Display 6-2 shows an example of how objects should be declared for 
saving their options.

---
Code Display 6-2 Saving Generic Object Options
~~~
/* The GenApplication object must declare a GCN list of the type appropriate for
 * the options being saved (or both types if the application uses both types).
 * This GenApplication declares a list of objects whose options do not have to be
 * loaded at startup. */

@object GenApplicationClass SampleApp = {
    GI_visMoniker = "Sample Application";
    GI_comp = SamplePrimary;                /* Primary window object is the only child. */
    gcnList(MANUFACTURER_ID_GEOWORKS, GAGCNLT_WINDOWS) = SamplePrimary;
        /* The above list is to declare windowed objects that must appear
         * when the application is opened and made usable. */
    gcnList(MANUFACTURER_ID_GEOWORKS, GAGCNLT_SELF_LOAD_OPTIONS) =
                    SampleController;
        /* The above list is used for generic objects that save their
         * options but do not need their options loaded at startup. */
}

/* Some applications that have generic objects save their own options might not
 * have a special Options menu but may just have a trigger somewhere for saving
 * options. In any case, the "save options" trigger sends MSG_META_SAVE_OPTIONS to
 * its GenApplication object. If you use an Options menu with GIGT_OPTIONS_MENU
 * set, this will automatically be built into the menu.
 * The SampleOptionsMenu is a child of SamplePrimary, not shown. */

@object GenInteractionClass SampleOptionsMenu = {
    GI_comp = SampleToolbox, SampleToolControl, SampleSaveOptsTrigger;
    GII_visibility = GIV_POPUP;                 /* Make it a menu. */
    ATTR_GEN_INTERACTION_GROUP_TYPE = (GIGT_OPTIONS_MENU);
}

/* The other objects (controllers) are not shown here. Just the "save options"
 * trigger, which sends MSG_META_SAVE_OPTIONS to the GenApplication object. */

@object GenTriggerClass SampleSaveOptsTrigger = {
    GI_visMoniker = `S', "Save Options";
    GTI_destination = SampleApp;
    GTI_actionMsg = MSG_META_SAVE_OPTIONS;
}
~~~

#### 6.3.2 The GEOS.INI File

GEOS can use multiple initialization files in network situations, but only the 
local GEOS.INI is editable. The local GEOS.INI (referred to hereafter simply 
as GEOS.INI or "the INI file") is read first into a moveable, swappable buffer, 
and it contains the path names of any other INI files to be used. The other INI 
files are subsequently loaded into other buffers. When the kernel searches for 
a specific entry in one of the INI files, it looks first in the local INI's buffer and 
then in any others in the order they were loaded. When it reaches what it 
wants, it stops searching. Thus, multiple entries are allowed but are not 
used, and the local INI file has precedence over all others.

##### 6.3.2.1 Configuration of the INI File

The GEOS.INI file has a very specific format and syntax. It is segmented into 
categories, each of which contains several keys that determine how GEOS will 
perform. (For an example of an INI file, see Code Display 6-3.) Note that 
typically the GEOS.INI file should not be accessed directly because its format 
is subject to change.

A category is essentially a group of keys. Certain geodes will work with their 
own categories, and certain categories will be used by several geodes. For 
example, the system category is used by the kernel and the UI to determine 
several of the default system settings. Each category is set in GEOS.INI by 
putting its name within square brackets. The text within the brackets is 
case-insensitive and ignores white space, so [My category], [mycategory], and 
[MY CATEGORY] are all equivalent.

A key is any setting that the system or an application can recognize and 
assign a value to. A single key may exist in several different categories; since 
both category and key define an entry, the entries will be unique if either key 
or category is different. Keys may be text strings, integers, or Boolean values, 
and every key is identified by its name, which is an ASCII string. Data may 
also be read from and written to the file in binary form; the kernel will 
automatically convert this into ASCII hexadecimal for storage and will revert 
it to binary during retrieval.

Both category names and key fields are called "entries." Each entry may exist 
on a single line or may cover several lines. An entry that contains carriage 
returns is technically known as a "blob." Each blob will automatically be 
enclosed in curly braces when it is written into the file. Any blob that 
contains curly braces will automatically have backslashes inserted before the 
closing braces so GEOS doesn't mistake them for the blob delimiters.

Comments may be added to the INI file by putting a semicolon at the 
beginning of the line. The file has several standard categories and keys that 
can be set within them. These are detailed in ["The INI File," Chapter 9 of the 
Tools Reference Manual](../Tools/tini.md).

---
Code Display 6-3 Example GEOS.INI File Entries
~~~
; The category "system" is used by the kernel and the UI to set certain system
; defaults such as the number of handles, the default system font and size, and
; the types of memory drivers to be loaded.

[system]

; The handles key is assigned an integer that determines the number of handle
; spaces allocated for the system's Handle Table.
handles = 2500

; The font key is assigned a character string that represents a file name
; or a list of file names separated by spaces.
; The listed file(s) will be read in as the font driver geode.
font = nimbus.geo

; The memory key is assigned a blob of text containing all the names of the memory
; drivers available to the system.
memory = {
disk.geo
emm.geo
xms.geo
}

; The category "My App's Category" is set for example only. It is used by the MyApp
; application.

[My App's Category]

; The myappHiScore key is an integer key set by the MyApp application.
myappHiScore = 52

; The myappBoolean key is a Boolean value. Booleans are case-insensitive, so True,
; true, and TRUE are all equated to "true". This is actually a Boolean value and
; is translated by the read and write routines that work with Boolean values.
myappBoolean = true

; The myappHiName is a text string that, in this case, contains carriage returns,
; backslash characters, and curly brace characters. The original text looked like
; this:
;       this is a multi-line
;       blob of text with curly
;       brace ({,}) characters }} in it
; It is automatically given backslashes in front of the closing braces, and it
; is automatically surrounded with curly braces.
myappHiName = {this is a multi-line
blob of text with curly
brace ({,\}) characters \}\} in it}
~~~

##### 6.3.2.2 Managing the INI File

InitFileSave(), InitFileRevert(), 
InitFileGetTimeLastModified(), InitFileCommit()

Because the INI file is common to all geodes and to all threads in the system, 
only one thread at a time may access it. This synchronization is handled by 
the kernel whenever a routine to read from or write to the INI file is used. 
Additionally, the INI file is loaded into a buffer when the system is first run; 
all operations on the INI file actually work on this buffer, and the buffer may 
be flushed to disk only by the kernel.

There are, however, four routines that work directly on the INI file. One saves 
the file, another reverts it from the last save, the third checks the time the 
file was last modified, and the fourth commits any pending INI file changes 
to disk.

To save the local GEOS.INI, use the routine **InitFileSave()**; this saves the 
changes to the backup file. It requires no parameters and returns an error 
flag if the file could not be saved. **InitFileRevert()** reverts the GEOS.INI file 
to its state the last time it was backed up. This routine takes no parameters, 
and it returns an error flag if the revert can not be accomplished.

**InitFileGetTimeLastModified()** returns the system counter's value stored 
the last time GEOS.INI was modified.

**InitFileCommit()** takes all the changes made since the file was last 
modified and flushes them to disk. This commits all the changes and should 
be used only from the kernel. It should not be used by applications.

##### 6.3.2.3 Writing Data to the INI File

InitFileWriteData(), InitFileWriteString(), 
InitFileWriteStringSection(), InitFileWriteInteger(), 
InitFileWriteBoolean()

GEOS provides five routines to write to the INI file, one for each of the 
allowable data types. Each of these routines will first gain exclusive access to 
the local INI buffer, then locate the appropriate category for writing. After 
writing the key and its value, the routine will relinquish exclusive access to 
the buffer, allowing other threads to write into it. Writing a category or key 
that does not exist in the local INI file will add it to the file.

Each of these routines takes at least three arguments: The category is 
specified as a null-terminated character string; a pointer to the string is 
passed. The key name is also specified as a null-terminated character string; 
again, a pointer to the string is passed. The third parameter is specific to the 
routine and contains the value to which the key will be set.

**InitFileWriteData()** writes a number of bytes into the INI buffer, and it 
takes four parameters. The additional parameter is the size of the data. The 
data will be converted into ASCII hexadecimal when the file is saved and will 
be converted back when the key is read.

**InitFileWriteString()** takes a pointer to the null-terminated character 
string to be written. If the character string contains carriage returns or line 
feeds, it will automatically be converted into a blob.

**InitFileWriteStringSection()** writes a new string section (a portion of a 
blob) into the specified entry. The specified entry must be a blob already, and 
the string section will be appended to the blob. A string section is a line of a 
blob delineated by line feeds or carriage returns.

**InitFileWriteInteger()** takes as its third argument the integer to be 
written.

**InitFileWriteBoolean()** takes a Boolean value. A zero value represents 
false, and any nonzero value represents true. When looking at the INI file 
with a text editor, the Boolean value will appear as a text string of either 
"true" or "false"; it will, however, be interpreted as a Boolean rather than a 
text string.

##### 6.3.2.4 Getting Data from the INI File

InitFileReadDataBuffer(), InitFileReadDataBlock(), 
InitFileReadStringBuffer(), InitFileReadStringBlock(), 
InitFileEnumStringSection() 
InitFileReadStringSectionBuffer(), 
InitFileReadStringSectionBlock(), InitFileReadInteger(), 
InitFileReadBoolean()

When you want to check what a key is set to in the INI file, you should use 
one of the **InitFileRead-()** routines. These search the local INI file first and 
then each of the additional INI files in the order they were loaded. They will 
return the first occurrence of a given key, so if the key exists in both your local 
INI file and another INI file, these routines will return only the local value.

All of these routines take at least two parameters. The first is the category of 
the entry to be retrieved; this is stored as a null-terminated ASCII string, and 
a pointer to the string is passed. The second is the key of the entry. This, too, 
is stored as a null-terminated ASCII string, and a pointer to the string is 
passed.

**InitFileReadBoolean()** returns the Boolean value of the given key. If the 
key is set "false," a value of zero (FALSE) will be returned. If the key is set 
"true," a nonzero value will be returned (-1, the constant TRUE).

**InitFileReadInteger()** returns the integer value of the given key.

**InitFileReadDataBuffer()** and **InitFileReadDataBlock()** both return 
the data bytes stored in the given key. The first, however, takes the address 
of a buffer already allocated and puts the data into the buffer. The second 
allocates a new block on the heap and puts the data into it. If you don't know 
the size of the data, you should use **InitFileReadDataBlock()**.

**InitFileReadStringBuffer()** and **InitFileReadStringBlock()** both 
return the null-terminated string stored in the given key. In both cases, curly 
braces will be stripped off of blobs and backslash characters will be removed 
if appropriate. The first, however, takes the address of a buffer already 
allocated and puts the string in the buffer. The second allocates a new block 
on the heap and returns the string in it. If you don't know the approximate 
size of the string, use **InitFileReadStringBlock()**.

**InitFileReadStringSectionBuffer()** and its counterpart 
**InitFileReadStringSectionBlock()** both return a null-terminated section 
of the string stored in the given key. A string section is defined as any number 
of contiguous printable ASCII characters and is delimited by carriage returns 
or line feeds. These routines take the number of the string section desired 
and return the section (if it exists). **InitFileReadStringSectionBuffer()** 
takes the address of a buffer already allocated on the heap and returns the 
string section in the buffer. **InitFileReadStringSectionBlock()** allocates a 
new block on the heap and returns the string section in it. You should use this 
routine if you don't know the approximate size of the string section.

**InitFileEnumStringSection()** enumerates the specified blob, executing a 
specified callback routine on each string section within the blob.

##### 6.3.2.5 Deleting Items from the INI File

InitFileDeleteEntry(), InitFileDeleteCategory(), 
InitFileDeleteStringSection()

Besides reading and writing data, you can delete categories and keys that 
were previously entered. **InitFileDeleteEntry()** takes pointers to both the 
null-terminated category name and the null-terminated key name and 
deletes the entry. **InitFileDeleteCategory()** takes only a pointer to the 
null-terminated category name and deletes the entire category, including all 
the keys stored under it.

In addition, you can delete a single string section from a specified blob. 
**InitFileDeleteStringSection()** takes the category and key names of the 
blob as well as the index of the string section, and it deletes the string section. 
If the string section does not exist or if either the key or category can not be 
found, the routine will return an error flag.

### 6.4 General System Utilities

The kernel provides a number of routines that fulfill general needs for system 
utilities. These range from setting the system's date and time to retrieving 
the amount of swap memory used to shutting down the system in order to 
execute a DOS-based program.

#### 6.4.1 Changing the System Clock

TimerGetDateAndTime(), TimerSetDateAndTime()

The system clock reflects the current date and time of day. 
**TimerGetDateAndTime()** takes a pointer to a **TimerDateAndTime** 
structure and fills it with the current year, month, day, day of week, hour, 
minute, and second. **TimerSetDateAndTime()** sets the current date and 
time to the values in the passed structure. It is unusual for any application 
other than the Preferences Manager to change the system clock.

#### 6.4.2 Using Timers

TimerStart(), TimerStop(), TimerSleep(), TimerGetCount()

If your geode needs an action to happen on a timed basis, you will want to use 
a timer. GEOS lets you set up timers that will call a routine or send a message 
after a given interval has passed. GEOS offers four types of timers:

+ One-shot  
A one-shot timer counts a certain number of "ticks." (There are 60 ticks 
in a second.) It will call a routine or send a message after the specified 
number of ticks have been counted.

+ Continual  
A continual timer counts a certain number of ticks and then resets and 
counts again. Each time it reaches the specified number of ticks, it calls 
a routine or sends a message.

+ Millisecond  
A millisecond timer is a one-shot timer that counts in milliseconds rather 
than ticks and calls a routine when time is up (it does not send a 
message).

+ Sleep  
A sleep timer causes the thread that creates it to sleep for a certain 
number of ticks. The sleep timer is unlike the others in that it does not 
send a message or call a routine when time is up; it only awakens the 
sleeping thread.

+ Real-time  
A real-time timer is passed the number of days since 1980, the hour, and 
the minute of the event. On hardware platforms that support such a 
thing, GEOS will wake up when the real-time timer goes off. (All other 
timers are ignored under these conditions.)

All timers except sleep timers are created and started with the routine 
**TimerStart()**, and continual timers can be destroyed with **TimerStop()**. 
**TimerStop()** may also be used to prematurely stop a one-shot timer, though 
if the one-shot sent a message, the message could be in the recipient's queue 
even after **TimerStop()** was called. Sleep timers are created and started 
with **TimerSleep()**, and they do not need to be stopped.

An additional routine, **TimerGetCount()**, returns the current system 
counter. The system counter contains the number of ticks counted since 
GEOS was started.

#### 6.4.3 System Statistics and Utilities

SysStatistics(), SysGetInfo(), SysGetConfig(), 
SysGetPenMode(), SysGetDosEnvironment()

Occasionally, a geode will need to query the kernel about the state or 
configuration of the system. GEOS offers five routines for this:

**SysStatistics()** returns a structure of type **SysStats**. This structure 
contains information about how busy the CPU is, how much swap activity is 
going on, how many context switches occurred in the last second, how many 
interrupts occurred in the last second, and how many runnable threads exist. 
You can not set or otherwise alter this structure.

**SysGetInfo()** returns a particular statistic about the current system status. 
It can return a number of different statistics including the CPU speed, the 
total number of handles in the handle table, the total heap size, the total 
number of geodes running, and the size of the largest free block on the heap. 
Most of the information your application can request with this routine can be 
garnered from the Perf performance meter; it is easiest to use Perf when 
debugging, though a few geodes will need to know this information.

**SysGetConfig()** returns information about the current system's 
configuration including the processor type and machine type. It also returns 
flags about the particular session of GEOS including whether this session was 
started with Swat, whether a coprocessor exists, and other information.

**SysGetPenMode()** returns TRUE if GEOS is currently running on a 
pen-based machine.

**SysGetDosEnvironment()** returns the value of a DOS environment 
variable. It is passed a string representing the variable name and returns a 
string representing the value.

#### 6.4.4 Shutting the System Down

DosExec(), SysShutdown()

There are two ways for a geode to shut down GEOS. The first, **DosExec()**, 
shuts the system down to run a program under DOS, returning after the DOS 
program has finished-unless a task-switch driver is in use, in which case 
the system will create a new task and cause the task-switcher to switch to 
the new task. The second, **SysShutdown()**, forces the system to shut itself 
down completely. Neither of these routines is commonly used by anything 
other than the kernel, GeoManager, special "launcher" programs, or the UI. 
Their use by other libraries or applications is discouraged unless absolutely 
necessary.

**DosExec()** takes several parameters including the pathname of the DOS 
program to be run, arguments for the program, an optional disk handle of the 
disk that contains the program to be run, the optional directory and disk 
handle in which the program should be executed, and a record of 
DosExecFlags. If the return value is nonzero, an error occurred in loading 
the DOS program, and you can use **ThreadGetError()** to check what error 
occurred. Note that **DosExec()** always returns. Applications should not rely 
on **DosExec()** shutting the system down; if a task switcher is present, GEOS 
will be swapped out rather than shut down.

**SysShutdown()** causes GEOS to exit in one of several ways. This routine 
should be passed a shutdown mode. If the mode is SST_CLEAN, 
SST_RESTART, SST_SUSPEND, or SST_CLEAN_FORCED, the routine will 
return; otherwise, it will not return and the shutdown will commence. If 
SST_CLEAN is passed, the shutdown may be aborted after **SysShutdown()** 
returns. You can have **SysShutdown()** cause GEOS to reboot itself after 
shutting down (as the Preferences Manager application does for certain 
preferences settings), but this starts GEOS fresh. This routine is very rarely 
used by anything other than the UI, the kernel, or the Preferences Manager 
application.

If something else (typically the UI or task switcher) shuts the system down, 
objects that register for shutdown notification will receive 
MSG_META_CONFIRM_SHUTDOWN. The application should call 
**SysShutdown()** with the mode SST_CONFIRM_START; this allows the object 
to have exclusive rights for asking the user to confirm the shutdown (when 
the object is finished with the user interaction, it can call **SysShutdown()** 
with SST_CONFIRM_END to release exclusive access). This is useful if your 
application or library has an ongoing operation and wants to verify the 
shutdown with the user.

### 6.5 The Error-Checking Version

GEOS has two versions of its system software. The normal version as shipped 
retail is the "non-error-checking" version. The other, used for debugging 
applications and other geodes, is called the "error-checking" version, or EC 
version. Nearly all components of the system software exist in both 
versions-the kernel, the UI, libraries, drivers, etc.

Together, Swat and the EC version provide extensive and superb debugging 
power. The EC version allows you to call special error-checking routines to 
check the integrity of handles, resources, memory, files, and other things. 
These error-checking routines all begin with EC; for more information, see 
their entries in the Routine Reference Book. In addition to the full error 
checking provided by the EC versions of the system geodes, you can add your 
own error-checking code to your programs. 

#### 6.5.1 Adding EC Code to Your Program

When compiling your code, you can include certain extra lines in either the 
EC version or the non-EC version of your program. For example, during 
debugging you want your program to check the validity of each memory 
handle before locking the memory block; for the final version of your 
program, however, you don't want this validity check because it slows down 
your program. You can easily add a few instructions that will be compiled 
only into the EC version.

GEOS provides five macros, listed below, for version-specific instructions. 
These macros must be treated as statements; they may not be used in 
expressions.

EC  
This macro adds the specified line of code to the EC version 
only. When the non-EC version is compiled from the same code, 
the line will be left out.

EC_ERROR  
This macro halts the execution of the EC version of a program 
by calling **FatalError()** with a specified error code. The call to 
**FatalError()** will not be included in the non-EC version.

EC_ERROR_IF  
This macro works like EC_ERROR, above, but it also allows you 
to set a condition for whether **FatalError()** will be called. If 
the condition is met, **FatalError()** will be called.

NEC  
This macro adds the specified line of code to the non-EC version 
but leaves it out of the EC version. (It is the converse of the EC 
macro.)

EC_BOUNDS  
This macro checks the validity of a specified address (pointer) 
by calling the **ECCheckBounds()** routine. If the pointer is out 
of bounds, **ECCheckBounds()** will call **FatalError()**. This 
macro may only add the bounds check to the EC version.

An example of use of these macros is shown in Code Display 6-4.

---
Code Display 6-4 EC Macros
~~~
/* This code display shows only the usage of these macros; assume that each
 * line shown below exists within a particular function or method. */

/* The EC macro adds a line of code to the EC version. Its format is
 *   EC(line)           where line is the line of code to be added.
 * Note that the NEC macro is similar. */
     EC( @call MyErrorDialogBox::MSG_MY_ERR_PRINT_ERROR(); )
     NEC( @call MyErrorDialogBox::MSG_MY_ERR_PRINT_NO_ERROR(); )

/* The EC_ERROR macro adds a call to FatalError() to the EC version. Its format is
 *   EC_ERROR(code)     where code is the error code to be called. */
     EC_ERROR(ERROR_ATTR_NOT_FOUND)

/* The EC_ERROR_If macro is similar to EC_ERROR but is conditional. Its format is
 *   EC_ERROR_IF(test, code)      where test is a Boolean value and code is the
 * error code to be called. */
     lockVariable = MyAppCheckIfLocked();                 /* TRUE if inaccessible. */
     EC_ERROR_IF(lockVariable, ERROR_ACCESS_DENIED)       /* Error if inaccessible. */

/* The EC_BOUNDS macro adds a call to ECCheckBounds() to the EC version. Its format is
 *   EC_BOUNDS(addr)              where addr is the address to be checked. */
     myPointer = MyAppGetMyPointer();
     EC_BOUNDS(myPointer)
~~~

#### 6.5.2  Special EC Routines

SysGetECLevel(), SysSetECLevel(), SysNotify(), EC-(), 
CFatalError(), CWarningNotice()

While using Swat and the EC version, you can set and retrieve the current 
level of error checking employed. The level is set with a record of flags, each 
of which determines whether a certain type of checking is turned on. This 
record is of type **ErrorCheckingFlags**. You can retrieve it with 
**SysGetECLevel()** and set it with **SysSetECLevel()**.

**SysNotify()** puts up the standard error dialog box-white with a black 
frame. This routine is available in both the EC and the non-EC versions of the 
kernel. This is the box used by the kernel to present unrecoverable errors to 
the user. You can also call it up and allow the user any of five options: retry, 
abort, continue, reboot, or exit. Usually, this dialog box is used only for errors, 
but it can be used for other user notification as well. (Note that with the "exit" 
and "reboot" options, this routine will not return. Note also that only certain 
combinations of the five options are supported.)

The **CFatalError()** and **CWarningNotice()** routines provide run-time and 
compile-time error or warning messages. The **CFatalError()** routine calls 
the kernel's **FatalError** function, which puts up an error dialog box and, in 
Swat, causes Swat to hit a breakpoint so you can debug the error. 
**CWarningNotice()** may be put in error-checking code to make the compiler 
put up a compile-time warning note.

### 6.6 Inter-Application Communication

Applications do not usually need to communicate with each other. Most 
applications will interact only with the kernel and with libraries; such 
applications don't even notice if any other applications are running. However, 
a few applications will need to communicate with other applications.

For example, many desktop-managers provide a "print file" command; the 
user selects a file's icon, then chooses this command to print out the 
appropriate file. The desktop manager will have no way of knowing how to 
print the file; after all, the file could have been created by an application 
which hadn't even been written when the desktop manager was installed. 
The desktop manager therefore causes the appropriate application to be 
started. It then instructs the application to print the file.

Because GEOS is an object-oriented system, there is a natural model for 
inter-application communication. When one application needs to contact 
another, it can simply send a message; thus, sending information or 
instructions to another application is not, in principle, different from sending 
it from one object to another within an application.

The GEOS Inter-Application Communications Protocol (IACP) specifies how 
to open communications with another application, and how to send messages 
back and forth.

#### 6.6.1 IACP Overview

There is a major difference between sending a message within an 
application, and sending one to a different application. When you send a 
message from one object to another within an application, you know that the 
recipient exists, and you know the optr of that recipient. This makes it easy 
to send messages.

When you send a message to another application, however, you do not (at 
first) know any optrs to that application. In fact, you may not even know that 
the application is running. Often, all you will know is something like, "I want 
to send a message to SpiffyWrite".

GEOS uses a client-server model of inter-application communication. Every 
**GeodeToken** corresponds to a server. Whenever an application is launched, 
GEOS checks to see if there is a server-list corresponding to the application's 
token. If there is, GEOS adds the app's Application object to the server-list; if 
there is not, GEOS creates a server-list and adds the Application object to 
that list.

For example, suppose the user launches a single copy of SpiffyWrite; this 
application has a manufacturer-ID of MANUFACTURER_ID_SPIFFYWARE, 
and the token characters "SWRI". GEOS will check if there's a server-list for 
that **GeodeToken**. Let us suppose there isn't such a list; GEOS will 
automatically create one, and add SpiffyWrite's Application object to that 
list. The Application object is now said to be a server for the list.

Now let us suppose another application needs to contact SpiffyWrite; for 
example, perhaps a desktop program needs to print a SpiffyWrite file. It 
tells the kernel that it would like to be a client on the list for the token 
"{MANUFACTURER_ID_SPIFFYWARE, "SWRI"}". GEOS will check to see if a 
server-list for that token exists. If so, it will add the client to that list; this 
will cause a notification message (MSG_META_IACP_NEW_CONNECTION) to 
be sent to every server for that list.

Once a client is linked to a server, it can send a message to the server list. It 
does this by encapsulating a message, then passing the encapsulated 
message to the server-list. GEOS will dispatch the message to every server for 
the list; the server objects will receive it just like any ordinary message. (It 
actually passes the encapsulated message to the server object as an 
argument to MSG_META_IACP_PROCESS_MESSAGE; the server can then 
dispatch the message to the final recipient.)

This establishes the link between the applications. The client can pass an 
optr to the servers by putting it in the encapsulated message; a server object 
can then send messages straight to a particular object.

When a client no longer needs to communicate with a server, it unregisters 
itself from the server list. GEOS then sends a notification message to every 
server object.

Customarily, whoever allocates a global resource must also free it. For 
example, if a client might pass information to the server by allocating a 
global block, writing the data to the block, and passing the block to the server. 
The server should notify the client when the server is finished with that data; 
the client can then free the block. Similarly, if a server allocates a block to 
pass information to a client, the server should free the block.

![](Art/figure_6-1.png)

**Figure 6-1** IACP Clients and Servers  
_A client can encapsulate a message, then use an IACP routine to instruct the 
kernel to send the message to the servers for a server list. There may be several 
servers on any given server list._

There may be several servers for a given server list. For example, if three 
copies of SpiffyWrite were running at once, each of their Application objects 
would be a server for the same server-list. Furthermore, any object can make 
itself a server for any list. All servers will receive copies of every message 
sent to the server list. To distinguish between different servers for a list, 
every server for a list is assigned a distinct **ServerNumber**. If it chooses to, 
a client can specify that a message be sent only to the server with a specific 
number.

#### 6.6.2 GenApplicationClass Behavior

**GenApplicationClass** is built to support IACP automatically. If a server or 
client object is subclassed from GenApplicationClass, most of the work of 
supporting IACP is done transparently to the application writer. The 
following capabilities are built in:

+ The Application Object automatically registers itself as a server for the 
appropriate list when it is launched in application mode. If it is launched 
in engine mode, it registers itself when it receives 
MSG_META_APP_STARTUP. If the last client-connection to the 
application is closed, and the Application is not currently running in 
application mode, the Application object will shut down automatically.

+ When the Application object registers itself as a server for its own list, it 
sends itself MSG_GEN_APPLICATION_IACP_REGISTER. An application 
may subclass this if it wants to take other action at this time (e.g. 
registering itself for other lists). Similarly, when an Application object 
unregisters itself from its own server list, it sends itself 
MSG_GEN_APPLICATION_IACP_UNREGISTER.

+ The Application object automatically handles the MSG_META_IACP- 
messages appropriately. In particular, when the kernel passes an 
encapsulated message to an Application object with 
MSG_META_IACP_PROCESS_MESSAGE, the Application object 
automatically dispatches the message to the appropriate location.

+ An Application object will refuse to quit as long as any client has an open 
IACP connection to it. (It can, however, be forcibly detached; this happens 
when the system is shut down, as noted below.). In such a case, the 
Application object will automatically call **IACPShutdownAll()** to shut 
down all IACP links it has open, whether it is a client or a server on those 
links.

+ When a link is closed, IACP automatically sends 
MSG_META_IACP_LOST_CONNECTION to all objects on the other side of 
the link. When an Application object receives this message, it waits until 
all remaining messages from the link have been handled; it then calls 
**IACPShutdown()** for that connection. It also forwards this message to 
all Document objects, so a Document object will know to close itself if the 
IACP connection was the only reference to it. Again, the Application 
object does this whether it is a client or a server.

+ If the Application object is forcibly detached, it sends itself 
MSG_GEN_APPLICATIONIACP_SHUTDOWN_ALL_CONNECTIONS. The 
default handler for this message will call **IACPShutdownAll()** to shut 
down all IACP links the Application object has open, whether it is a client 
or a server on those links. You can subclass this message if you need to 
take some additional action when the IACP connections are severed.

#### 6.6.3 Messages Across an IACP Link

IACPSendMessage(), IACPSendMessageToServer()

Either a client or a server may send messages over an IACP link. Both clients 
and servers use the same technique. The message sender encapsulates a 
message, and passes the encapsulated message to **IACPSendMessage()**. 
**IACPSendMessage()** dispatches the message to every object on the other 
side of the link. For example, if a client passes a message to 
**IACPSendMessage()**, that message will be dispatched to every server 
object for the specified list.

IACPSendMessage() is passed five arguments:

+ The token for the IACP link.
+ The **EventHandle** of an encapsulated message.
+ The **TravelOption** for that message.
+ An encapsulated completion message; this will be dispatched once each 
time the first message has been successfully handled.
+ A member of the **IACPSide** enumerated type. This tells whether the 
message is being sent by a client or a server. If you pass the value 
IACPS_CLIENT, the message will be dispatched to all servers; if you pass 
IACPS_SERVER, the message will be dispatched to all clients.

The message will be dispatched to all geodes on the other side of a link. Note 
that a client need not send the message to the server object per se. It can use 
the travel options field to direct the message anywhere within the server 
object's geode. It can also specify the optr of the recipient when it 
encapsulates the message; in this case, it should pass a **TravelOption** of -1.

Every time the encapsulated message is successfully handled, the 
"completion message" will be dispatched. Typically, the completion message 
is addressed to the object that called **IACPSendMessage()**, instructing it to 
free global resources that had been allocated for the message.

The routine returns the number of messages that were dispatched. This lets 
the sender know how many completion messages to expect, and lets it 
properly initialize all reference counts to global resources.

A client may choose to send a message to a specific server. It can do this by 
calling **IACPSendMessageToServer()**. This takes almost the same 
arguments as **IACPSendMessage()**. However, instead of being passed an 
**IACPSide** value, it is passed a server number. GEOS will dispatch a single 
copy of the message to the specified server. **IACPSendMessageToServer()** 
returns the number of times the message was dispatched. This will 
ordinarily be one; however, if the specified server is no longer registered, it 
will be zero.

#### 6.6.4 Being a Client

Any object can register as a client for an IACP server list. When an object is 
a client, it can send messages to the server list, which will pass them along 
to the servers for that list.

##### 6.6.4.1 Registering as a Client

IACPConnect(), IACPCreateDefaultLaucnhBlock()

To register as a client for a list, call the routine **IACPConnect()**. When you 
call this routine, you specify which server list you are interested in. If there 
is no such server list running, you can instruct the kernel to start up the 
server list, as well as one of the default applications for that list.

**IACPConnect()** is passed five arguments:

+ The GeodeToken of the list for which you want to register.

+ A set of **IACPConnectFlags**. The following flags are available:

IACPF_OBEY_LAUNCH_MODEL  
This indicates that if GEOS should follow the launch model, 
which will specify whether the user should be presented a 
dialog box, asking the user whether an existing application 
should be used as the server, or a new application launched. If 
you set this flag, you must pass an **AppLaunchBlock()**, with 
the ALB_appMode field set to 
MSG_GEN_PROCESS_OPEN_APPLICATION; you must also set 
IACPF_SERVER_MODE to IACPSM_USER_INTERACTIBLE.

IACPF_CLIENT_OD_SPECIFIED  
This flag indicates that you will specify what object will be the 
new client. If you do not set this flag, the sending geode's 
Application object will be registered as the client.

IACPF_FIRST_ONLY  
If you pass this flag, the client will be connected to only the first 
server on the server list. Any messages sent by the client to the 
server list will be passed only to that one server.

IACPSM_SERVER_MODE  
This field is three bits wide; it holds a member of the 
**IACPServerMode** enumerated type. This type specifies how 
the client expects the server to behave. Currently, only two 
types are supported:

IACPSM_NOT_USER_INTERACTIBLE  
This is equal to zero. It indicates that the server object need not 
interact with the user.

IACPSM_USER_INTERACTIBLE  
This is equal to two. It indicates that the server should be able 
to interact with the user like any normal application.

+ The MemHandle of an AppLaunchBlock. If the server you specify is 
not running, GEOS will launch the application specified by the 
AppLaunchBlock. If you pass a null MemHandle, GEOS will return an 
error if no server is running.

+ The optr of the client object. This is ignored if 
IACPF_CLIENT_OD_SPECIFIED was not passed.

+ A pointer to a word. IACPConnect() will write the number of servers on 
the list to that word.

**IACPConnect()** returns a word-sized **IACPConnection** token. You will 
need to pass that token when you call another IACP routine to use the 
connection. It will also return the number of server objects on the list; it 
returns this value by writing it to the address indicated by the pointer 
passed.

If the server list you indicate is not currently running, IACP may do one of 
two different things. If you pass a null handle as the third argument, 
**IACPConnect()** will fail. It will return the error value 
IACP_NO_CONNECTION, and indicate that there are no servers on the 
specified list.

If you pass an **AppLaunchBlock**, **IACPConnect()** will examine that 
launch block to see what application should be launched to act as a server. 
The AppLaunchBlock should specify the location and name of the application 
to open. If the ALB_appRef.AIR_diskHandle field is non-zero, 
**IACPConnect()** will look in the specified disk or standard path for an 
application with the right **GeodeToken**; otherwise, it will look in the 
standard places for an application.

Note that if you pass a launch block to **IACPConnect()**, you may not alter 
or free it afterwards. If the application is created, the block you pass will be 
its launch block; if not, the kernel will free the block automatically. In any 
event, the caller no longer has access to the block.

If **IACPConnect()** launches an application, the caller will block until that 
application has been created and registers for the server list. If the 
application does not register for that list, the caller will never unblock. You 
must therefore make sure that you are launching the right application for the 
list. Note that every application object will automatically register for the 
server list which shares its token.

To create a launch block, you should call 
**IACPCreateDefaultLaunchBlock()**. This routine is passed a single 
argument, which specifies how the application will be opened. That 
argument must be MSG_GEN_PROCESS_OPEN_APPLICATION (the 
application will be opened as a standard, user-interactible application); 
MSG_GEN_PROCESS_OPEN_ENGINE (the application will be opened in 
engine mode, i.e. with no user interface); or 
MSG_GEN_PROCESS_OPEN_CUSTOM (which has an application-specified 
meaning).

**IACPCreateDefaultLaunchBlock()** allocates a launch block and sets up 
its fields appropriately. As created, the launch block will have the following 
characteristics:

+ The application's initial working directory (i.e. the launch block's 
ALB_diskHandle field) will be SP_DOCUMENT.

+ No application directory will be specified in the launch block (i.e. 
ALB_appRef.AIR_diskHandle will be zero); **IACPConnect()** will attempt 
to find the application on its own.

+ No initial data file will be specified (i.e. ALB_dataFile will be blank).

+ The application will determine its own generic parent (i.e. 
ALB_genParent will be null).

+ No extra data word will be passed to the application (i.e. ALB_extraData 
will be zero).

+ There will be no output descriptor (i.e. ALB_userLoadAckOutput and 
ALB_userLoadAckMessage will be null.)

**IACPCreateDefaultLaunchBlock()** returns the handle of the 
newly-created launch block. Once the block is created, you can alter any of its 
fields before passing the launch block to **IACPConnect()**. (Once you pass the 
launch block to **IACPConnect()**, you may not alter it any more.)

Often a client will want the server to open a specific document. For example, 
if a desktop-manager is implementing a "print-file" command, it will need to 
open a server application, instruct it to open the file to be printed, and then 
instruct it to print the file. To make the server open a document, pass the 
document's name in ALB_dataFile. The server will open the file when you 
register, and close it when you unregister.

##### 6.6.4.2 Unregistering as a Client

IACPShutdown(), IACPShutdownAll()

If an application no longer needs to interact with a particular server list, it 
should call **IACPShutdown()**. This routine is also used by servers which 
wish to remove themselves from a server list. This section describes how the 
routine is used by clients; section 6.6.5 of chapter 6 describes how it is used 
by servers.

The routine is passed two arguments:

+ The **IACPConnection** token for the link which is being shut down.

+ An optr. This must be null if the routine is being called by a client.

**IACPShutdown()** sends MSG_META_IACP_LOST_CONNECTION to all 
objects on the other side of the link; that is, if a client calls 
**IACPShutdown()**, all servers on the list will be sent this message.

**IACPShutdownAll()** closes all IACP links for the application which calls it. 
The Application object automatically calls this routine when the application 
is exiting.

#### 6.6.5 Being a Server

Every time an application is launched, its Application object automatically 
registers as a server for the server list that shares its **GeodeToken**. The 
Application class has default handlers for all the notification messages IACP 
sends to the server objects.

If you wish, you can have another object act as a server. However, if you do 
this, you will have to do more of the work yourself. While the notification 
messages are defined for **MetaClass**, and thus can be handled by any class 
of object, **MetaClass** does not come with handlers for these messages; if the 
server is not subclassed from **GenApplicationClass**, you will have to write 
the handlers yourself. This is discussed below.

##### 6.6.5.1 Registering and Unregistering a Server

IACPRegisterServer(), IACPUnregisterServer()

You will not generally need to register and unregister a server object 
explicitly. As noted above, when an application is launched, the application 
object is automatically registered as a server for the list with its 
**GeodeToken**; when the application exits, the Application object is 
automatically unregistered from that list.

However, you may wish to explicitly register an object as a server. For 
example, you might want your application object to be a server on a list with 
a different **GeodeToken**; or you might want to register a non-Application 
object as a server. In this case, you will need to explicitly register and 
unregister the object.

To register an object as a server, call **IACPRegisterServer()**. This routine 
is passed the following arguments:

+ The **GeodeToken** of the list for which you are registering as a server.

+ The optr of the object which is registering as a server. This object must 
be able to handle the MSG_META_IACP- messages appropriately (this is 
built into **GenApplicationClass**).

+ A member of the **IACPServerMode** enumerated type. This type 
specifies how the client expects the server to behave. Currently, only two 
types are supported:

    IACPSM_NOT_USER_INTERACTIBLE  
    This is equal to zero. It indicates that the server object will not 
interact directly with users.

    IACPSM_USER_INTERACTIBLE  
    This is equal to two. It indicates that the server will be able to 
interact with the user like any normal application.

+ A set of **IACPServerFlags**. Currently, only one flag is supported: 
IACP_MULTIPLE_INSTANCES, indicating that multiple copies of the 
application might be running at once. (Every multi-launchable 
application should set this flag.)

**IACPRegisterServer()** registers the object as a server for the appropriate 
list; it creates the server list if necessary.

To unregister an object as a server, call **IACPUnregisterServer()**. This 
routine is passed two arguments: the **GeodeToken** of the server list, and the 
optr of the server. The object will be removed immediately from the server 
list. Note, however, that the server list might have already dispatched some 
messages to the server being removed; these messages might be waiting on 
the server object's queue, and thus the server object might get some IACP 
messages even after it calls **IACPUnregisterServer()**. One way to deal 
with this is to have the server object send itself a message, via the queue, 
immediately after it calls **IACPUnregisterServer()**. When the object 
receives this message, it will know that it has no more IACP messages on its 
queue.

Every server object on a given server list has a unique server number. This 
server number will not change while the server is attached to the list. A 
server object can find out its server number by calling 
**IACPGetServerNumber()**. This routine takes two arguments: the 
**GeodeToken** of the server list, and the optr to the server object. It returns 
the object's server number.

##### 6.6.5.2 Non-Application Servers and Clients

MSG_META_IACP_PROCESS_MESSAGE, IACPProcessMessage(), 
MSG_META_IACP_NEW_CONNECTION, 
MSG_META_IACP_LOST_CONNECTION

Every server and client object must be able to handle certain messages. 
**GenApplicationClass** comes with handlers for these messages, so you need 
not write them yourself. However, if you will be using some other kind of 
object as the server, you must handle the messages yourself. You may also 
choose to have your application object subclass any of these messages; in that 
case, you should generally have your handler use **@callsuper**.

When a server or client sends an IACP message, the kernel passes the 
encapsulated message to each object on the other side of the link. It does this 
by sending the message MSG_META_IACP_PROCESS_MESSAGE to each 
object. This message comes with three arguments:

msgToSend  
The **EventHandle** of the encapsulated message.

topt  
The **TravelOption** for that message.

completionMessage  
The **EventHandle** of any message to be sent after msgToSend 
has been dispatched. (This field may be set to zero, indicating 
that there is no completion message.)

The recipient of MSG_META_IACP_PROCESS_MESSAGE should call 
**IACPProcessMessage()**. This routine is passed four arguments: the optr of 
the object calling the routine, and the three arguments passed with 
MSG_META_IACP_PROCESS_MESSAGE. **IACPProcessMessage()** 
dispatches both encapsulated messages properly.

Remember, if the client or server is subclassed from **GenApplicationClass**, 
all of this is done for you. You need only write a handler for the message if the 
client or server object is not from a subclass of **GenApplicationClass**.

Whenever a client registers on an IACP list, the kernel sends 
MSG_META_IACP_NEW_CONNECTION to all servers on that list. This 
message comes with three arguments:

appLaunchBlock  
This is the handle of the **AppLaunchBlock** which the server 
passed to **IACPConnect()**.

justLaunched  
This is a Boolean value. If the server's application has just been 
launched specifically to be a server, this will be true (i.e. 
non-zero).

IACPConnection  
This is the token for the IACP connection.

When an object (either client or server) removes itself from an IACP 
connection, the kernel sends MSG_META_IACP_LOST_CONNECTION to all 
objects on the other side of the link. This message has two parameters: 

connection  
The token for the IACP list.

serverNum  
If a server removed itself from the list (and this message is 
being sent to clients), this field will have the number of the 
server that removed itself. If a client removed itself from the 
list, this field will be zero.

Whenever a new client is attached to a server list, 
MSG_META_IACP_NEW_CONNECTION is sent to every server object. This 
message comes with three arguments:

appLaunchBlock  
This is the handle of the application's launch block. The default 
**GenApplicationClass** handler examines the launch block to 
see if the application should open a document.

justLaunched  
If the application was just launched specifically to be a server, 
this field will be true (i.e. non-zero).

connection  
This is the token for the IACP connection.

When a client is removed from a server list, every server object is sent 
MSG_META_IACP_LOST_CONNECTION. Similarly, when a server is removed, 
every client object is sent MSG_META_IACP_LOST_CONNECTION. The 
message comes with two arguments:

IACPConnection  
This is the token for the IACP connection.

serverNum  
If a server left the list (and the message is being sent to clients), 
this argument will hold its serverNum. If a client left the list 
(and the message is being sent to servers), this argument will 
be set to zero.

[GEOS Programming](ccoding.md) <-- &nbsp;&nbsp; [table of contents](../concepts.md) &nbsp;&nbsp; --> [The Clipboard](cclipb.md)
