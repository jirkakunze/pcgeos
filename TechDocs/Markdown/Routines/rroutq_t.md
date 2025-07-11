## 3.6 Routines Q-T
----------
#### qsort
    extern void _pascal qsort(
            void *array, 
            word count, 
            word elementSize,
            PCB(int, compare, (const void *, const void *)));

This is a standard quicksort routine. The callback routine must be decared 
_pascal.

----------
#### QueueGetInfo()
    word    QueueGetInfo(
            QueueHandle qh);            /* queue to query */

This routine returns information about a specific event queue. Pass the 
handle of the queue; for information about the current process' queue, pass a 
null handle. This routine returns the number of events (or messages) 
currently in the queue.

**Include:** geode.h

----------
#### QueueGetMessage()
    EventHandle QueueGetMessage(
            QueueHandle qh);            /* queue to query */

This routine returns the next message on the given queue, blocking if the 
queue is empty. When a new message is added to the empty queue, this 
routine will unblock the thread and return the message. This routine is used 
almost exclusively by the kernel.

**Include:** geode.h

----------
#### QueuePostMessage()
    void    QueuePostMessage(
            QueueHandle     qh,         /* queue to add event to */
            EventHandle     event,      /* event to be added to queue */
            MessageFlags    flags);     /* MF_INSERT_AT_FRONT or zero */

This routine adds the specified *event* to the passed queue. The only valid flag 
for this routine is MF_INSERT_AT_FRONT, which will put the event in the 
first spot of the queue.

**Include:** geode.h

----------
#### RangeEnum()
    Boolean RangeEnum(
            CellFunctionParameters  * cfp,      /* cell function parameters */
            RangeEnumParams         * params);  /* special other parameters */

This routine calls a callback routine for each cell in a specified range. This 
routine is passed pointers to two structures, both of which are shown below. 
It returns *false* if all the cells were processed, *true* if any of the cells caused 
the routine to abort before the end of the range was reached.

**Callback Parameters:** 
The callback routine, which must be declared _pascal, receives a 
**RangeEnumCallbackParams** structure, which has the following 
definition:

    typedef struct {
        RangeEnumParams         *RECP_params;       /* see below */
    /* current row, column, and cell data of cell */
        word            RECP_row;
        word            RECP_column;
        word            RECP_cellData;
    } RangeEnumCallbackParams;

The callback routine can do anything with the cell information. It should 
return *false* after successfully processing the cell; if an error occurs, or if it 
wants to abort the **RangeEnum()**, it should return *true*.

**Structures:** The CellFunctionParameters structure has the following definition:

    typedef struct {
        CellFunctionParameterFlags      CFP_flags;
            /* can have the following flags:
             * CFPF_DIRTY
             *  set parameter block dirty
             * CFPF_NO_FREE_COUNT
             *  counts the number of calls to
             *  a non-special RangeEnum() */
        VMFileHandle            CFP_file;
            /* VM file handle of cell file */
        VMBlockHandle           CFP_rowBlocks[N_ROW_BLOCKS];
            /* array of handles to grouped row blocks */
    } CellFunctionParameters;

**Include:** cell.h

----------
#### RangeExists()
    Boolean RangeExists( /* returns non-zero if there are cells in range */
            CellFunctionParameters  * cfp,          /* see RangeEnum() */
            word                    firstRow,       /* range delimiters */
            byte                    firstColumn,
            word                    lastRow,
            byte                    lastColumn);

This routine returns *true* if there are any cells in the specified range. It is 
passed a pointer to the **CellFunctionParameters** structure for the cell file, 
as well as the indices of the first and last row, and the first and last column, 
of the range to check.

**Include:** cell.h

----------
#### RangeInsert()
    void    RangeInsert(
            CellFunctionParameters  * cfp,      /* see RangeEnum() */
            RangeInsertParams       * rep);     /* parameters structure */

This routine shifts existing cells to make room for new ones. (It does not 
actually create new cells.) Which cells are shifted, and in what direction, is 
specified by the **RangeInsertParams()** structure. This structure has three 
fields:

*RIP_bounds* - A **Rectangle** structure which specifies which cells should be 
shifted. The cells currently in this range will be shifted across 
or down, depending on the value of *RIP_delta*; the shifted cells 
displace more cells, and so on, to the edge of the visible portion 
of the cell file. To insert an entire row (which is much faster 
than inserting a partial row), set **RIP_bounds.R_left** = 0 and 
**RIP_bounds.R_right** = LARGEST_COLUMN.

*RIP_delta* - A **Point** structure which specifies how far the cells should be 
shifted and in which direction. If the range of cells is to be 
shifted horizontally, *RIP_delta.P_x* should specify how far the 
cells should be shifted over, and *RIP_delta.P_y* should be zero. 
If the cells are to be shifted vertically, *RIP_delta.P_y* should 
specify how far the cells should be shifted down, and 
*RIP_delta.P_x* should be zero.

*RIP_cfp* - This is the address of the **CellFunctionParameters** 
structure. You don't have to initialize this; the routine will do 
so automatically.

**Include:** cell.h

**Warnings:** If cells are shifted off the "visible" portion of the cell file, you will be unable 
to access them by row or column numbers; but they will not be deleted. For 
this reason, you should free all such cells *before* calling **RangeInsert()**. (You 
can find out if there are any cells at the edges by calling **RangeExists()**.) For 
an explanation of the "visible" and "scratch-pad" portions of a cell file, see 
Section 19.4.1 of the Concepts book.

----------
#### realloc()
    void *  realloc(
            void *      blockPtr,       /* address of memory to resize */
            size_t      newSize);       /* New size of memory in bytes */

The **malloc()** family of routines is provided for Standard C compatibility. If 
a geode needs a small amount of fixed memory, it can call one of the routines. 
The kernel will allocate a fixed block to satisfy the geode's **malloc()** requests; 
it will allocate memory from this block. When the block is filled, it will 
allocate another fixed malloc-block. When all the memory in the block is 
freed, the memory manager will automatically free the block.

If a geode needs to change the size of a section of memory assigned to it by 
the **malloc()** family of routines, it should use **realloc()**. **realloc()** resizes the 
piece of memory specified and returns the memory's new base address.

If the new size is smaller then the previous size, bytes will be cut off from the 
end. The request is guaranteed to succeed. Furthermore, the memory will not 
be moved; the address returned will be the same as the address passed.

If the new size is larger than the previous size, **realloc()** may move the data 
to accommodate the request. If so, it will return the new address. The new 
memory added will not be zero-initialized. If **realloc()** cannot fulfill the 
request, it will return a null pointer, and the memory will not be altered.

Resizing a stretch of memory down to zero bytes is exactly the same as 
freeing it with **free()**. If you pass a null address to realloc(), it will allocate 
the memory the same way **malloc()** does.

The memory must be in a malloc-block assigned to the geode calling 
**realloc()**. If you want to resize memory in another geode's malloc-block, call 
**GeoReAlloc()**.

**Warnings:** Pass exactly the same address as the one returned to you when you allocated 
the memory. If you pass a different address, the results are undefined.

**See Also:** calloc(), free(), malloc(), GeoReAlloc()

----------
#### SerialClose()
    StreamError SerialClose(
            GeodeHandle         driver,
            SerialUnit          unit,
            Boolean             linger);

Close the stream to a serial port.

----------
#### SerialCloseWithoutReset()
    StreamError SerialClose(
            GeodeHandle         driver,
            SerialUnit          unit,
            Boolean             linger);

Close the stream to a serial port, without actually resetting the port.

----------
#### SerialFlush()
    StreamError SerialFlush(
            GeodeHandle         driver,
            SerialUnit          unit,
            StreamRoles         roles);

Flush all data pending in a serial port's input or output buffer (depending on 
the value of *roles*).

----------
#### SerialGetFormat()
    StreamError SerialGetFormat(
            GeodeHandle         driver,
            SerialUnit          unit,
            SerialFormat *      format,
            SerialMode *        mode,
            SerialBaud *        baud);

Get the format of a stream to a specified serial port.

----------
#### SerialGetModem()
    StreamError SerialGetModem(
            GeodeHandle         driver,
            SerialUnit          unit,
            SerialModem *       modem);

Read a modem's hardware flow control bits.

----------
#### SerialOpen()
    StreamError SerialOpen(
            GeodeHandle         driver,
            SerialUnit          unit,
            StreamOpenFlags     flags,
            word                inBuffSize,
            word                outBuffSize,
            word                timeout);

This routine opens a stream to the specified serial port. It is passed the 
following arguments:

*driver* - The **GeodeToken** of the serial driver.

*unit* - The serial port to open.

*flags* - This specifies whether the call should fail if the port is busy, or 
wait for a time to see if it will become free.

*inBuffSize* - The size of the stream buffer used for input from the serial port.

*outBuffSize* - The size of the stream buffer used for output to the serial port.

*timeout* - The number of clock ticks to wait for the port to become free. 
(This argument is ignored if *flags* is not 
STREAM_OPEN_TIMEOUT.)

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### SerialQuery()
    StreamError SerialQuery(
            GeodeHandle         driver,
            SerialUnit          unit,
            StreamRoles         role,
            word *              bytesAvailable);

Find out how much space is available in a serial buffer, or how much data is 
waiting to be read.

----------
#### SerialRead()
    StreamError SerialRead (
            GeodeHandle     driver,
            SerialUnit      unit,
            StreamBlocker   blocker,
            word            buffSize,
            byte *          buffer,
            word *          numBytesRead);

Read data from a serial port and write it to a passed buffer.

----------
#### SerialReadByte()
    StreamError SerialReadByte (
            GeodeHandle     driver,
            SerialUnit      unit,
            StreamBlocker   blocker,
            word            buffSize,
            byte *          dataByte);

Read a byte of data from a serial port and write it to a passed variable.

----------
#### SerialSetFormat()
    StreamError SerialSetFormat(
            GeodeHandle         driver,
            SerialUnit      unit,
            SerialFormat        format,
            SerialMode      mode,
            SerialBaud      baud);

Set the format for a stream to a specified serial port.

----------
#### SerialSetModem()
    StreamError SerialSetModem(
            GeodeHandle         driver,
            SerialUnit          unit,
            SerialModem         modem);

Set a modem's hardware flow control bits.

----------
#### SerialWrite()
    StreamError SerialWrite(
            GeodeHandle         driver,
            SerialUnit          unit,
            StreamBlocker       blocker,
            word                buffSize,
            const byte *        buffer,
            word *              numBytesWritten);

Write data to a serial port.

----------
#### SerialWriteByte()
    StreamError SerialWrite(
            GeodeHandle         driver,
            SerialUnit          unit,
            StreamBlocker       blocker,
            word                buffSize,
            byte                dataByte);

Write one byte of data to a serial port.

----------
#### SGC_MACHINE
    byte    SGC_MACHINE(val);
            dword   val;

This macro is used to extract the machine type from a **SysGetConfig()** 
return value.

**Include:** system.goh

----------
#### SGC_PROCESSOR
    byte    SGC_PROCESSOR(val);
            dword   val;

This macro is used to extract the processor type from a **SysGetConfig()** 
return value.

**Include:** system.goh

----------
#### SoundAllocMusic()
    MemHandle   SoundAllocMusic(
            const word      *song, 
            word            voices );

This routine takes a pointer to a fixed buffer of music and returns a 
**MemHandle** which may then be passed to **SoundPlayMusic()** to play the 
music. If the music buffer is in a movable resource, you must initialize it 
using **SoundInitMusic()** instead of **SoundAllocMusic()**. To find out how to 
set up one of these buffers of music, see "Sound Library," Chapter 13 of the 
Concepts book. The *voices* argument is the number of voices in the buffer. 

----------
#### SoundAllocMusicNote()
    MemHandle SoundAllocMusicNote(
            word _far       instrument, 
            word            frequency, 
            word            volume,
            word            DeltaType, 
            word            duration);

This routine allocates a **MemHandle** which may be passed to 
**SoundPlayMusicNote()**. You must provide all information about the note: 
its frequency, volume, and duration. You may specify an instrument, passing 
a value corresponding to a standard *instrument* (such as IP_PIANO). Specify 
the frequency in Hertz or use one of the constants such as MIDDLE_C_b to 
specify a standard note frequency. Volume ranges from zero to 0xffff-you 
may wish to use a constant value such as DYNAMIC_FFF if you want help 
trying to choose a loudness. The note's duration is determined by its delta 
type, one of SSDTT_MSEC, SSDTT_TICKS, and SSDTT_TEMPO. If you pass 
SSDTT_MSEC or SSDTT_TICKS, the duration is measured in milliseconds or 
ticks (each tick is one sixtieth of a second). If you pass SSDTT_TEMPO, you 
may set the size of your time unit when you call **SoundPlayMusicNote()**. 
The *duration* determines how many time units the note should play. If the 
delta type is SSDTT_TICKS and *duration* is 30, then the note will sound for 
half a second.

----------
#### SoundAllocMusicStream()
    MemHandle   SoundAllocMusicStream(
            word        streamType,
            word        priority,
            word        voices,
            word        tempo);

This routine returns a token suitable for passing to 
**SoundPlayMusicToStream()**. It is passed several arguments. The 
**SoundStreamType** determines how much space to allocate for the stream 
and will determine how much data can be written to the stream at one time. 
If you pass SST_ONE_SHOT, it indicates that the stream will not be explicitly 
destroyed, and that your stream should destroy the stream when the song is 
done. You must specify how many voices there are in the music buffer. You 
must also pass a starting *tempo* for the music stream.

----------
#### SoundAllocSampleStream()
    MemHandle SoundAllocSampleStream(void);

This routine allocates a sample stream handle. If the returned handle is *null*, 
the library was unavailable (i.e. some other thread has grabbed exclusive 
access).

----------
#### SoundDisableSampleStream()
    void    SoundDisableSampleStream(
            MemHandle       mh);

This routine disassociates the DAC player from the passed sample handle. 
Before you play more sounds using the handle, you will have to call 
**SoundEnableSampleStream()** again. 

----------
#### SoundEnableSampleStream()
    Boolean     SoundEnableSampleStream(
            MemHandle       mh,
            word            priority,
            word            rate,
            word            manufacturerID,
            word            format);

This routine associates a DAC player with the allocated sample handle. You 
must pass the sound handle, as returned by **SoundAllocSampleStream()**. 
You must also pass certain pieces of information about the sound you will be 
playing on the DAC device: the *priority* with which to grab the DAC player 
(e.g. SP_STANDARD), the sampling rate, and the *format* of the sample (as 
identified by a *manufacturerID* and a **DACSampleFormat** value).

----------
#### SoundFreeMusic()
    void    SoundFreeMusic(
            MemHandle       mh);

This routine frees up a music handle. The music must not be playing; call 
**SoundStopMusic()** if you are not sure. You may not use the music handle 
after calling this routine on it.

----------
#### SoundFreeMusicNote()
    void SoundFreeMusicNote(
            MemHandle       mh);

This routine frees up the passed note handle. The note must not be playing 
when you call this routine; call **SoundStopMusicNote()** if you are not sure. 
You should not try to use the note's handle after freeing it.

----------
#### SoundFreeMusicStream()
    void SoundFreeMusicStream(
            MemHandle       mh);

This routine frees up the music stream's token. No music must be playing via 
the stream; call **SoundDisableMusicStream()** if you are not sure. Do not 
try to use the stream after calling this routine on it.

----------
#### SoundFreeSampleStream()
    void    SoundFreeSampleStream(
            MemHandle       mh);

This routine frees the passed sampled sound handle. You must not try to use 
this handle after calling this routine on it.

----------
#### SoundGetExclusive()
    void    SoundGetExclusive(void);

This routine grabs the exclusive semaphore for the sound library; if another 
thread has already grabbed the exclusive, this routine will wait until the 
exclusive is released. Sounds which are playing now will be permitted to 
finish, but from now on, only the thread calling this routine will be allowed 
to play new sounds. When done with the sound library exclusive, call 
**SoundReleaseExclusive()**.

----------
#### SoundGetExclusiveNB()
    Boolean SoundGetExclusiveNB(void);

This routine grabs the exclusive semaphore for the sound library, doing so 
even if some other thread has already grabbed the exclusive. Sounds which 
are playing now will be permitted to finish, but from now on, only the thread 
calling this routine will be allowed to play new sounds. This routine will 
return *true* if another thread already has exclusive access.

When done with the sound library exclusive, call 
**SoundReleaseExclusive()**. 

----------
#### SoundInitMusic()
    void SoundInitMusic(
            MemHandle       mh, 
            byte            voices);

This routine initializes a pre-defined simple music buffer structure. If the 
music buffer is stored in a fixed block, you can call **SoundAllocMusic()** 
instead. This allows a music buffer stored in a block referenced by a pointer 
to be playable using **SoundPlayMusic()**.

----------
#### SoundPlayMusic()
    Boolean     SoundPlayMusic(
            MemHandle       mh, 
            word            priority,
            word            tempo, 
            char            flags);

This routine plays a buffer of music previously initialized by 
**SoundInitMusic()** or allocated by **SoundAllocMusic()**. The *priority* value 
will determine whether your sound will play if other sounds are already 
occupying the voices - pass a value such as SP_STANDARD. The *tempo* value 
will be used to determine the length of a 1/128th note. If your music buffer 
contained any notes whose lengths were measured by SSDTT_TEMPO delta 
type, then you should set this value accordingly. The *flags* argument 
determines whether the music's handle should be automatically freed when 
the sound is done playing. You may pass either or both of the flags 
UNLOCK_ON_EOS or DESTROY_ON_EOS.

Remember that you must have called **SoundInitMusic()** on the music 
handle before you may use it to play music.

**Include:** sound.h

----------
#### SoundPlayMusicNote()
    Boolean     SoundPlayMusicNote(
            MemHandle       mh,             /* handle of note */
            word            priority, 
            word            tempo,
            word            flags);

This routine plays a buffer of music previously allocated by 
**SoundAllocMusicNote()** - the return value of that function is passed as 
*mh*. The *priority* value will determine whether your sound will play if other 
sounds are already occupying the voices - pass a value such as 
SP_STANDARD. The *tempo* value will be used to determine the length of a 
1/128th note. If your note's delta type is SSDTT_TEMPO, then you should set 
this value accordingly. The *flags* argument determines whether the notes's 
handle should be automatically freed when the note is done playing. You may 
pass either or both of the flags UNLOCK_ON_EOS or DESTROY_ON_EOS.

This routine returns *true* if the library was unavailable (i.e. if some other 
thread had grabbed the sound exclusive).

**Include:** sound.h

----------
#### SoundPlayToMusicStream()
    Boolean     SoundPlayToMusicStream(
            MemHandle                   mh,
            const word                  * sample,
            word                        size,
            SampleFormatDescription     *format);

This routine plays a music buffer to a stream. Specify which stream to play 
to by means of the token returned by **SoundAllocMusicStream()**. To play 
music to the buffer, pass the size of the buffer you are playing and a pointer 
to the start of the piece. This piece of buffer must be made up of whole 
events - it should not start or end in the middle of an event (e.g. you can't 
specify that you want to play a note but not give its frequency, even if you 
plan to play another buffer to the stream that might begin with a frequency). 

If you don't know the size of the buffer, it may be all right - any data in the 
buffer after the GE_END_OF_SONG will be ignored.

----------
#### SoundPlayToSampleStream()
    Boolean SoundPlayToSampleStream(
            MemHandle                   mh,
            word _far                   * sample,
            word                        size,
            SampleFormatDescription     * format);

This routine passes sampled sound data to a DAC player. You must pass a 
sample sound handle to this routine - to acquire such a handle, call 
**SoundAllocSampleStream()**. The sample sound handle must be 
associated with a DAC player - to so associate the handle, call 
**SoundEnableSampleStream()**. You must pass a pointer to the *sample* 
data, along with the *size* of the sample as measured in bytes. You may change 
the *format* information which will determine how the DAC player handles the 
data.

----------
#### SoundReallocMusic()
    Boolean     SoundReallocMusic(
            MemHandle       mh,
            word _far       * song);

This routine allows you to associate a new music buffer with an existing 
music handle. The new music buffer must not have more voices than was 
originally requested with **SoundAllocMusic()**. Do not call this routine with 
the handle of a sound that may be playing; call **SoundStopMusic()** on the 
handle if you are not sure. See "Sound Library," Chapter 13 of the Concepts 
book to find out how to set up the buffer of music.

----------
#### SoundReallocMusicNote()
    Boolean     SoundReallocMusicNote(
            MemHandle       mh,
            word            freq,
            word            vol,
            word            timer,
            word            durat,
            word _far       * instrum);

This routine allows you to associate new note values with an existing note 
handle. Do not call this routine with the handle of a note that may be playing; 
call **SoundStopMusicNote()** on the handle if you are not sure. 

----------
#### SoundReleaseExclusive()
    void    SoundReleaseExclusive(void);

This routine releases the sound library exclusive semaphore. You will not 
need to call this routine unless your code calls **SoundGrabExclusive()** or 
**SoundGrabExclusiveNB()**. This routine allows other threads to play 
sounds. If another thread called **SoundGrabExclusive()** while your thread 
had the exclusive, it will now grab the exclusive.

----------
#### SoundStopMusic()
    Boolean     SoundStopMusic(
            MemHandle       mh);        /* Handle of music buffer */

This routine stops the playing of a simple music buffer. It returns *true* if the 
library was unavailable (i.e. some other thread has grabbed the exclusive).

----------
#### SoundStopMusicNote()
    Boolean     SoundStopMusicNote(
            MemHandle       mh);

This routine stops a note that is playing. Pass the handle of the note, as was 
returned by **SoundAllocMusicNote()**. This routine returns true if the 
sound library was unavailable (i.e. some other thread has grabbed the 
exclusive). 

----------
#### SoundStopMusicStream()
    Boolean     SoundStopMusicStream(
            MemHandle       mh);

This routine stops any music being played to the stream. All sounds are 
flushed from the stream. It takes one argument, the token of the sound 
stream, as returned by **SoundAllocMusicStream()**.

----------
#### SoundStopSampleStream()
    void    SoundStopSampleStream(
            MemHandle       mh);

This routine stops a sound playing through a previously allocated sampled 
sound stream.

----------
#### SpoolConvertPaperSize()
    word    SpoolConvertPaperSize(
            int         width,          /* width of paper */
            int         height,         /* height of paper */
            PageType    pt);            /* type of page */

This routine converts a width and height into a page size number.

**Include:** spool.goh

----------
#### SpoolCreatePaperSize()
    Boolean     SpoolCreatePaperSize( /* Returns true if failed */
            word        * retValue, /* returns paper size value */
            char        * name,     /* name of paper size */
            int         width,      /* width of paper */
            int         length,     /* length of paper */
            PageLayout  layout);    /* default page layout /

This routine defines and stores a new paper size for later use by the user.

**Include:** spool.goh

----------
#### SpoolCreatePrinter()
    Boolean SpoolCreatePrinter(     /* Returns true if error 
                                       (printer already exists) */
            char                *name,      /* name of printer */
            PrinterDriverType   type,       /* driver type */
            int                 * retVal);  /* Will hold printer number */

Adds the printer to the list of currently installed printers and returns the 
new printer number. This routine is normally called from within the 
Preferences manager. Returns *true* if the printer already exists.

**Include:** spool.goh

----------
#### SpoolDeletePaperSize()
    Boolean     SpoolDeletePaperSize(
            word    size);      /* size number to delete */

This routine deletes a user-defined paper size.

**Include:** spool.goh

----------
#### SpoolDeletePrinter()
    void    SpoolDeletePrinter(
            int     prtrNum);       /* printer number to delete */

Deletes the requested printer from the system.

**Include:** spool.goh

----------
#### SpoolGetDefaultPrinter()
    int     SpoolGetDefaultPrinter(); /* Returns printer number */

Returns the system-default printer, which is used (for example) by the 
**PrintControlClass** as the default printer to print to.

**Include:** spool.goh

----------
#### SpoolGetNumPaperSizes()
    int     SpoolGetNumPaperSizes(
            PageType    type);      /* type of page */

Use this routine to find the number of paper sizes, both pre-defined and 
user-defined, that should appear in a paper size list.

**Include:** spool.goh

----------
#### SpoolGetNumPrinters()
    int     SpoolGetNumPrinters(
            PrinterDriverType       type);          /* driver type */

This routine returns the number of installed printers with the given type.

**Structures:**

    typedef ByteEnum PrinterDriverType;
    /* The driver type may be one of the following:
            PDT_PRINTER,
            PDT_PLOTTER,
            PDT_FACSIMILE,
            PDT_CAMERA,
            PDT_OTHER,
            PDT_ALL     */

**Include:** spool.goh

----------
#### SpoolGetPaperSize()
    XYSizeAsDWord SpoolGetPaperSize(
            int         size,       /* This must be between 0 and the return 
                                     * value of SpoolGetNumPaperSizes() */
            PageType    pt,         /* type of page */
            PageLayout  *layout);   /* Will hold returned page layout */

Use this routine to determine the dimensions of a paper size.

**Include:** spool.goh

----------
#### SpoolGetPaperSizeOrder()
    dword   SpoolGetPaperSizeOrder( /* High byte is number of unused sizes;
                                     * Low byte is # of ordered sizes */
            PageType    type,
            byte        *order,     /* buffer of size MAX_PAPER_SIZES */
                                    /* On return, this buffer will be 
                                     * filled with the page size numbers
                                     * arranged in the order 
                                     * corresponding to their display */
            byte        *userSizes); /* buffer of size MAX_PAPER_SIZES */
                                     /* On return, will hold ordered 
                                     * array of user paper sizes. */

----------
#### SpoolGetPaperSizeString()
    Boolean     SpoolGetPaperSizeString( /* true if error*/
            char        * retValue, /* buffer for returned value */
            int         size,       /* Must be between 0 and the return
                                     * value of SpoolGetNumPaperSizes() */
            PageType    pt);        /* type of page */

Use this routine to determine the string to be displayed for a specific paper 
size. Upon return, *retValue* will point to a character string and the Boolean 
return value will be *false* if successful. If any error occurs, or if the page type 
couldn't be found, the returned value will be *true*.

**Include:** spool.goh

----------
#### SpoolGetPrinterString()
    Boolean SpoolGetPrinterString( /* Returns true if error */
            int     *retValue,  /* On return, will point to length of string */
            char    *string,    /* returned name string */
            int     prtrNum);   /* printer number */

This routine fills a buffer with the requested null-terminated printer name 
string. If the printer could not be found, the return value will be *true* (set for 
error).

**Include:** spool.goh

----------
#### SpoolSetDefaultPrinter()
    void    SpoolSetDefaultPrinter(
            int prtrNum);       /* printer number */

Sets the system-default printer, used (for example) by **PrintControlClass** 
as the default printer. This routine is normally called from within the 
Preferences manager.

**Include:** spool.goh

----------
#### SpoolSetDocSize()
    void    SpoolSetDocSize(
            Boolean         open;           /* false if document is closed */
            PageSizeInfo    * psr);         /* NULL if document is closed */

This routine tells the application's **PageSizeControl** object the document's 
size.

**Include:** spool.goh

----------
#### SpoolSetPaperSizeOrder()
    void    SpoolSetPaperSizeOrder(
            void    * ptr,      /* Array of PageSizeOrder entries */
            word    number);    /* number of entries in array */

This routine resets the order in which paper sizes are displayed to the user.

**Include:** spool.goh

----------
#### SpreadsheetInitFile()
    VMBlockHandle SpreadsheetInitFile(
            const SpreadsheetInitFileData           * ifd);

This routine initializes a VM file for use by the spreadsheet object. It allocates 
a spreadsheet map block in the file and initializes this block. The routine 
returns the map block's handle; applications will need to remember this 
handle. It does not change any existing blocks in the VM file.

The *ifd* parameter is pointer to a **SpreadsheetInitFileData** structure 
containing the file handle and the number of rows and columns to allocate.

**Structures:** The **SpreadsheetInitFileData** structure is defined as follows:

    typedef struct {
        word                    SIFD_file;
        word                    SIFD_numRows;
        word                    SIFD_numCols;
        SpreadsheetDrawFlags    SIFD_drawFlags;
    } SpreadsheetInitFileData;

    /* SpreadsheetDrawFlags:
     * SDF_DRAW_GRAPHICS
     * SDF_DRAW_NOTE_BUTTON
     * SDF_DRAW_HEADER_FOOTER_BUTTON
     * SDF_DRAW_GRID                */

**Include:** ssheet.goh

----------
#### StreamClose()
    StreamError StreamClose (
            GeodeHandle         driver,
            StreamToken         stream,
            Boolean             linger);

This routine shuts down a stream. It is passed the following arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*linger* - Set *true* (i.e., non-zero) if the data currently in the stream 
should be kept until it's read; set *false* to flush the data 
immediately.

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### StreamFlush()
    StreamError StreamFlush (
            GeodeHandle         driver,
            StreamToken         stream);

This routine flushes all the data pending in a stream. It is passed the 
following arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### StreamOpen()
    StreamError  StreamOpen (
            GeodeHandle         driver,
            word                buffSize,
            GeodeHandle         owner,
            HeapFlags           heapFlags,
            StreamToken *       stream);

This routine opens a stream. It is passed the following:

*driver* - The **GeodeToken** of the stream driver.

*buffSize* - The size of the stream buffer, in bytes.

*owner* - The geode which will own the stream.

*heapFlags* - The flags for the creation of the buffer block.

**stream* - The stream token will be written here.

If **StreamOpen()** is successful, it returns zero and writes the stream's token 
to **stream*. If it is unsuccessful, it returns a member of the **StreamError** 
enumerated type.

----------
#### StreamQuery()
    StreamError StreamQuery (
            GeodeHandle         driver,
            StreamToken         stream,
            StreamRoles         role,
            word *              bytesAvailable);

This routine finds out either how much free space is available in a stream's 
buffer, or how much data is waiting to be read. It is passed the following 
arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*role* - If this is STREAM_ROLES_WRITER, the routine will return the 
amount of free space available in the stream buffer. If it is 
STREAM_ROLES_READER, it will return the amount of data 
waiting to be read.

**bytesAvailable* - The routine will write the number of bytes available (for 
writing or reading) to this variable.

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### StreamRead()
    StreamError StreamRead (
            GeodeHandle         driver,
            StreamToken         stream,
            StreamBlocker       blocker,
            word                buffSize,
            byte *              buffer,
            word *              numBytesRead);

This routine reads data from a stream. The routine takes the following 
arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*blocker* - Specify whether to block if there is not enough data waiting to 
be read.

*buffsize* - Size of passed buffer (i.e. amount of data to read from stream).

*buffer* - Pointer to buffer where data from stream will be written.

**numBytesReadRead* - **StreamRead()** will write to this variable the number of bytes 
actually read from the stream.

If **StreamRead()** is successful, it returns zero. If it is unsuccessful, or could 
not read all the data requested from the stream, it returns a member of the 
*StreamError* enumerated type.

----------
#### StreamReadByte()
    StreamError StreamWriteByte (
            GeodeHandle         driver,
            StreamToken         stream,
            StreamBlocker       blocker,
            byte *      dataByte);

This routine reads a single byte from a stream. It takes the following 
arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*blocker* - Specify whether to block if there is not enough room to write 
the data.

**dataByte* - Read a byte from the stream, and write it to this variable.

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### StreamWrite()
    StreamError StreamWrite (
            GeodeHandle         driver,
            StreamToken         stream,
            StreamBlocker       blocker,
            word                buffSize,
            const byte *        buffer,
            word *              numBytesWritten);

This routine writes data to a stream. The routine takes the following 
arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*blocker* - Specify whether to block if there is not enough room to write all 
the data.

*buffsize* - Size of passed data buffer (i.e. amount of data to write to 
stream).

*buffer* - Pointer to data to write to stream.

**numBytesWritten* - StreamWrite() will write to this variable the number of bytes 
actually written to the stream.

If **StreamWrite()** is successful, it returns zero. If it is unsuccessful, or could 
not write all the data to the stream, it returns a member of the **StreamError** 
enumerated type.

----------
#### StreamWriteByte()
    StreamError StreamWriteByte (
            GeodeHandle         driver,
            StreamToken         stream,
            StreamBlocker       blocker,
            byte                dataByte);

This routine writes a single byte to a stream. It takes the following 
arguments:

*driver* - The **GeodeToken** of the stream driver.

*stream* - The **StreamToken** of the stream.

*blocker* - Specify whether to block if there is not enough room to write 
the data.

*dataByte* - Write this byte to the stream.

If the routine is successful, it returns zero. If it is unsuccessful, it returns a 
member of the **StreamError** enumerated type.

----------
#### SysGetConfig()
    dword   SysGetConfig();

This routine returns a set of values defining the system configuration. The 
returned dword contains four byte values, listed below from least significant 
byte to most significant byte:

**configuration flags**  
This byte contains a record of **SysConfigFlags** reflecting the 
system status. This record includes information on how the 
system was started, whether Swat is running it, whether the 
system was restarted, etc.

**reserved byte**  
This byte contains reserved information unusable by 
applications.

**processor type**  
This byte contains a value reflecting the processor type of the 
machine running GEOS. This is of type **SysProcessorType** 
and is one of SPT_8088, SPT_8086, SPT_80186, SPT_80286, 
SPT_80386, or SPT_80486. Use the macro SGC_PROCESSOR to 
extract this value from the returned dword.

**machine type**  
This byte contains a value of **SysMachineType** indicating the 
type of the machine running GEOS. It may be one of the 
following values: SMT_UNKNOWN, SMT_PC, SMT_PC_CONV, 
SMT_PC_JR, SMT_PC_XT, SMT_PC_XT_286, SMT_PC_AT, 
SMT_PS2_30, SMT_PS2_50, SMT_PS2_60, SMT_PS2_80, or 
SMT_PS1. Use the macro SGC_MACHINE to extract this value 
from the returned dword.

**Include:** system.h

----------
#### SysGetDosEnvironment()
    Boolean SysGetDosEnvironment( /* true if error (not found) */
            const char      * variable,     /* environment variable */
            char            * buffer,       /* buffer for return value */
            word            bufSize);       /* maximum return string length */

This routine looks up a specified DOS environment variable in the 
environment buffer. It takes three parameters:

*variable* - A pointer to the null-terminated character string representing 
the name of the variable to be searched for.

*buffer* - A pointer to a locked or fixed buffer in which the variable's 
value will be returned.

*bufSize* - The size of the passed buffer in bytes (the maximum number of 
characters that can be returned including the terminating null 
character).

If the variable is not found, the error flag returned will be true.

**Include:** system.h

----------
#### SysGetECLevel()
    ErrorCheckingFlags SysGetECLevel(
            MemHandle * checksumBlock);

This routine checks the current error-checking level of the system. The 
returned record of **ErrorCheckingFlags** describes which levels of error 
checking are turned on and which are off. If checksum error checking 
(ECF_BLOCK_CHECKSUM) is on, pass a pointer to the handle of a block on 
which the checksum will be done.

**Include:** ec.h

----------
#### SysGetInfo()
    dword   SysGetInfo(
            SysGetInfoType info);       /* type of information to retrieve */

This routine returns general system information. Pass the type of 
information to be returned; the value returned depends on the type passed in 
info. Note that the largest returned value is a dword; many different return 
values should be cast to the appropriate type when calling **SysGetInfo()**.

The info parameter (of **SysGetInfoType**) can have one of the following 
values:

SGIT_TOTAL_HANDLES  
Returns the total number of handles in the kernel's handle 
table.

SGIT_HEAP_SIZE  
Returns the total heap size in bytes.

SGIT_LARGEST_FREE_BLOCK  
Returns the size (in bytes) of the largest possible block that 
may be allocated at the moment.

SGIT_TOTAL_COUNT  
Returns the total number of clock ticks since the current 
session of GEOS started (subtracts the initial system clock 
value from the current time).

SGIT_NUMBER_OF_VOLUMES  
Returns the total number of volumes registered with the 
system.

SGIT_TOTAL_GEODES  
Returns the total number of geodes currently loaded.

SGIT_NUMBER_OF_PROCESSES  
Returns the total number of processes currently loaded.

SGIT_NUMBER_OF_LIBRARIES  
Returns the total number of libraries currently loaded.

SGIT_NUMBER_OF_DRIVERS  
Returns the total number of drivers currently loaded.

SGIT_CPU_SPEED  
Returns the CPU speed of the processor. The value returned 
will be ten times the ratio of the CPU speed relative to a base 
XT processor.

SGIT_SYSTEM_DISK  
Returns the disk handle of the disk on which GEOS (the 
GEOS.INI file) resides.

SGIT_UI_PROCESS

**Include:** sysstats.h

----------
#### SysGetPenMode()
    Boolean SysGetPenMode();

This routine returns true if GEOS is running on a pen-based system, false if 
it is not.

**Include:** system.h

----------
#### SysLocateFileInDosPath()
    DiskHandle SysLocateFileInDosPath( /* sets thread's error value */
            const char      * fname,        /* file name */
            char            * buffer);      /* returned path of file */

This routine searches for a specified file along the search path specified in the 
DOS environment variable PATH. The parameters are

*fname* - A pointer to the null-terminated file name to search for.

*buffer* - A pointer to a locked or fixed buffer into which the full path of 
the file will be placed.

This routine returns the disk handle of the disk on which the file resides as 
well as the file's full path (with drive name) in the *buffer* pointed to by buffer. 
The path returned is a null-terminated character string. If the file could not 
be found, a null disk handle will be returned. The error value can be retrieved 
with **ThreadGetError()**.

**Include:** system.h

----------
#### SysNotify()
    word    SysNotify(
            SysNotifyFlags      flags,          /* options to offer user */
            const char          * string1,      /* first string to display */
            const char          * string2);     /* second string to display */

This routine causes the kernel to put up a standard notification dialog box on 
the screen. This dialog box is white with a black border and is used nearly 
exclusively for error notification by the kernel. Pass this routine the following 
parameters:

*flags* - A record of **SysNotifyFlags** indicating the options the dialog 
presents to the user. These flags are shown below.

*string1* - A pointer to a null-terminated character string put up in the 
dialog box (may be a null pointer).

*string2* - A pointer to a second null-terminated string presented in the 
dialog box (may be a null pointer).

The returned word is the user's response, based on the **SysNotifyFlags** 
passed (see below).

**Structures:** **SysNotifyFlags** is a record of several flags; none, any, or all of the flags may 
be set at a time. The five flags are

SNF_RETRY  
Allow the user to retry the operation that brought up the 
notification box. If the user selects this option, it will be 
returned by the routine.

SNF_EXIT  
Allow the user to exit GEOS entirely. If the user selects this 
option, it will be returned by the routine after an 
SST_CLEAN_FORCED shutdown has been initiated.

SNF_ABORT  
Allow the user to abort the operation that brought up the 
notification box. If the user selects this option, it will be 
returned by the routine.

SNF_CONTINUE  
Allow the user to continue the operation. If the user selects this 
option, it will be returned by the routine.

SNF_REBOOT  
Allow the user to shut down and reboot GEOS directly. If the 
user selects this option, the routine will not return.

**Include:** system.h

----------
#### SysRegisterScreen()
    void    SysRegisterScreen(
            GeodeHandle     driver,
            WindowHandle        root);

----------
#### SysSetECLevel()
    void    SysSetECLevel(
            ErrorCheckingFlags  flags,          /* level of error checking */
            MemHandle           checksumBlock); /* block to check, if any */

This routine sets the error-checking level of the software. Pass it a record of 
**ErrorCheckingFlags** indicating which levels of error checking should be 
employed. If checksum checking (ECF_BLOCK_CHECKSUM) is turned on, 
also pass the handle of a block on which the checksum will be performed.

**Include:** ec.h

----------
#### SysSetExitFlags()
    word    SysGetExitFlags(
            ExitFlags       bitsToSet,
            ExitFlags       bitsToClear);

----------
#### SysShutdown()
    Boolean SysShutdown(
            SysShutdownType     type,
            ...);

This routine causes the system to shut down, exiting to the native operating 
system (typically DOS). It takes variable parameters depending on the first 
parameter. The first parameter is the type of shutdown requested, and it 
determines the calling format of the routine. **SysShutdown()** returns a 
Boolean value dependent on the type of shutdown.

The parameters and calling format for this routine depend on the value in the 
*type* parameter. The possible values (**SysShutdownType**) are listed below 
with the associated parameter and return information.

SST_CLEAN  
Shut down all applications cleanly, allowing any that wish to to abort the 
shutdown. The routine will return *true* if a system shutdown is already 
in progress at the time of the call. This type of shutdown will send 
MSG_META_CONFIRM_SHUTDOWN to all objects registered on the 
MANUFACTURER_ID_GEOWORKS:GCNSLT_SHUTDOWN_CONTROL GCN 
list (but only if the shutdown is not cancelled). Each object on that list 
must return an acknowledgment of the shutdown. The parameter format 
and parameters are

    Boolean SysShutdown(
        SysShutdownType     type,
        optr                notificationOD,
        Message             msg);

*notificationOD* - The optr of an object which will receive the message passed in 
*msg* after the shutdown has been acknowledged. Pass a null 
optr to use the default notification (MSG_META_DETACH sent 
to the UI).

*msg* - The message to be sent to the object in *notificationOD*.

SST_CLEAN_FORCED  
Shut down all applications cleanly without the possibility of cancellation. 
This type takes no additional parameters and does not allow other geodes 
to abort the shutdown. It will return, but the return value will be 
meaningless.

SST_DIRTY  
Attempt to exit device drivers and close all files without shutting down 
applications. Does not return. The parameters of this type are

    Boolean SysShutdown{
        SysShutdownType     type,       /* SST_DIRTY */
        const char          * reason);

The *reason* parameter is a pointer to a text string presented to the user 
as a reason for the dirty shutdown. The string is null-terminated. Pass -1 
if no reason is to be given.

SST_PANIC  
Exit system device drivers (GA_SYSTEM) without exiting applications or 
closing files. This can be bad for the system and should be used only in 
emergency situations. This type of shutdown takes no additional 
parameters and does not return.

SST_REBOOT  
This is used by GEOS when the user hits *Ctrl-Alt-Del*. Applications 
should not call it.

SST_RESTART  
This is like SST_CLEAN_FORCED above, but it reloads GEOS after 
shutting down rather than exit completely. It takes no additional 
parameters; it will return TRUE if the system could not be restarted, 
FALSE if the shutdown has been initiated.

SST_FINAL  
Perform the final phase of a shutdown. This routine is called *only* by the 
UI when the SST_CLEAN_FORCED shutdown is complete. This type does 
not return, and it takes one additional parameter. The calling format and 
parameters of this type are

    Boolean SysShutdown(
        SysShutdownType         type,
        const char              * reason);

The *reason* parameter is a character string explaining the reason 
(typically an error) for the final shutdown.

SST_SUSPEND  
Suspend system operation in preparation for task switching, and 
broadcast MSG_META_CONFIRM_SHUTDOWN to all objects on the 
MANUFACTURER_ID_GEOWORKS:GCNSLT_SHUTDOWN_CONTROL GCN 
list (see **MetaClass**). All notified objects must return acknowledgment of 
the shutdown. This type of **SysShutdown()** returns *true* if another 
system shutdown is already in progress. It takes two additional 
parameters:

    Boolean SysShutdown(
        SysShutdownType     type,
        optr                notificationOD,
        Message             msg);

*notificationOD* - The optr of an object which will receive the message passed in 
*msg* after the shutdown has been acknowledged. Pass a null 
optr to use the default notification (MSG_META_DETACH sent 
to the UI), though this is not usually the intent of the call.

*msg* - The message to be sent to the object in *notificationOD*.

SST_CONFIRM_START  
Called by the recipient of MSG_META_CONFIRM_SHUTDOWN; this 
allows shutdown confirmation dialog boxes to be presented in order to the 
user. The caller of this type will be blocked until all previous callers have 
finished their confirmation procedure. When **SysShutdown()** returns, 
the caller may present its confirmation dialog and continue or abort the 
shutdown. If **SysShutdown()** returns *true* from a call with this type, the 
caller should *not* present the confirmation dialog to the user and need not 
call **SysShutdown()** with SST_CONFIRM_END; another thread has 
already cancelled the shutdown. This type takes no additional 
parameters.

SST_CONFIRM_END  
The counterpart of SST_CONFIRM_START, this ends the confirmation 
sequence in an object's MSG_META_CONFIRM_SHUTDOWN handler. It 
takes one additional parameter and returns nothing. The calling format 
is shown below:

    void    SysShutdown(
        SysShutdownType     type,
        Boolean             confirm);

The *confirm* parameter should be TRUE if the shutdown is to be 
continued, FALSE if the shutdown should be aborted.

**Include:** system.h

**Warnings:** Most applications should not call **SysShutdown()**. Any that do should do so 
with extreme care.

----------
#### SysStatistics()
    void    SysStatistics(
            SysStats * stats);          /* returned statistics */

This routine returns system performance statistics. Pass it a pointer to an 
empty **SysStats** structure; the routine will fill in the appropriate fields. 
**SysStats** has the following structure:

    typedef struct {
        dword           SS_idleCount;
        SysSwapInfo     SS_swapOuts;
        SysSwapInfo     SS_swapIns;
        word            SS_contextSwitches;
        word            SS_interrupts;
        word            SS_runQueue;
    } SysStats;

**Include:** sysstats.h

----------
#### SysUnlockBIOS()
    void    SysUnlockBIOS(void);

----------
#### TextSearchInString()
    char *  TextSearchInSTring(
            const char      *str1,
            conat char      *startPtr,
            const char      *endPtr,
            word            strSize,
            const char      *str2,
            word            str2Size,
            word            searchOptions,
            word            *matchLen);

TextSearchInString() searches in a single text chunk for a passed text string. 
If a match is found, a pointer to that match (and the length of the match) are 
returned in passed buffers. 

*str1* is a pointer to the main string you will be searching in.

*startPtr* and *endPtr* are pointers to locations within *str1* to begin and end the 
search.

*strSize* stores the size of *str1*, or zero if null-terminated.

*str2* stores the match string, which may include wildcards (type **WildCard**).

*str2Size* stores the size of *str2*, or zero if null-terminated.

*searchOptions* stores the **SearchOptions** to use by the search mechanism. 
The high byte should be zeroed.

*matchLen* stores a buffer to store the size of the matched word. (The matched 
word itself is returned by the routine.)

**Include:** Objects/vTextC.goh

----------
#### TextSearchInHugeArray()
    dword   TextSearchInSTring(
            char            *str2,
            word            str2Size,
            dword           str1Size,
            dword           curOffset,
            dword           endOffset,
            FileHandle      hugeArrayFile,
            VMBlockHandle   hugeArrayBlock,
            word            searchOptions,
            word            *matchLen);

TextSearchInHugeArray() searches in a huge array for a passed text string. 
If a match is found, a dword offset to the match (and the length of the match) 
are returned in passed buffers. 

*str2* stores the match string, which may include wildcards (type **WildCard**).

*str2Size* stores the size of *str2*, or zero if null-terminated.

*str1Size* stores the total length of the string being searched.

*curOffset* stores the offset from the start of *str1* to the first character to check.

*endOffset* stores the offset from the start of *str1* to the last character to check.

*hugeArrayFile* stores the file handle of the huge array.

*hugeArrayBlock* stores the VM block handle of the huge array.

*searchOptions* stores the **SearchOptions** to use by the search mechanism. 
The high byte should be zeroed.

*matchLen* stores a buffer to store the size of the matched word. (The matched 
word itself is returned by the routine.)

**Include:** Objects/vTextC.goh

----------
#### TGI_PRIORITY()
    byte    TGI_PRIORITY(val);
            word    val;

This macro extracts the thread priority from the value returned by 
**ThreadGetInfo()**.

----------
#### TGI_RECENT_CPU_USAGE()
    byte    TGI_RECENT_CPU_USAGE(val);
            word    val;

This macro extracts the recent CPU usage from the value returned by 
**ThreadGetInfo()**.

----------
#### ThreadAllocSem()
    SemaphoreHandle ThreadAllocSem(
            word    value);         /* allowable locks on the semaphore */

This routine allocates and initializes a new semaphore for private use by a 
multithreaded application. Pass the value with which to initialize the 
semaphore; this value represents the number of threads that can grab the 
semaphore before other grab attempts will block. Typically, the passed value 
will be one. The routine returns the handle of the new semaphore.

**Include:** sem.h

----------
#### ThreadAllocThreadLock()
    ThreadLockHandle ThreadAllocThreadLock();

This routine allocates a special semaphore called a thread lock. With a 
normal semaphore, a thread that grabs the semaphore twice without 
releasing it will deadlock; with a thread lock, a thread can grab it more than 
once in succession. The thread has to release it once for each time it grabs the 
thread lock, however.

In all other aspects, however, the thread lock resembles a normal semaphore. 
**ThreadAllocThreadLock()** returns the handle of the new thread lock.

**Include:** sem.h

----------
#### ThreadAttachToQueue()
    void    ThreadAttachToQueue(
            QueueHandle     qh,             /* queue to attach */
            ClassStruct     * class);       /* primary class of thread */

This routine attaches the calling thread to the passed event queue. This is 
used only for event-driven threads. Typically, this routine is called when a 
thread is created; attaching to queues is automatic in nearly all cases, and 
you will rarely need this routine.

Pass the handle of the queue in *qh* and a *class* pointer in class. The class will 
be attached to the event queue and will handle all messages sent directly to 
the thread. This class should nearly always be a subclass of **ProcessClass**.

If a queue handle of zero is passed, the thread wants to "reattach" to the 
current queue. This is used typically during shutdown of event-driven 
threads, and it is nearly always taken care of automatically by 
**ProcessClass**.

**Include:** thread.h

----------
#### ThreadCreate()
    ThreadHandle ThreadCreate(
            word    priority,       /* Initial base priority of new thread */
            word    valueToPass,    /* Optional data to pass to new thread */
            word    (*startRoutine)(word valuePassed),
                                    /* Pointer to entry routine */
            word    stackSize,      /* Size of the stack for the new thread */
            GeodeHandle owner);     /* Geode that will own the new thread */

This routine creates a new procedural thread for a process. If you need a new 
event-driven thread, send MSG_PROCESS_CREATE_EVENT_THREAD to your 
process object instead.

Pass the following parameters to this routine:

*priority* - The priority of the new thread. Typically this will be one of the 
standard thread priorities (see below).

*valueToPass* - A word of optional data to be passed to the entry routine of the 
new thread. This can be used, for example, to indicate the 
thread's initial context or for initializing thread variables.

*startRoutine* - A pointer to the entry routine to be executed immediately for 
the thread. This may be in either fixed or movable memory. The 
segment must be a virtual segment. Note that if the routine is 
in movable memory, it may degrade heap performance for the 
life of the thread (its movable block will remain locked for 
extended stretches of time). The routine may return the 
thread's exit code or may call **ThreadDestroy()** directly.

*stackSize* - The stack size allocated for the thread. 512 bytes is typically 
enough for threads doing neither UI nor file system work; 
threads working with the file system will require 1 K. Threads 
working with UI objects will require 3 K.

*owner* - The geode handle of the geode that will own the thread. If the 
calling thread's geode will own the new thread, it can call 
**GeodeGetProcessHandle()** prior to calling 
**ThreadCreate()**.

**ThreadCreate()** returns the thread handle of the new thread. If an error 
occurs, the calling thread's error code will be set and a null handle returned; 
you should likely call **ThreadGetError()** to retrieve the error code after 
creating the new thread. A return of NO_ERROR_RETURNED from 
**ThreadGetError()** means no error occurred.

The standard thread priorities that may be passed in the *priority* parameter 
are listed below:

PRIORITY_TIME_CRITICAL  
The highest priority of all; you should not use this in general 
because it will pre-empt nearly all other threads. (It may be 
useful, however, during debugging.)

PRIORITY_HIGH  
A high priority; generally only used for highly important 
threads.

PRIORITY_UI  
Another high priority; this is used for User Interface threads to 
provide quick response to user actions.

PRIORITY_FOCUS  
A medium-level priority; this is used for whatever thread has 
the current input focus (whichever thread the user is currently 
working with).

PRIORITY_STANDAR  D
The standard application thread priority; you should typically 
use this when creating new threads.

PRIORITY_LOW  
A low priority for tasks that can be done in the background.

PRIORITY_LOWEST  
The lowest standard priority; it is used for threads that can 
take any amount of time to complete.

**Include:** thread.h

----------
#### ThreadDestroy()
    void    ThreadDestroy(
            word    errorCode,  /* Error code to indicate cause of destruction */
            optr    ackObject,  /* Object to receive destruction acknowledgment */
            word    ackData);   /* Additional word of data to pass (as the low
                                 * word of optr for source of MSG_META_ACK) */

This routine causes the current (calling) thread to exit and then destroy 
itself. The thread is responsible for ensuring that it has no leftover resources 
allocated or semaphores locked.

Pass it an error code or exit code meaningful to the application and the other 
threads in the application. This error code will be used by the debugger to 
determine the cause of the thread's exit; a null error code usually indicates 
successful completion of the thread's task.

Pass also the optr of the object to receive acknowledgement of the thread's 
destruction. The object specified will receive MSG_META_ACK after the 
calling thread is completely destroyed.

**Be Sure To:** Always clean up before exiting a thread. Unlock locked resources, free 
allocated memory, etc. You do not have to do these things for the application's 
primary thread; the process object (the primary thread) will automatically 
clean up after itself.

**Include:** thread.h

----------
#### ThreadFreeSem()
    void    ThreadFreeSem(
            SemaphoreHandle sem);           /* semaphore to be freed */

This routine frees the specified semaphore that had been allocated with 
**ThreadAllocSem()**. You must be sure that no threads are using the 
semaphore or will use it after it has been freed. Subsequent access attempts 
could cause illegal handle errors or worse.

**Include:** sem.h

----------
#### ThreadFreeThreadLock()
    void    ThreadFreeThreadLock(
            ThreadLockHandle sem);              /* thread lock to be freed */

This routine frees the specified thread lock that had been allocated with 
**ThreadAllocThreadLock()**. You must be sure that no threads are using or 
will use the thread lock after it has been freed. Subsequent attempts to grab 
or release the thread lock could cause illegal handle errors.

**Include:** sem.h

----------
#### ThreadGetError()
    word    ThreadGetError(void)

This routine returns the thread's current error value.

----------
#### ThreadGetInfo()
    word    ThreadGetInfo(
            ThreadHandle        th,     /* thread to get information about */
            ThreadGetInfoType   info);  /* type of information to get */

This routine gets information about the specified thread. The information 
desired is specified in the *info* parameter; the subject thread is specified in 
the *th* parameter. If the thread handle passed is zero or a null handle, the 
routine will return information about the calling thread.

The *info* parameter is one of the following values of **ThreadGetInfoType**, 
specifying the type of information to be returned by **ThreadGetInfo()**:

TGIT_PRIORITY_AND_USAGE  
The returned word will contain both the thread's priority and 
the thread's recent CPU usage. To extract the priority of the 
thread, use the macro TGI_PRIORITY; to extract the recent CPU 
usage, use the macro TGI_RECENT_CPU_USAGE.

TGIT_THREAD_HANDLE  
Useful only when the *th* parameter is zero, this will return the 
thread handle of the subject thread. If *th* is zero, the handle of 
the calling thread will be returned.

TGIT_QUEUE_HANDLE  
The returned word will contain the queue handle of the 
event-driven thread specified in *th*. If the thread specified is not 
event-driven, a null queue handle will be returned.

**Include:** thread.h

----------
#### ThreadGrabThreadLock()
    void    ThreadGrabThreadLock(
            ThreadLockHandle sem);              /* thread lock to grab */

This routine attempts to grab the thread lock for the calling thread. If the 
thread lock is currently held by another thread, the caller will block until the 
lock becomes available. If the caller already has the thread lock, it will grab 
the lock again and continue executing.

**Be Sure To:** Thread locks must be released with **ThreadReleaseThreadLock()** once for 
each time they are grabbed.

**Warnings:** This routine provides no deadlock protection for multiple threads. If multiple 
threads will be grabbing multiple thread locks, the locks should always be 
grabbed in the same order to minimize the potential for deadlock.

**Include:** sem.h

----------
#### ThreadHandleException()
    void    ThreadHandleException(
            ThreadHandle        th,         /* thread to handle the exception */
            ThreadExceptions    exception,  /* exception to handle */
            void    (*handler)  ());        /* pointer to handler */

This routine allows a thread to set up a handler for a processor exception. 
This can be useful for debugging purposes. Pass the following three 
parameters:

*th* - The handle of the thread to handle the exception. Pass zero for 
the current thread.

*exception* - A **ThreadException** type (see below).

*handler* - A pointer to a handler in fixed or locked memory. Pass a null 
pointer to use the GEOS default exception handler.

**Structures:** The **ThreadException** type has the following values:

    TE_DIVIDE_BY_ZERO
    TE_OVERFLOW
    TE_BOUND
    TE_FPU_EXCEPTION
    TE_SINGLE_STEP
    TE_BREAKPOINT

**Include:** thread.h

----------
#### ThreadModify()
    void    ThreadModify(
            ThreadHandle        th,                 /* thread to modify */
            word                newBasePriority,    /* thread's new base priority */
            ThreadModifyFlags   flags);             /* flags (see below) */

This routine modifies the priority of the specified thread. Use it to either set 
the base priority of the thread or reset the current CPU usage to zero. The 
parameters should have the following values:

*th* - The thread handle; pass zero to change the priority of the 
calling thread.

*newBasePriority* - The new base priority of the thread. Use one of the standard 
priorities - see **ThreadCreate()** - or use a value between zero 
and 255.

*flags* A record of **ThreadModifyFlags**; pass TMF_BASE_PRIO to 
change the thread's base priority or TMF_ZERO_USAGE to reset 
the thread's recent CPU usage to zero.

**Warnings:** Unless the thread is timing-critical, you should not set the base priority to 
zero.

**Include:** thread.h

----------
#### ThreadPrivAlloc()
    word    ThreadPrivAlloc(
            word            wordsRequested,     /* number of words to allocate */
            GeodeHandle     owner);             /* handle of geode to own data */

This routine allocates a number of contiguous words in the private data of all 
geodes (loaded and yet-to-be loaded). It is exactly the same as 
**GeodePrivAlloc()**; see the entry for that routine.

**Include:** thread.h

**See Also:** GeodePrivAlloc()

----------
#### ThreadPrivFree()
    void    ThreadPrivFree(
            word    range,              /* offset to first word to be freed */
            word    wordsRequested);    /* number of words to free */

This routine frees a number of contiguous private-data words previously 
allocated with **ThreadPrivAlloc()**. It is similar to **GeodePrivFree()**; see 
the entry for that routine for full information.

**Include:** thread.h

**See Also:** GeodePrivFree()

----------
#### ThreadPSem()
    SemaphoreError ThreadPSem(
            SemaphoreHandle sem);               /* semaphore to grab */

This routine attempts to grab the passed semaphore via a "P" operation. If 
the semaphore has already been grabbed, the thread will block until the 
semaphore becomes available, even if it was grabbed by the same thread.

**ThreadPSem()** returns an error code of type **SemaphoreError**, described 
in **ThreadPTimedSem()**, below. The error code is intended to indicate 
abnormal return by the previous thread; if the semaphore never becomes 
available, the thread will block indefinitely and the routine will not return.

**Be Sure To:** When the thread no longer needs the semaphore, it should release it with 
ThreadVSem().

**Warnings:** This routine provides no deadlock protection. If threads will grab multiple 
common semaphores, they should always grab/release them in the same 
order, minimizing the chances for deadlock.

A thread may not try to grab a particular semaphore twice without releasing 
it in between grabs. The thread will block on itself and will deadlock. If a 
thread may need to grab the semaphore twice in a row, it should use a thread 
lock instead (see **ThreadAllocThreadLock()** for more information).

**Include:** sem.h

----------
#### ThreadPTimedSem()
    SemaphoreError ThreadPTimedSem(
            SemaphoreHandle     sem,        /* semaphore to grab */
            word                timeout);   /* ticks before timeout */

This routine attempts to grab the passed semaphore via a "P" operation. If 
the semaphore has already been grabbed, the thread will block for at most 
the number of ticks specified in *timeout*.

**ThreadPTimedSem()** returns an error code of type **SemaphoreError**, 
which has three values:

SE_NO_ERROR  
No error occurred and the semaphore was grabbed properly.

SE_TIMEOUT  
The time elapsed and the semaphore was not grabbed. If this 
value is returned, the thread should *not* proceed with whatever 
protected operation was to happen. Instead, it should either 
attempt to grab the semaphore again or should proceed with 
other tasks.

SE_PREVIOUS_OWNER_DIED  
The previous owner of the semaphore exited abnormally. If the 
thread currently holding the semaphore exited without 
releasing the semaphore, for example, this would be returned.

Often *timeout* is passed as zero to indicate that if the semaphore isn't 
available right now, the thread will go on with some other action.

**Be Sure To:** When the thread no longer needs the semaphore, it should release it with 
**ThreadVSem()**.

**Warnings:** This routine provides no deadlock protection. If threads will grab multiple 
common semaphores, they should always grab/release them in the same 
order, minimizing the chances for deadlock.

A thread may not try to grab a particular semaphore twice without releasing 
it in between grabs. The thread will block on itself and will deadlock. If a 
thread may need to grab the semaphore twice in a row, it should use a thread 
lock instead, though there is no timeout equivalent for thread locks (see 
**ThreadAllocThreadLock()** for more information).

**Include:** sem.h

----------
#### ThreadReleaseThreadLock()
    void    ThreadReleaseThreadLock(
            ThreadLockHandle sem);              /* threadlock to release */

This routine releases the specified thread lock previously grabbed with 
**ThreadGrabThreadLock()**. Pass the handle of the thread lock as returned 
by **ThreadAllocThreadLock()**.

Do not try to release a thread lock that has not previously been grabbed. The 
results are unpredictable.

**Include:** sem.h

----------
#### ThreadVSem()
    void    ThreadVSem(
            SemaphoreHandle sem);               /* semaphore to release */

This routine releases a semaphore that was grabbed with ThreadPSem() or 
**ThreadPTimedSem()**. Pass the handle of the semaphore as returned by 
**ThreadAllocSem()**.

Do not try to release a semaphore that has not previously been grabbed with 
one of the above routines. The results are unpredictable.

**Include:** sem.h

----------
#### TimerGetCount()
    dword   TimerGetCount();

This routine returns the value of the system counter. The returned value is 
the number of ticks since GEOS started.

**Include:** timer.h

----------
#### TimerGetDateAndTime()
    void    TimerGetDateAndTime(
            TimerDateAndTime * dateAndTime);    /* buffer for returned values */

This routine returns the current time and date. Pass it a pointer to an empty 
**TimerDateAndTime** structure to be filled in by the routine.

**Include:** timedate.h

----------
#### TimerSetDateAndTime()
    void    TimerSetDateAndTime(
            word                    flags,          /* which item to set */
            const TimerDateAndTime  * dateAndTime); /* new values */

This routine sets the current date and/or time of the system. Pass it the 
following:

*flags* - A word of flags. Pass TIME_SET_DATE to set the day, month, 
and year; pass TIME_SET_TIME to set the hour, minute, and 
second. Pass both to set both.

*dateAndTime* - A pointer to a **TimerDateAndTime** structure containing the 
information to be set.

**Include:** timedate.h

----------
#### TimerSleep()
    void    TimerSleep(
            word    ticks);     /* number of ticks the thread should sleep */

This routine invokes a "sleep timer" that will put the calling thread to sleep 
for the given number of ticks. At the end of the time, the thread will continue 
executing with the next instruction.

**Warnings:** Do not use sleep timers as a substitute for semaphores for thread 
synchronization.

**Include:** timer.h

----------
#### TimerStart()
    TimerHandle TimerStart(
            TimerType   timerType,      /* type of timer to start */
            optr        destObject,     /* object to receive notification
                                         * message when timer expires */
            word        ticks,          /* amount of time to run */
            Message     msg,            /* notification message */
            word        interval,       /* interval for continual timers */
            word        * id);          /* buffer for returned timer ID */

This routine starts a timer of any type. The timer will run for the specified 
number of ticks and then will send the given message to the destination 
object. The message is sent with the flags MF_FORCE_QUEUE, 
MF_CHECK_DUPLICATE and MF_REPLACE, so it will always be put in the 
recipient's queue and will always replace any duplicates already in the 
queue. Pass this routine the following:

*timerType* - A value of **TimerType** indicating the type of timer to start.

*destObject* - The optr of the object that will be sent the specified message 
when the time is up.

*ticks* - The number of ticks for the timer to run. (Sixty ticks equals one 
second.) 

*msg* - The message to be sent to the destination object when time is 
up.

*interval* - For continual timers, the interval (number of ticks) at which to 
send out the message to the destination object. The timer will 
send the message once at the end of each interval. The first 
message will be sent *ticks* ticks after the timer is started. The 
second message will be sent *interval* ticks after that.

*id* - A pointer to a word in which the timer's ID will be returned. 
You will need this ID for **TimerStop()**.

This routine returns the handle of the timer as well as an ID pointed to by 
the *id* parameter. You will need the handle and the ID for **TimerStop()**.

**TimerType:** The **TimerType** enumerated type defines what type of timer should be 
initiated. It has the following values:

TIMER_ROUTINE_ONE_SHOT  
Start a timer that will call a routine and then free itself when 
the time is expired. This type is supported in assembly but not 
in C.

TIMER_ROUTINE_CONTINUAL  
Start a timer that will call a routine once per time interval 
until **TimerStop()** is called. This type is supported in 
assembly but not in C.

TIMER_EVENT_ONE_SHOT  
Start a timer that will send a message to a given object, then 
free itself, when time is expired.

TIMER_EVENT_CONTINUAL  
Start a timer that will send a message to a given object once per 
time interval until **TimerStop()** is called.

TIMER_MS_ROUTINE_ONE_SHOT  
Start a timer that has millisecond accuracy. For this timer, the 
number of ticks will actually be the number of milliseconds. 
The timer will call a specified routine and then free itself when 
time is expired. This type is supported in assembly but not in C.

TIMER_EVENT_REAL_TIME  
Start a timer that will call a routine at some particular date 
and time. On devices that support such a timer, this event will 
wake a sleeping machine.

**Include:** timer.h

----------
#### TimerStop()
    Boolean TimerStop(
            TimerHandle     th,     /* handle of timer to be stopped */
            word            id);    /* timer ID (returned by TimerStart() */

This routine stops a timer that had been started with **TimerStart()**. Pass it 
the timer handle and the ID as returned by that routine (the ID of a continual 
timer will always be zero).

The returned error flag will be *true* if the timer could not be found.

**Warnings:** If you call **TimerStop()** to stop a continual timer that sends its message 
across threads, there may be timer events left in the recipient's event queue. 
It is unsafe in this situation to assume that all timer events have been 
handled. To ensure the timer message has been handled, you can send the 
destination an "all-safe" message with the MF_FORCE_QUEUE flag.

**Include:** timer.h

----------
#### TocDBLock()
    void * TocDBLock(
            DBGroupAndItem      thing);

Use this routine to lock a name array maintained by a PrefTocList object.

**Include:** config.goh

----------
#### TocDBLockGetRef()
    void * TocDBLockGetRef(
            DBGroupAndItem      thing,
            optr                *refPtr);

This routine locks a name array maintained by a PrefTocList object, 
returning the item's pointer and optr.

**Include:** config.goh

----------
#### TocFindCategory()
    Boolean TocFindCategory(
            TocCategoryStruct       *cat);

This routine searches a PrefTocList object's name list for a given token.

**Structures**: 

    typedef struct {
        TokenChars      TCS_tokenChars;
        DBGroupAndItem  TCS_files;          /* file name array */
        DBGroupAndItem  TCS_devices;        /* device name array--only if
                                             * TCF_EXTENDED_DEVICE_DRIVERS
                                             * is set. */
    } TocCategoryStruct;

**Include:** config.goh

----------
#### TocGetFileHandle()
    word TocGetFileHandle();

Use this routine to get the handle of the file used by PrefTocLists to store 
their name array data.

**Include:** config.goh

----------
#### TocNameArrayAdd()
    word TocNameArrayAdd(
            DBGroupAndItem      array, 
            const char          *nameToFind,
            const void          *data);

Use this routine to add a name to a name array maintained by a PrefTocList 
object.

**Include:** config.h

----------
#### TocNameArrayFind()
    word TocNameArrayGetElement(
            DBGroupAndItem      array, 
            word                element,
            void                *buffer);

Use this routine to find a name in the name list maintained by a PrefTocList 
object.

**Include:** config.goh

----------
#### TocNameArrayGetElement()
    word TocNameArrayGetElement(
            DBGroupAndItem      array, 
            word        element,
            void        *buffer);

Use this routine to retrieve a given element from a name array maintained 
by a PrefTocList object. 

**Include:** config.goh

----------
#### TocSortedNameArrayAdd()
    word TocSortedNameArrayAdd(
            word                arr, 
            const char          *nameToAdd,
            NameArrayAddFlags   flags,
            const void          *data);

This routine adds a name to a sorted name array associated with a 
PrefTocList object. 

**Structures:** 

    typedef WordFlags NameArrayAddFlags;
    #define NAAF_SET_DATA_ON_REPLACE 0x8000

**Include:** config.goh

----------
#### TocSortedNameArrayFind()
    Boolean TocSortedNameArrayFind(
            word                        arr, 
            const char                  *nameToFind,
            SortedNameArrayFindFlags    flags,
            void                        *buffer, 
            word                        *elementNum);

This routine looks up a name in a sorted name array associated with a 
PrefTocList object.

**Structures:** 

    typedef WordFlags SortedNameArrayFindFlags;
    #define SNAFF_IGNORE_CASE 0x0080

**Include:** config.goh

----------
#### TocUpdateCategory()
    void TocUpdateCategory(
            TocUpdateCategoryParams *params);

Use this routine to update a PrefTocList object based upon the files in a given 
directory with a given token.

**Structures:** 

    typedef struct {
        TocUpdateCategoryFlags  TUCP_flags;
        TokenChars              TUCP_tokenChars;
        byte                    TUCP_fileArrayElementSize;
        TocUpdateAddCallback    *TUCP_addCallback;
        byte                    TUCP_pad; /* Wants to be word-aligned */
    } TocUpdateCategoryParams;

    typedef word _pascal TocUpdateAddCallback(
        const char *filename,
        optr chunkArray);
    /* Return 0 if add aborted, else return offset of new element within
     * block */

**Include:** config.goh

----------
#### TOKEN_CHARS()
    dword   TOKEN_CHARS(a, b, c, d);
            char    a, b, c, d;

This macro creates a single dword value from four given characters. This is 
useful when creating a token characters value for a specific token.

----------
#### TokenDefineToken()
    word    TokenDefineToken(
            dword           tokenChars,         /* four token characters */
            ManufacturerID  manufacturerID,     /* manufacturer ID for token */
            optr            monikerList,        /* optr of moniker list */
            TokenFlags      flags);             /* token flags */

This routine adds a new token and moniker list to the token database. If the 
token already exists in the token DB, the old will be replaced with the new. 
This routine must only be called by a thread that can lock the block in which 
the passed moniker or moniker list resides. This routine must be passed the 
following parameters:

*tokenChars* - The four token characters that identify this moniker or 
moniker list in the token database. Create this dword value 
from the four characters with the macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*monikerList* - The optr of the moniker list to be added to the token database.

*flags* - A record of **TokenFlags** indicating the relocation status of the 
moniker list.

**Warnings:** This routine may legally move locked LMem blocks (token database items), 
thereby invalidating all pointers to token database items.

**Include:** token.h

----------
#### TokenGetTokenInfo()
    Boolean TokenGetTokenInfo(
            dword           tokenChars,         /* four characters of token */
            ManufacturerID  manufacturerID,     /* manufacturer ID of token */
            TokenFlags      * flags);           /* returned token flags */

This routine returns information about a specified token. Pass it the 
following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*flags* - A pointer to an empty flags record; the flags set (if any) for the 
specified token (if it exists) will be returned here.

This routine returns a (non-zero) value of **VMStatus** if the token was not 
found in the token database. It returns zero if successful.

**Include:** token.h

----------
#### TokenListTokens()
    dword   TokenListTokens(
            TokenRangeFlags     tokenRangeFlags,
            word                headerSize,
            ManufacturerID      manufacturerID));

This routine lists all the tokens in the token database. It allocates a new 
block on the global heap and writes in it an array of **GeodeToken** structures. 
This routine returns the actual tokens, not the token groups.

The returned dword consists of two values: The high word represents the 
number of tokens in the returned block and may be extracted with the macro 
**TokenListTokensCountFromDWord()**. The low word represents the 
handle of the newly-allocated block and can be extracted with the macro 
**TokenListTokensHandleFromDWord()**.

**Include:** token.h

----------
#### TokenListTokensCountFromDWord()
    word    TokenListTokensCountFromDWord(d);
            dword   d;

This macro extracts the number of tokens from the value returned by 
**TokenListTokens()**.

----------
#### TokenListTokensHandleFromDWord()
    word    TokenListTokensHandleFromDWord(d);
            dword   d;

This routine extracts the MemHandle from the value returned by 
**TokenListTokens()**.

----------
#### TokenLoadMonikerBlock()
    Boolean TokenLoadMonikerBlock(
            dword                   tokenChars,     /* four characters of token */
            ManufacturerID          manufacturerID, /* manufacturer ID of token */
            DisplayType             displayType,    /* type of display for token */
            VisMonikerSearchFlags   searchFlags,    /* flags for finding token */
            word                    * blockSize,    /* returned block size */
            MemHandle               * blockHandle); /* returned block handle */

This routine loads a specified token's moniker, allocating a new global 
memory block for the moniker. The returned Boolean will be *true* if the 
moniker was found, *false* otherwise. Information about the moniker is 
returned in the values pointed to by *blockSize* (the size of the newly allocated 
block) and *blockHandle* (the handle of the new block). If the moniker is not 
found, both return pointers will be NULL and no block will be allocated.

Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*displayType* - A value of **DisplayType** indicating the size of the display (used 
to indicate small-screen devices, primarily).

*searchFlags* - A record of **VisMonikerSearchFlags** indicating what type of 
moniker is being requested.

*blockSize* - A pointer to a word in which the new block's size will be 
returned.

*blockHandle* - A pointer to a handle in which the new block's handle will be 
returned.

**Include:** token.h

----------
#### TokenLoadMonikerBuffer()
    Boolean TokenLoadMonikerBuffer(
            dword                   tokenChars,         /* four characters of token */
            ManufacturerID          manufacturerID,     /* manufacturer ID of token */
            DisplayType             displayType,        /* type of display for token */
            VisMonikerSearchFlags   searchFlags,        /* flags for finding token */
            void                    * buffer,           /* pointer to buffer for token */
            word                    bufSize,            /* size of passed buffer */
            word                    * bytesReturned);   /* number of bytes returned */

This routine loads a specified token's moniker into a provided buffer. The 
returned Boolean will be *true* if the moniker was found, *false* otherwise. The size 
of the returned moniker will be returned in the word pointed to by the 
*bytesReturned* parameter.

Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*displayType* - A value of **DisplayType** indicating the size of the display (used 
to indicate small-screen devices, primarily).

*searchFlags* - A record of **VisMonikerSearchFlags** indicating what type of 
moniker is being requested.

*buffer* - A pointer to a locked or fixed buffer into which the moniker will 
be copied.

*bufSize* - The size of the passed buffer; also the maximum size of the 
moniker that may be returned.

*bytesReturned* - The size of the moniker actually returned in the buffer.

**Include:** token.h

----------
#### TokenLoadMonikerChunk()
    Boolean TokenLoadMonikerChunk(
            dword                   tokenChars,     /* four characters of token */
            ManufacturerID          manufacturerID, /* manufacturer ID of token */
            DisplayType             displayType,    /* type of display for token */
            VisMonikerSearchFlags   searchFlags,    /* flags for finding token */
            MemHandle               lmemBlock,      /* locked block for new chunk */
            word                    * chunkSize,    /* returned new chunk size */
            ChunkHandle             * chunkHandle); /* returned new chunk handle */

This routine loads a specified token's moniker, allocating a new chunk in a 
local memory block for the moniker. The returned Boolean will be *true* if the 
moniker was found, *false* otherwise.

Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*displayType* - A value of **DisplayType** indicating the size of the display (used 
to indicate small-screen devices, primarily).

*searchFlags* - A record of **VisMonikerSearchFlags** indicating what type of 
moniker is being requested.

*lmemBlock* - The MemHandle of the local memory block in which the new 
chunk will be allocated. If the block is locked, you must 
dereference the global handle after calling this routine.

*chunkSize* - A pointer to a word in which the size of the allocated chunk will 
be returned.

*chunkhandle* - A pointer to a chunk handle in which the handle of the newly 
allocated chunk will be returned.

**Warnings:** This routine can move chunks in the passed block, thereby invalidating 
pointers to any chunk in the block.

**Include:** token.h

----------
#### TokenLoadTokenBlock()
    Boolean TokenLoadTokenBlock(
            dword           tokenChars,         /* four characters of token */
            ManufacturerID  manufacturerID,     /* manufacturer ID of token */
            word            * blockSize,        /* returned size of new block */
            MemHandle       * blockHandle);     /* returned handle of block */

This routine loads the specified token's **TokenEntry** structure into a 
newly-allocated global memory block. The returned Boolean will be *true* if the 
token was found, *false* otherwise.

Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*blockSize* - A pointer to a word in which the size of the newly-allocated 
block will be returned.

*blockHandle* - A pointer to a global handle in which the handle of the 
newly-allocated block will be returned.

**Include:** token.h

----------
#### TokenLoadTokenBuffer()
    Boolean TokenLoadTokenBuffer(
            dword           tokenChars,         /* four characters of token */
            ManufacturerID  manufacturerID,     /* manufacturer ID of token */
            TokenEntry      * buffer);          /* buffer for returned token */

This routine loads the specified token's **TokenEntry** structure into a passed 
buffer. The returned Boolean will be *true* if the token was found, *false* 
otherwise. Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*buffer* - A pointer to a locked or fixed buffer into which the token entry 
will be copied.

**Include:** token.h

----------
#### TokenLoadTokenChunk()
    Boolean TokenLoadTokenChunk(
            dword           tokenChars,         /* four characters of token */
            ManufacturerID  manufacturerID,     /* manufacturer ID of token */
            MemHandle       lmemBlock,          /* handle of block for chunk */
            word            * chunkSize,        /* returned size of new chunk */
            ChunkHandle     * chunkHandle);     /* returned chunk handle */

This routine loads the specified token's **TokenEntry** structure into a 
newly-allocated chunk. The returned Boolean will be *true* if the token was found, 
*false* otherwise.

Pass this routine the following:

*tokenChars* - The four token characters that identify the token database 
entry. Create this dword from the four characters with the 
macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*lmemBlock* - The MemHandle of the local memory block in which the new 
chunk will be allocated. If the block is locked, you must 
manually dereference this handle after the routine call.

*chunksize* - A pointer to a word in which the size of the newly-allocated 
chunk will be returned.

*chunkHandle* - A pointer to a chunk handle in which the handle of the 
newly-allocated chunk will be returned.

**Warnings:** This routine can move chunks in the passed block, thereby invalidating 
pointers to any chunk in the block.

**Include:** token.h

----------
#### TokenLockTokenMoniker()
    void    * TokenLockTokenMoniker(
            TokenMonikerInfo    tokenMonikerInfo);  /* The DB group and item numbers
                                 * as returned by TokenLookupMoniker() */

This routine locks a token's moniker so it may be drawn; it returns a pointer 
to the locked chunk containing the moniker information. Pass it the 
structure returned by **TokenLookupMoniker()**.

**Be Sure To:** Unlock the moniker with **TokenUnlockMoniker()** after you have finished 
drawing it.

**Include:** token.h

----------
#### TokenLookupMoniker()
    Boolean TokenLookupMoniker(
            dword                   tokenChars,         /* four characters of token */
            ManufacturerID          manufacturerID,     /* manufacturer ID of token */
            DisplayType             displayType,        /* display type of token */
            VisMonikerSearchFlags   searchFlags,        /* flags for finding token */
            TokenMonikerInfo *      tokenMonikerInfo);  /* DB group and item of token */

This routine finds and retrieves a pointer to the specific moniker for the 
specified token, given also the token's display type and other attributes. Pass 
the following:

*tokenChars* - The four token characters that identify this moniker or 
moniker list in the token database. Create this dword value 
from the four characters with the macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

*displayType* - A value of **DisplayType** indicating the size of the display (used 
to indicate small-screen devices, primarily).

*searchFlags* - A record of **VisMonikerSearchFlags** indicating what type of 
moniker is being requested.

*tokenDBItem* A pointer to an empty **TokenMonikerInfo** structure, in which 
the token's group and item numbers will be returned.

The return value is an error flag: it will be *true* if the item could not be found 
in the token database, *false* otherwise.

**Include:** token.h

----------
#### TokenCloseLocalTokenDB()
    void    TokenCloseLocalTokenDB()

This routine closes the local token database.

----------
#### TokenListTokens()
    dword TokenListTokens(
            TokenRangeFlags     tokenRangeFlags,
            word                headerSize,
            ManufacturerID      manufacturerID)

This routine locates all the tokens that meet specified criteria, allocates a 
block, and copies the tokens to that block. The upper word of the return value 
is the number of matching tokens found; the lower word is the handle of the 
block which was allocated.

----------
#### TokenOpenLocalTokenDB()
    word    TokenOpenLocalTokenDB()

This routine opens the local token database. It returns zero on success, and 
a **VMStatus** error code on failure.

**Include:** token.h

----------
#### TokenRemoveToken
    Boolean TokenRemoveToken(
            dword           tokenChars,         /* four characters of token */
            ManufacturerID  manufacturerID,     /* manufacturer ID of token */

This routine removes the specified token and its moniker list from the token 
database. It returns an error flag: if the token could not be found, the 
returned flag is *true*; otherwise it is *false*. Pass the following:

*tokenChars* - The four token characters that identify this moniker or 
moniker list in the token database. Create this dword value 
from the four characters with the macro TOKEN_CHARS.

*manufacturerID* - The manufacturer ID number of the manufacturer responsible 
for the token database entry.

**Include:** token.h

----------
#### TokenUnlockTokenMoniker()
    void    TokenUnlockTokenMoniker(
            void * moniker);

This routine unlocks a moniker that had been locked with 
**TokenLockMoniker()**. Pass a pointer to the locked moniker, as returned by 
the locking routine.

**Include:** token.h

----------
#### TypeFromFormatID()
    word    TypeFromFormatID(id);
            ClipboardItemFormatID   id;

This macro extracts the word-sized format ID (of type 
**ClipboardItemFormat**) from a **ClipboardFormatID** argument.

[Routines M-P](rroutm_p.md) <-- [Table of Contents](../routines.md) &nbsp;&nbsp; --> [Routines U-Z](rroutu_z.md)