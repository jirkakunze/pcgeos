# 16 Impex Library
Most applications which write data files should be compatible with other 
applications. For example, applications will want to be able to import data, 
i.e. open a file written by another application and translate its data into a 
format the application can use. However, writing code to open another 
application's file and translate it into your own application's format can be 
difficult. Any application that wanted to properly support file-importing 
would have to provide a great many of these utilities. Furthermore, there 
would be a lot of duplicated effort; one application's routine for importing a 
particular file format would be much like another's. Applications would have 
similar problems *exporting* data, i.e. writing their data in another 
application's format.

The Impex (Import/Export) Library, along with specific translation libraries, 
solves these problems. They automatically translate files from non-GEOS 
formats into the *Metafile* (i.e. Clipboard) format and back. Applications just 
need to include the Import and Export objects; once they do this, importing 
and exporting data is just like using the Clipboard. Furthermore, extending 
the Impex Library to handle new formats is easy. All you have to do is write 
a new translation library for that one format and put the library in the 
SP_IMPORT_EXPORT_DRIVERS standard path. All applications which use 
the Impex objects will then be able to use that new format. Developers can 
write their own translation libraries; however, few will need to do this, since 
GEOS provides libraries for the most popular formats.

The Impex library currently supports import and export of text and graphic 
files. Geoworks is planning on extending it to allow the import and export of 
spreadsheet and font files as well. When these capabilities are available, they 
will be just like the already-supported text and graphic import and export.

The Impex Library works very much like the Clipboard. For this reason, you 
should be familiar with the Clipboard (see "The Clipboard," Chapter 7 of the 
Concepts Book). You should also be familiar with VM chains (described in 
"Virtual Memory," Chapter 18 of the Concepts Book).

## 16.1 Impex Basics
The Impex library provides a uniform way of importing data from files 
written by non-GEOS applications, and of exporting data to files written in 
these formats. It also saves coding time. Every outside format needs a single 
translation library, which translates between the Metafile format and the 
format for that specific library. The Impex objects provide all the necessary 
user interface (though the libraries and applications can add more), and also 
do all necessary interaction with the translation libraries.

In order to import data, an application will need to have an ImportControl 
object. Similarly, in order to export data, an application will need an 
ExportControl. Most applications that produce data files will want to use 
both of these objects.

Some applications will create a new GEOS file for imported data; others will 
add it into the current document, as if they were pasting data from the 
clipboard. Similarly, some applications will export the entire current 
document; others will export only the current selection, if any. This is left 
entirely to the discretion of the application.

### 16.1.1 The Impex Objects
There are two Impex objects which applications need to know about: 
ImportControl and ExportControl. These objects are both subclassed off of 
**ImportExportClass**, which provides some functionality needed for both 
objects; however, all this functionality is internal, so you can ignore this 
class. (No one should ever create an ImportExport object, and it has no 
instance data which applications may set.) **ImportExportClass** is itself 
subclassed from **GenControlClass**; therefore, ImportControl and 
ExportControl objects inherit all the functionality of controllers.

Applications will generally have one ImportControl and one ExportControl 
object. Applications may put these in different places; for example, some will 
place both of these on the File menu, while others may put them in the 
"Open" or "Save As" dialog boxes. Applications should decide this based on 
how they think the user will be using the import and export capabilities.

The ImportControl and ExportControl objects communicate with the 
application by sending messages. The ImportControl object sends a message 
to its recipient near the end of the Import process, when it has translated a 
file into a VM chain for the application to copy. The ExportControl object 
sends a message near the beginning of the export process, instructing the 
application to create a VM chain for the application to export. In both cases 
the message sent and the recipient are set by the application. Most 
applications will have the messages sent to their process objects; 
however, applications which use the object model of document control may 
want to have the messages sent to the target object. 

### 16.1.2 How the Impex Objects Work
The Impex objects manage the user interface for importing and exporting 
data. They also open and create the appropriate files when necessary. If a 
user wants to import a file, the Import object lets the user select a source file 
and format. The import object will then call an appropriate library to 
translate the source file into a VM chain in the clipboard (i.e. MetaFile) 
format. At this point, it sends a message to the application, passing the 
handle of the VM chain. Similarly, when the user wants to export data, the 
Export object first lets the user select a name and format for the destination 
file. The Export object then sends a message to the application requesting a 
source VM chain in the Metafile format. The Export object then calls the 
appropriate translation library, which translates the MetaFile into a 
corresponding DOS file in the specified format.

It may help to give a blow-by-blow example. Let's suppose someone is using 
FooPaint, a graphics program for GEOS. FooPaint uses both the 
ImportControl and ExportControl objects to let the user import and export 
graphics. The user wants to import a graphic file written by BazDraw, a 
non-GEOS application. The user activates the "Import" trigger. The following 
things happen:

1. The ImportControl checks what translation libraries are available. This 
tells it what formats can be imported.

2. The ImportControl presents a dialog box to the user. This box contains a 
list of available formats. It also contains a file selector, which the user 
uses to select a file of the appropriate format. Assume the user chooses 
the BazDraw format, and the file FISH.BAZ.

3. The ImportControl opens FISH.BAZ and a temporary VM file. It then 
starts up the BazDraw-to-Metafile library. It then sends a message to 
this library, passing both files' handles with the message.

4. The library reads FISH.BAZ and writes an equivalent Metafile sequence 
in a VM chain in the temporary file. When it is done, it returns the VM 
chain handle to the ImportControl.

5. The ImportControl sends a message to its destination object (often the 
application's Process object). This is the application's first direct 
involvement in the import. The message includes such information as the 
handles of the VM chain, the type of data being imported (in this case, a 
graphics metafile), and the message to send on completion.

6. The application copies the information from the VM chain. This is almost 
identical to pasting data from the clipboard. FooPaint will probably 
respond by adding the graphic to the target document.

7. When the application is finished copying the data, it calls the 
**ImpexImportExportCompleted()** utility routine. This routine sends 
an appropriate acknowledgment message to the ImportControl object. 
The ImportControl destroys the temporary VM file.

Note that almost all of this takes place without any action by the application. 
The only thing the application has to do is copy data from the VM chain to its 
own file, translating from the Metafile format to its own format. This is 
exactly what the application does whenever it pastes data from the clipboard; 
thus, applications which use the clipboard already have almost everything 
they need to use the Impex objects.

Exporting data is almost the same as importing it. The application is sent a 
message at the beginning of the operation, asking the application to write a 
VM chain in the Metafile format. The ExportControl will already have 
created a temporary VM file for the application to use; the application just 
has to allocate a chain in it and fill it with the appropriate information. The 
ExportControl presents a dialog box to the user, letting the user select a 
name, format, and location for the exported file; it then starts up the 
appropriate translation library to write the file.

## 16.2 Using Impex
The Impex objects are easy to use. Essentially, an application has to declare 
two objects and handle a single message from each of them. Applications 
which can use the clipboard already have most of the code they will need to 
use the Impex objects.

### 16.2.1 Common Impex Concepts
There are certain concepts and structures which are used by both of the 
Impex objects. Applications which use the Impex objects will have to be 
familiar with them.

#### 16.2.1.1 Metafile Formats
    ImpexDataClasses

The Impex objects are designed to translate into a few specific formats. These 
formats are known collectively as the Metafile formats. These formats (except 
for the "font" format) are identical to the standard "Clipboard Item Formats" 
described in the Clipboard chapter (see section 7.2.4 of "The Clipboard," 
Chapter 7 of the Concepts Book). When an application uses the Impex 
objects, it has to specify what types of data it is prepared to import or export. 
It does this by setting an **ImpexDataClasses** record in both the 
ImportControl and the ExportControl objects. **ImpexDataClasses** has the 
following fields:

IDC_TEXT  
The data is in the Metafile Text format. This is identical to the 
Clipboard's CIF_TEXT format. In addition to text, this format 
records information about fonts, spacing, embedded graphics, 
etc.

IDC_GRAPHICS  
The data is in the Metafile Graphics format. This is simply a 
GString in a VM chain. This format is identical to the 
Clipboard's CIF_GRAPHICS_STRING format.

IDC_SPREADSHEET  
The data is in the Metafile Spreadsheet format. This 
corresponds to the Clipboard's CIF_SPREADSHEET format.

IDC_FONT  
The data is in the Metafile Font format. This format is used to 
translate fonts between representations.

Every translation library translates a DOS file into a VM chain in one of these 
four formats, and vice versa. When an application declares an Impex object, 
it specifies what formats should be supported for that application. The user 
will be allowed to choose translation libraries which translate to or from a 
supported Metafile format.

#### 16.2.1.2 ImpexTranslationParams
    ImpexTranslationParams

The Import and Export objects have to pass information to the application 
and the translation libraries. The same sort of information gets passed in 
many situations; for example, the objects often have to pass the 
**VMFileHandle** and **VMBlockHandle** which specify the VM chain. For 
simplicity, the Impex objects just pass a pointer to a 
**ImpexTranslationParams** structure when they send messages to the 
translation libraries or the application. The library or application takes the 
appropriate action, changing the **ImpexTranslationParams** structure as 
necessary; it then sends a response message, which takes a pointer to the 
same **ImpexTranslationParams**. **ImpexTranslationParams** has the 
following structure:

    typedef struct {
        optr                ITP_impexOD;
        Message             ITP_returnMsg;
        ImpexDataClasses    ITP_dataClass;
        VMFileHandle        ITP_transferVMFile;
        VMBlockHandle       ITP_transferVMChain;
        dword               ITP_internal;
        ManufacturerID      ITP_manufacturerID;
        ClipboardFormat     ITP_clipboardFormat;
    } ImpexTranslationParams;

*ITP_impexOD*  
This field holds the optr of whatever Impex object sent the 
message. The response message should be addressed to this 
optr.

*ITP_returnMsg*  
This field holds the message which the library or application 
should send to the Impex object when it is finished. This 
message always takes a single argument, namely a pointer to 
the **ImpexTranslationParams**. The application should just 
pass the pointer to the **ImpexTranslationParams** to the 
routine **ImpexImportExportCompleted()**; this routine will 
send the appropriate notification message.

*ITP_dataClass*  
This is a **ImpexDataClasses** record. Exactly one of the flags 
will be set, indicating what sort of Metafile format is being 
used.

*ITP_transferVMFile*  
This is the **VMFileHandle** of the transfer file. The transfer file 
is automatically created and destroyed by an Impex object.

*ITP_transferVMChain*  
This is the **VMBlockHandle** of the first block in the transfer 
VM chain. In Import operations, the translation library creates 
the VM chain in the transfer file; in Export operations, the 
application creates it. The chain will be freed when the Impex 
object destroys the transfer VM file. For information about 
creating VM chains, see section 18.4 of "Virtual Memory," 
Chapter 18 of the Concepts Book.

*ITP_internal*  
This field is for internal use by the Impex objects. You should 
not change it.

*ITP_manufacturerID*  
This field contains the manufacturer ID which should be used 
for the Metafile data; see "The Clipboard," Chapter 7 of the 
Concepts Book.

*ITP_clipboardFormat*  
This field specifies what format should be used for the Metafile 
data; see "The Clipboard," Chapter 7 of the Concepts Book.

The meaning of each field can change, depending on the circumstances. For 
example, when the ImportControl sends its message to the application, the 
*ITP_transferVMChain* field will contain the handle of a VM chain containing 
the imported data. In contrast, when the ExportControl sends its message, 
*ITP_transferVMChain* contains a null handle; the application should allocate 
a VM chain, fill it with data, and write the **VMBlockHandle** of the chain to 
this field. When a field has a special meaning, the documentation will explain 
it.

### 16.2.2 The ImportControl Object
Applications which use the Impex library will generally have a single 
ImportControl object. This object is usually a child of the File menu; however, 
applications may put it wherever they want. They should also place it on the 
application object's GAGCNLT_SELF_LOAD_OPTIONS General Change 
Notification list. Applications should disable this object (with 
MSG_GEN_SET_NOT_ENABLED) whenever they are not prepared to accept 
imported data; for example, some applications will choose to disable file 
importing whenever they are unable to open a new document.

The ImportControl object is subclassed from **GenControlClass** (by way of 
**ImportExportClass**, as noted above). It thus has all the functionality of 
that class. It also has a few instance data fields of its own; they are shown in 
Code Display 16-1.

----------
**Code Display 16-1 ImportControlClass Instance Data**

    /* ICI_attrs is a word-length record which stores attribute information for the
     * ImportControl object. It has only one flag, ICA_IGNORE_INPUT. */
    @instance ImportControlAttrs    ICI_attrs = 0;

    /* ICI_dataClasses is a word-length record which indicates what Metafile formats
     * are supported by the application. The application must set this field. */
    @instance ImpexDataClasses      ICI_dataClasses = 0;

    /* ICI_destination and ICI_message indicate what message should be sent when the
     * ImportControl object has finished importing a file. The application must set
     * these fields. The message must take a single argument, namely a pointer to an
     * ImpexTranslationParams structure in ss:bp (on the stack). */
    @instance optr                  ICI_destination;
    @instance word                  ICI_message;

    /* Applications may wish to add their own UI objects to the Import dialog box.
     * They can do so by defining a generic tree (the top object of which must be not
     * "usable"), and putting an optr to the top object in ATTR_IMPORT_CONTROL_APP_UI.
     */
    @vardata optr                   ATTR_IMPORT_CONTROL_APP_UI;

    /* Controller features flags */
    typedef ByteFlags           ImportControlFeatures;
    #define IMPORTCF_BASIC      0x01

    typedef ByteFlags               ImportControlToolboxFeatures;
    #define IMPORTCTF_DIALOG_BOX    0x01

----------
#### 16.2.2.1 ICI_attrs
    ImportControlAttrs, MSG_IMPORT_CONTROL_GET_ATTRS, 
    MSG_IMPORT_CONTROL_SET_ATTRS

*ICI_attrs* is a word-length record of type **ImportControlAttrs**. This record 
contains only one flag:

ICA_IGNORE_INPUT  
If this flag is on, the ImportControl will consume all input to 
the application while the import occurs. By default, this flag is 
off.

To find out the current setting of this field, send 
MSG_IMPORT_CONTROL_GET_ATTRS to the ImportControl. To change this 
field, send MSG_IMPORT_CONTROL_SET_ATTRS to the ImportControl.

----------
#### MSG_IMPORT_CONTROL_GET_ATTRS
    ImportControlAttrs   MSG_IMPORT_CONTROL_GET_ATTRS();

This message retrieves the current setting of the ImportControl's *ICI_attrs* 
field.

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Return:** The ImportControl's ICI_attrs field.

**Interception:** This message should not be intercepted.

----------
#### MSG_IMPORT_CONTROL_SET_ATTRS
    void    MSG_IMPORT_CONTROL_SET_ATTRS(
            ImportControlAttrs      attrs);

This message changes the current settings of an ImportControl's *ICI_attrs* 
field. 

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Parameters:**  
*attrs* - The new settings for the ICI_attrs field.

**Interception:** This message should not be intercepted.

#### 16.2.2.2 ICI_dataClasses
    MSG_IMPORT_CONTROL_GET_DATA_CLASSES, 
    MSG_IMPORT_CONTROL_SET_DATA_CLASSES

When you declare an Import object, you must specify what kind of Metafiles 
your application is prepared to accept. You do this by setting the value of the 
*ICI_dataClasses* field. This field is a word-length record of type 
**ImpexDataClasses** (described in section 16.2.1.2 above). If (for 
example) only the IDC_TEXT bit is set, the ImportControl will use only those 
import libraries which produce text Metafile output. More than one bit may 
be set; when the ImportControl sends its notification, it will tell the 
application what type of data is being imported.

To find out the current settings of the *ICI_dataClasses* field, send 
MSG_IMPORT_CONTROL_GET_DATA_CLASSES. To change the settings of 
this field, send MSG_IMPORT_CONTROL_SET_DATA_CLASSES.

----------
#### MSG_IMPORT_CONTROL_GET_DATA_CLASSES
    ImpexDataClasses     MSG_IMPORT_CONTROL_GET_DATA_CLASSES();

This message retrieves the current setting of the ImportControl's 
*ICI_dataClasses* field. This tells you what kind of data can be imported.

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Return:** The ImportControl's *ICI_dataClasses* field.

**Interception:** This message should not be intercepted.

----------
#### MSG_IMPORT_CONTROL_SET_DATA_CLASSES
    void    MSG_IMPORT_CONTROL_SET_DATA_CLASSES(
            ImpexDataClasses        dataClass);

This message changes the current settings of an ImportControl's 
*ICI_dataClasses* field. 

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Parameters:**  
*dataClass* - The new settings for the *ICI_dataClasses* field.

**Interception:** This message should not be intercepted.

#### 16.2.2.3 The ImportControl Action
    MSG_IMPORT_CONTROL_GET_ACTION, 
    MSG_IMPORT_CONTROL_SET_ACTION, 
    ImpexImportExportCompleted()

The ImportControl does most of its work transparently to the application. It 
interacts with the rest of the application only when the user has selected a 
file to import and the appropriate translation library has produced a VM 
chain. At this point the ImportControl sends a notification message to the 
application. The application responds by copying the data from the VM chain 
and sending back an acknowledgment message. The ImportControl can then 
destroy the temporary VM transfer file.

The application determines what message will be sent, and to what object, by 
setting the *ICI_destination* and *ICI_message* fields. Whatever object will 
receive the message should define an appropriate message. The 
ImportControl will send this message with a single parameter: *itp*, a pointer 
to an **ImpexTranslationParams** structure (see section 16.2.1.2 above). The fields of the structure have the following meanings in this case:

*ITP_impexOD*  
The object to which the application should send its 
acknowledgment message. In this case, it is the optr of the 
ImportControl.

*ITP_returnMsg*  
The acknowledgment message to send when the import has 
been completed. In this case, it is 
MSG_IMPORT_CONTROL_IMPORT_COMPLETE.

*ITP_dataClass*  
An **ImpexDataClasses** record with one flag set. This flag 
indicates what type of Metafile has been prepared.

*ITP_transferVMFile*  
The **VMFileHandle** of the temporary transfer file.

*ITP_transferVMChain*  
The **VMBlockHandle** of the lead block in the VM chain 
containing the imported data.

*ITP_internal*  
For use by the ImportControl and should not be changed by the 
application.

The recipient of the message should take any appropriate action; usually this 
entails copying the data from the VM chain, as if it were pasting data from 
the Clipboard. When the application is finished, it should call 
**ImpexImportExportCompleted()**. This routine takes one parameter, 
namely the *itp* pointer which was passed to the object. (The 
**ImpexTranslationParams** structure should not have been changed.) 
**ImpexImportExportCompleted()** reads the appropriate message and 
destination from the **ImpexTranslationParams** and sends the proper 
acknowledgment message (which in this case is 
MSG_IMPORT_CONTROL_IMPORT_COMPLETE).

Applications which use the object model of document control will often set 
*ICI_destination* to TO_APP_TARGET; this will make it send its messages to 
the target object. The application can find out the ImportControl's action by 
sending it MSG_IMPORT_CONTROL_GET_ACTION. The application can 
change the ImportControl's action by sending it 
MSG_IMPORT_CONTROL_SET_ACTION. 

----------
#### MSG_IMPORT_CONTROL_GET_ACTION
    void    MSG_IMPORT_CONTROL_GET_ACTION(
            ImpexAction *       retValue);

This message retrieves the values of an ImportControl's *ICI_destination* and 
*ICI_message* fields. These fields indicate what action the ImportControl will 
take when it is finished preparing a file for import.

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Parameters:**  
*retValue* - A pointer to an **ImpexAction** structure.

**Return:** The value of *ICI_message* (i.e. the message sent by the ImportControl).

*recipient* - A pointer to an **ImpexAction** structure describing 
the message sent.

**Interception:** This message should not be intercepted.

**Structures:** The message and recipient are written to an **ImpexAction** structure:

    typedef struct {
        word    message;    /* message sent */
        word    unused;
        optr    destOD;     /* Destination of message */
    } ImpexAction;

----------
#### MSG_IMPORT_CONTROL_SET_ACTION
    void    MSG_IMPORT_CONTROL_SET_ACTION(
            optr    destOD,     /* Send messages to this object. */
            word    ICImsg);    /* Send this message to the above recipient. */

This message changes the values of an ImportControl's *ICI_destination* and 
*ICI_message* fields. These fields indicate what action the ImportControl will 
take when it is finished preparing a file for import.

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Parameters:**  
*destOD* - Set ICI_destination to this value. 

**ICImsg** - Set ICI_message to this value.

**Interception:** This message should not be intercepted.

#### 16.2.2.4 Adding to the Import Dialog Box
When the user selects the "Import" trigger or tool, the Import controller 
brings up a dialog box. The application can, if it wishes, add UI objects to this 
box. It does so by defining a tree of generic objects (the top object of which 
must be set "not usable"). It must place an optr to the top object in the tree 
in ATTR_IMPORT_CONTROL_APP_UI. When the ImportControl builds the 
dialog box, it will add that optr as one of the children in the tree and set it 
"usable".

### 16.2.3 The ExportControl Object
Applications which use the Impex library will generally have a single 
ExportControl object. This object is usually a child of the File menu; however, 
applications may put it wherever they want. They should also place it on the 
application object's GAGCNLT_SELF_LOAD_OPTIONS GCN list. Applications 
should disable this object (with MSG_GEN_SET_NOT_USABLE) whenever 
they are not able to prepare data for export; for example, some applications 
will choose to disable file exporting whenever the "Cut" and "Copy" functions 
are disabled.

The ExportControl object is subclassed from **GenControlClass** (by way of 
**ImportExportClass**, as noted above). It thus has all the functionality of 
that class. It also has a few instance data fields of its own; they are shown in 
Code Display 16-2.

----------
**Code Display 16-2 ExportControlClass Instance Data**

    /* ECI_attrs is a word-length record which stores attribute information for the
     * ImportControl object. It has only one flag, ECA_IGNORE_INPUT. */
    @instance ExportControlAttrs        ECI_attrs = 0;

    /* ECI_dataClasses is a word-length record which indicates what Metafile formats
     * are supported by the application. The application must set this field. */
    @instance ImpexDataClasses          ECI_dataClasses = 0;

    /* ECI_destination and ECI_message indicate what message should be sent when the
     * ExportControl object is preparing to export a file. The application must set
     * these fields. The message must take a single argument, namely a pointer to an
     * ImpexTranslationParams structure in ss:bp (on the stack). */
    @instance optr                  ECI_destination;
    @instance word                  ECI_message;

    /* Applications may wish to add their own UI objects to the Export dialog box.
     * They can do so by defining a generic tree (the top object of which must be not
     * "usable"), and putting an optr to the top object in ATTR_EXPORT_CONTROL_APP_UI.
     */
    @vardata optr                   ATTR_EXPORT_CONTROL_APP_UI;

    /* Controller features flags */
    typedef ByteFlags           ExportControlFeatures;
    #define EXPORTCF_BASIC          0x01

    typedef ByteFlags           ExportControlToolboxFeatures;
    #define EXPORTCTF_DIALOG_BOX                0x01

----------
#### 16.2.3.1 ECI_attrs
    ExportControlAttrs, MSG_EXPORT_CONTROL_GET_ATTRS, 
    MSG_EXPORT_CONTROL_SET_ATTRS

*ECI_attrs* is a word-length record of type **ExportControlAttrs**. This record 
contains only one flag:

ECA_IGNORE_INPUT  
If this flag is on, the ExportControl will consume all input to 
the application while the import occurs. By default, this flag is 
off.

To find out the current setting of this field, send 
MSG_EXPORT_CONTROL_GET_ATTRS to the ImportControl. To change this 
field, send MSG_EXPORT_CONTROL_SET_ATTRS to the ImportControl.

----------
#### MSG_EXPORT_CONTROL_GET_ATTRS
    ExportControlAttrs   MSG_EXPORT_CONTROL_GET_ATTRS();

This message retrieves the current setting of the ExportControl's *ECI_attrs* 
field.

**Source:** Unrestricted.

**Destination:** Any ExportControl object.

**Return:** The ExportControl's *ECI_attrs* field.

**Interception:** This message should not be intercepted.

----------
#### MSG_EXPORT_CONTROL_SET_ATTRS
    void    MSG_EXPORT_CONTROL_SET_ATTRS(
            ExportControlAttrs      attrs);

This message changes the current settings of an ExportControl's *ECI_attrs* 
field. 

**Source:** Unrestricted.

**Destination:** Any ExportControl object.

**Parameters:**  
*attrs* - The new settings for the ECI_attrs field.

**Interception:** This message should not be intercepted.

#### 16.2.3.2 ECI_dataClasses
    MSG_EXPORT_CONTROL_GET_DATA_CLASSES, 
    MSG_EXPORT_CONTROL_SET_DATA_CLASSES

When you declare an Export object, you must specify what kind of Metafiles 
your application is able to create. You do this by setting the value of the 
*ECI_dataClasses* field. This field is a word-length record of type 
**ImpexDataClasses** (described in section 16.2.1.2 above). If (for 
example) only the IDC_TEXT bit is set, the ExportControl will use only those 
export libraries which expect text Metafile input. More than one bit may be 
set; when the ExportControl sends its notification, it will tell the application 
what type of data it expects to export.

To find out the current settings of the *ECI_dataClasses* field, send 
MSG_EXPORT_CONTROL_GET_DATA_CLASSES. To change the settings of 
this field, send MSG_EXPORT_CONTROL_SET_DATA_CLASSES.

----------
#### MSG_EXPORT_CONTROL_GET_DATA_CLASSES
    ImpexDataClasses     MSG_EXPORT_CONTROL_GET_DATA_CLASSES();

This message retrieves the current setting of the ExportControl's 
*ECI_dataClasses* field. This tells you what kind of data can be exported.

**Source:** Unrestricted.

**Destination:** Any ExportControl object.

**Return:** The ExportControl's *ECI_dataClasses* field.

**Interception:** This message should not be intercepted.

----------
#### MSG_EXPORT_CONTROL_SET_DATA_CLASSES
    void    MSG_EXPORT_CONTROL_SET_DATA_CLASSES(
            ImpexDataClasses        dataClasses);

This message changes the current settings of an ExportControl's 
*ECI_dataClasses* field. 

**Source:** Unrestricted.

**Destination:** Any ExportControl object.

**Parameters:**  
*dataClasses* - The new settings for the ECI_dataClasses field.

**Interception:** This message should not be intercepted.

#### 16.2.3.3 The ExportControl Action
    MSG_EXPORT_CONTROL_GET_ACTION, 
    MSG_EXPORT_CONTROL_SET_ACTION

The ExportControl does most of its work transparently to the application. It 
interacts with the rest of the application after the user selects the name, 
location, and format of the exported file. At this point the ExportControl 
creates a temporary transfer file and sends a notification message to the 
application; the notification message passes the file handle and the format 
expected. The application responds by creating a VM chain in the transfer file 
and filling it with the data to export, formatted in the appropriate Metafile 
format. The ExportControl can then call the translation library to create the 
output file.

The application determines what notification message will be sent, and to 
what object, by setting the *ECI_destination* and *ECI_message* fields. 
Whatever object will receive the message should define an appropriate 
message. The ExportControl will send this message with a single parameter: 
*itp*, a pointer to an **ImpexTranslationParams** structure. The fields of the 
structure have the following meanings in this situation:

*ITP_impexOD*  
The object to which the application should send its 
acknowledgment message. In this case, it is the optr of the 
ExportControl.

*ITP_returnMsg*  
The acknowledgment message to send when the export has 
been completed. In this case, it is 
MSG_EXPORT_CONTROL_EXPORT_COMPLETE.

*ITP_dataClass*  
An **ImpexDataClasses** record with one flag set. This flag 
indicates what type of Metafile should be prepared.

*ITP_transferVMFile*  
The **VMFileHandle** of the temporary transfer file.

*ITP_transferVMChain*  
A null handle. When the application has created the transfer 
VM chain, it should write the **VMBlockHandle** of the head of 
the chain to this field. If the application fails for any reason, it 
should leave this field as a null handle.

*ITP_internal*  
This field is for use by the ExportControl and should not be 
changed by the application.

The recipient of the message should take any appropriate action; usually this 
entails translating the current selection into the Metafile format and writing 
it to a VM chain. When the application is finished, it should call 
**ImpexImportExportCompleted**(). This routine will send the appropriate 
acknowledgment message to the ExportControl object (in this case, 
MSG_EXPORT_CONTROL_EXPORT_COMPLETE). This routine takes one 
parameter, namely the *itp* pointer which was passed to the object. The 
*ITP_transferVMChain* field of the **ImpexTranslationParams** structure 
should be set to the handle of the head block in the VM chain. If the 
application was unable to prepare the data for export, it should clear this 
field.

The ExportControl object will have created a temporary file for the 
application to use. This file will be entirely empty when the application gets 
it. The ExportControl will ignore everything in the file except for the VM 
chain indicated by *ITP_transferVMChain*; thus, an application can feel free 
to allocate blocks in the VM file for scratch space. The ExportControl will 
destroy the file when the translation library has finished preparing the 
output file.

Applications which use the object model of document control will often set 
*ECI_destination* to TO_APP_TARGET; this will make it send its messages to 
the target object. The application can find out the ExportControl's action by 
sending it MSG_EXPORT_CONTROL_GET_ACTION. The application can 
change the ExportControl's action by sending it 
MSG_EXPORT_CONTROL_SET_ACTION. 

----------
#### MSG_EXPORT_CONTROL_GET_ACTION
    void    MSG_EXPORT_CONTROL_GET_ACTION(
            ObjectState *       retValue);

This message retrieves the values of an ExportControl's *ECI_destination* and 
*ECI_message* fields. These fields indicate what action the ExportControl will 
take when it needs to have data prepared for export.

**Source:** Unrestricted.

**Destination:** Any ImportControl object.

**Parameters:**  
*retValue* - A pointer to an **ObjectState** structure.

**Return:** The value of *ICI_message* (i.e. the message sent by the ExportControl).

*recipient* - A pointer to an **ObjectState** structure describing 
the message sent.

**Interception:** This message should not be intercepted.

**Structures:** The message and recipient are written to an **ObjectState** structure:

    typedef struct {
        int notUsed;
        word    message;        /* Message sent */
        optr    destOD;     /* Destination of message */
    } ObjectState;

----------
#### MSG_EXPORT_CONTROL_SET_ACTION
    void    MSG_EXPORT_CONTROL_SET_ACTION(
            optr    destOD,     /* Send messages to this object. */
            word    ECImsg);    /* Send this message to the above recipient. */

This message changes the values of an ExportControl's *ECI_destination* and 
*ECI_message* fields. These fields indicate what action the ExportControl will 
take when it is finished preparing a file for export.

**Source:** Unrestricted.

**Destination:** Any ExportControl object.

**Parameters:**  
*recipient* - Set ECI_destination to this value. 

*message* - Set ECI_message to this value.

**Interception:** This message should not be intercepted.

#### 16.2.3.4 Adding to the Import Dialog Box
When the user selects the "Export" trigger or tool, the Export controller 
brings up a dialog box. The application can, if it wishes, add UI objects to this 
box. It does so by defining a tree of generic objects (the top object of which 
must be set "not usable"). It must place an optr to the top object in the tree 
in ATTR_EXPORT_CONTROL_APP_UI. When the ExportControl builds the 
dialog box, it will add that optr as one of the children in the tree and set it 
"usable".

## 16.3 Writing Translation Libraries
GEOS comes with many translation libraries, and more are being added all 
the time. Geoworks is continually adding new translation libraries for 
popular formats. Nevertheless, we cannot guarantee to support every format. 
Developers may decide to write their own translation libraries. The current 
release of the GEOS SDK does not yet support developers writing their own 
translation libraries; however, we plan to support this by the final release of 
the SDK. This section describes how the libraries work in enough detail that 
developers will be able to do preliminary work in writing the libraries.

Most applications will find the provided translation libraries sufficient for 
their needs. Therefore, most developers can skip this section. You should read 
this if you are planning on writing translation libraries, or just if you want 
more understanding of the importing and exporting process.

Remember: once a translation library has been written, all a user has to do 
to install it is copy it to the appropriate directory. All existing applications 
which use Impex will then automatically be able to import and export that 
format. Users will be able to buy translation library collections from 
third-party vendors the way they buy font collections now.

### 16.3.1 How Translation Libraries Work
A translation library's task is easily stated. It has to do two things: read a 
native-format file and produce a Metafile translation, and read data in a 
Metafile and write corresponding data in a native-format file. How easy this 
is to do depends on the formats involved.

Every translation library specifies what format of Metafile it expects to work 
with. For example, the FooWrite translation library would translate 
FooWrite files into IDC_TEXT Metafiles and vice versa. When a user activates 
the "Import" trigger, he will be presented with a list of formats to use; those 
formats will correspond to all the libraries which can translate files into 
formats the application can accept. For example, if the application specified 
that it could accept text or graphics Metafiles, the user's choice of format 
would depend on which of the installed libraries could generate text or 
graphics Metafiles.

The translation library can also suggest a file mask. For example, the 
FooWrite translation library might specify that FooWrite data files meet the 
pattern "*.FOO". By default, the Import file selector will show only the files 
that match the library's mask. However, the user can override this mask, 
setting a different one or no mask at all.

When a user decides to import a file, the ImportControl opens the source file 
and creates a temporary transfer VM file. The ImportControl then starts up 
the appropriate translation library and passes the two file handles to it. The 
translation library should read the entire source file, translate it to the 
appropriate Metafile format, and write it to a VM chain in the transfer file. It 
then returns the **VMBlockHandle** of the head of the **VMChain** to the 
ImportControl. If it was unable to translate the file, it should return a null 
handle; the ImportControl then displays an appropriate error message. The 
ImportControl will close the source file automatically.

When a user decides to export a file, the ExportControl creates a temporary 
transfer file and opens an empty native-format file for the output. The 
ExportControl then calls the translation library. The library is passed the 
handles of the two files, as well as the handle of the Metafile VM chain. The 
library reads the Metafile and writes an appropriate data file. When it is 
finished, it notifies the ExportControl, which automatically closes the 
destination file and destroys the temporary transfer file.

### 16.3.2 Intermediate Formats
Many libraries will want to make use of intermediate formats. For example, 
a company may have defined its own transfer format for its applications. The 
simplest way for it to translate files into the GEOS Metafile might be to 
translate the file into its own transfer format, then translate from this format 
into the Metafile format. GEOS supports this with its use of intermediate 
translation libraries.

An intermediate library is much like an ordinary translation library. Like 
other libraries, it must translate from its own format into the Metafile 
format. The only difference is that intermediate libraries are not called by the 
Impex objects; instead, they are called by other translation libraries.

For example, suppose FooWare, Inc., has a line of graphic FooApps which 
includes FooDraw, FooPaint, FooSketch, and FooScribble. FooWare has 
developed its own file-transfer format, FooInterchangeFormat (FIF); it has 
code written to translate any FooApp's files into FIF and back. 

FooWare now wants to write translation libraries for GEOS. The first thing 
FooWare does is write an intermediate translation library which takes a DOS 
file containing FIF data and produces a VM chain containing a GEOS graphic 
Metafile, and vice versa. Once this is written, FooWare has an easy time 
writing the actual translation libraries. For example, the FooPaint 
translation library imports files by converting a FooPaint data file into the 
analogous FIF file. Since FooWare already has routines to do this translation, 
it just has to port existing code to the GEOS library. The FooPaint translation 
library then calls the FIF-to-Metafile intermediate translation library and 
gets the finished translation. Similarly, the FooPaint translation library 
exports data by calling the FIF-to-Metafile library to produce a FIF version of 
the data; it can then use ported code to produce an actual FooPaint file.

Note that any ordinary translation library can also be used as an 
intermediate translation library. For example, let's suppose that FooWare 
has code written to translate FIF files into PostScript files. This makes 
translating the documents even easier. The FIF-to-Metafile library can just 
use ported code to produce a PostScript version of the data; it can then call 
the PostScript-to-Metafile translation library, which is provided with GEOS. 
GEOS comes with translation libraries for many popular file-interchange 
formats; thus, many developers will be able to write translation libraries just 
by porting code from their pre-existing translators, then calling one of the 
GEOS translation libraries.

[Help Object Library](ohelp.md) <-- [Table of Contents](../objects.md) &nbsp;&nbsp; --> [The Spool Library](oprint.md)

