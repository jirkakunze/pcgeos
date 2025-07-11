## 2 System Configuration

This chapter details how your software layout should look after installation. This information is provided both so that you can confirm your setup and so that you can make appropriate modifications later.

### 2.1 Development Directories and Files

The installation program will install a large number of tool files, debugging files, and sample applications on to the development machine. These are arranged in the directory of your choice (with name specified in the ROOT_DIR environment variable). Most developers will use \PCGEOS as the root directory of their development tree, and this is the name assumed throughout this section.

**\PCGEOS** This directory is the root of the development tree. It does not contain any files, but all the development files are in subdirectories of this root.

**\PCGEOS\LIBRARY**
This directory houses a number of subdirectories, each of which contains the executable and symbolic information files for a GEOS library. Some of the library directories contain subdirectories. For example, the MATH directory contains a subdirectory COMPILER that contains files for different C compilers. For the most part, you will not need to look in these subdirectories unless you need to re-download the geodes.

**\PCGEOS\DRIVER**
This directory contains a number of subdirectories, each of which corresponds to a single type of driver (e.g. file system driver, printer driver, mouse driver, etc.). Each of these subdirectories contains the executable and symbolic files for the individual GEOS drivers (e.g. the \PCGEOS\DRIVER\MOUSE\MSBUS directory contains the files for the Microsoft bus-type mouse drivers). For the most part, you will not need to look in these subdirectories unless you need to re-download the geodes.

**\PCGEOS\INCLUDE**
This directory contains definition and header files which your programs will include, as well as a few subdirectories. Among these are kernel header files, library header files, and system makefiles. You will probably spend a lot of time looking in these files as well as in the documentation.

The .GOH files are Goc header files. The .MK files are Makefiles, used by the PMake utility. The .DEF files are definition files included by Esp programs. Glue uses the .PLT files to represent software platforms.

The subdirectories of the \PCGEOS\INCLUDE directory are

>LDF This contains the library definition files for all GEOS libraries. When you are ready to install a library, you will create a library definition (.ldf) file which will reside here. You will not access these files directly.

>OBJECTS This contains files associated with GEOS object libraries and GEOS classes. You will probably look here often for calling conventions and features of certain classes.

>ANSI This contains files used for standard ANSI C functions. You may look through these files occasionally.

>INTERNAL This contains files which are normally GEOS-internal. You will only look through these files if you are doing driver 
development.

>SDK_C This directory contains include files meant as sample code.

**\PCGEOS\LOADER** 
This directory contains the executable and symbol files for the GEOS loader. This is the program which loads the GEOS kernel. You will not need to look in this directory unless you need to re-download the loader. Subdirectories contain the Zoomer and PT9000 loaders.

**\PCGEOS\APPL**
This directory contains a number of subdirectories, each of which contains either a single application or a set of subdirectories for a category of application. The sample applications (in the SDK_C subdirectory) include source code but not executable or symbol files. The other applications include only the executable and symbol files.

The subdirectories of the \PCGEOS\APPL directory are

>DUMP   This directory contains the GEOS screen dumper.

>FILEMGRS   This directory contains the various file manager applications.

>GEOMANAG This directory contains the GeoManager application which is the GEOS file manager normally used on desktop machines.

>ZMANAGER This directory contains the Zoomer file manager, suitable for palmtop machines.

>ICON   This directory contains the executable and symbol files for the Icon editor tool. The icon editor is installed on the target machine; you must run it on the target, like other applications. You will not likely need to access this directory.

>PREFEREN   This directory contains the standard applications used for setting user preferences. There are four subdirectories: SETUP, PREFMGR, ZPREFMGR, and ZSETUP. The first two contain the setup and Preferences applications for desktop machines. The second two contain the setup and Preferences applications for the Zoomer. You should not need to access these directories.

>SDK_C  This directory contains the source code for all the GEOS sample applications. You will probably spend a lot of your time browsing, editing, and studying the sample applications. This directory contains a number of subdirectories; each contains either a single application or numerous applications, each in a further subdirectory. The applications included are outlined in "Sample C Applications" on page 23.

>STARTUP    This directory contains the applications and data needed by the system at startup. Specifically, it contains a subdirectory with the Welcome application's executable and symbol files. You will not likely need to access this directory.

**\PCGEOS\BIN** This directory contains the DOS executable tools you will use to construct GEOS applications. (This directory should be appended to your PATH environment variable.) You should not need to access this directory directly.

**\PCGEOS\TCL** This directory contains Tcl (Tool Command Language) scripts. The Swat debugger will use these scripts, which provide functions useful for working with GEOS data structures. You should only need to access this directory if you write your own Tcl code for Swat or if you want to look at or edit the provided Tcl files.

The EXTRA directory in this directory contains additional Tcl scripts that are both undocumented and unsupported. They are provided because you might find them useful even though they are unsupported.

### 2.2 Target Directories and Files

The target machine install will set up a standard GEOS environment. It will set up the proper directory structure; note that this structure is important-the pcs tool, which automatically downloads geodes to their proper directory assumes that those proper directories exist. Also note that the install program will set up two trees: one with error-checking code, and one with normal code. When the instructions tell you to do something from the top-level target directory, then you should be in the top-level EC directory if using error checking code and the top-level normal directory when testing regular code.

Note that the top-level directory of your target install (both top level directories, in fact) will contain a program file SWAT.EXE. This is a program known as the Swat Stub-it will communicate with the actual Swat program, which will run on the development machine.

Throughout the directory tree will be files named @DIRNAME.000. These files contain GEOS-internal information about the directory name and links to other directories and files. These files are created and maintained dynamically by GEOS.

The standard GEOS setup includes the following directories:

**DOCUMENT** Normally used to hold documents. This will most likely be empty after installation.

**PRIVDATA** Normally used to hold data which the user should not work with directly. This directory contains the following subdirectories:

>BACKUP Contains backup files created by the document control object (a standard feature of the document control).

>HWR    Contains handwriting recognition data.

>LOGO   Contains logo picture information.

>PREF   Contains information stored by the Preferences manager.

>SPOOL  Contains files maintained by the spool library (this normally consists of files containing data which will be sent to the printer).

>STATE  Contains the "state" files that running applications leave behind when GEOS shuts down.

>WASTE  Contents of the wastebasket. This directory will be created automatically when you drop a file or directory on the waste basket icon in GeoManager.

**SYSTEM**  Normally contains kernel and library geodes. If you create libraries to support your applications, you will most likely need to install them to this directory. This directory contains the library executables as well as several subdirectories for driver executables. The subdirectories are listed below (files are not listed here):

>FILEMGR    Contains file manager tools. If you create file manager tools, you should install them here.

>FONT   Contains font drivers. Note: Does not contain font definition files; the fonts themselves go in \USERDATA\FONT.

>FS Contains file system drivers.

>IMPEX  Contains import and export translation libraries.

>KBD    Contains keyboard drivers.

>MOUSE  Contains mouse drivers and pen drivers.

>PREF   Contains Preferences modules. If you create a Preferences module, you should install it here.

>PRINTER    Contains printer drivers.

>SAVERS Contains individual screen savers.

>SOUND  Contains sound drivers.

>SWAP   Contains memory swapping drivers.

>SYSAPPL    Contains special system applications such as Welcome and Graphical Setup.

>TASK   Contains task-switch drivers.

>VIDEO  Contains video drivers.

**USERDATA**    Top-level directory normally used to hold data which the user will be allowed to change or add to. This directory contains the following subdirectories:

>DECK   Contains all playing card deck data files.

>FONT   Contains all font data files, regardless of font format.

>HELP   Contains all help files.

**WORLD**   Normally used to hold applications. The World directory has several subdirectories for different types of applications. These subdirectories are listed below:

>ASM    Contains compiled assembly applications downloaded from the host machine's \PCGEOS\APPL\SDK_ASM directory. This will be empty on installation.

>C  Contains compiled C applications downloaded from the host machine's \PCGEOS\APPL\SDK_C directory. After you finish the installation procedures and first chapter of the tutorial, this should contain the HELLO.GEO application.

>Desk Accessories Contains desk accessory applications such as the Calculator. Applications installed in this directory will act as "desk accessories," which will always be on top of other application windows.

>Productivity Contains productivity applications such as GeoWrite and GeoDraw.

>Utilities  Contains utility applications that are not desk accessories. GeoManager, the icon editor, Preferences, and the screen dumper are all examples of utility applications; these should all be installed on your system.

### 2.3 Environment Variables

To make development easier, the environment variables on your 
development machine should have the following additions.

**PATH**    The development kit's \PCGEOS\BIN directory should be appended to your PATH. Your C compiler directory (and its \BIN directory, if appropriate) should also be in your PATH.

**ROOT_DIR**    This variable should contain the directory in which you installed your kit (normally C:\PCGEOS).

**PTTY**    This variable will control communication between the development and target PC. It should be of the format 

    PTTY=comPort,baudRate[,inter]

where comPort is the number of the development machine's serial port you are using, baudRate is a baud rate (e.g. 38400 or 19200), and inter is the number representing the interrupt level at which your serial port is operating. If this is the standard interrupt level for that serial port, this value need not be specified. These fields are separated by commas only; there should be no spaces. A typical PTTY setup is

    PTTY = 2,38400

Your target machine needs only one extra environment variable-a PTTY variable to control the target machine's side of communications with the development machine. It is set up in the same way as the development machine. However, you should use the number of the COM port by which the target machine is connected.

### 2.4 System Files

The \AUTOEXEC.BAT and \CONFIG.SYS files on the development machine should have certain changes made to them for the tools to function properly. These changes are normally invoked by the installation program, but they are listed below for reference.

The AUTOEXEC.BAT file should define the PATH, ROOT_DIR, and PTTY environment variables on the host machine. These three variables are described in the previous section. On the target machine, the AUTOEXEC.BAT file defines only the PATH and PTTY variables for the GEOS SDK.

The CONFIG.SYS file on the host machine defines the number of files and buffers DOS can have open at one time. For best results, the FILES should be set to something larger than 80, and BUFFERS should be set to something larger than 30. On the target machine, set them to similar numbers. The exact numbers best for your system may be different. See your DOS manual for more information on these items.

### 2.5 Sample C Applications

In your development machine's \PCGEOS\APPL\SDK_C directory, there should be a number of sample applications. The documentation will refer to these applications to illustrate certain points, but you may find a brief description of each useful if you wish to browse.

APPICON Illustrates an application moniker. See the header file in the Art subdirectory to see a typical application icon.

BENOIT  Acts as an example of a large-scale application. Has several source files, and some of the source code is in assembly.

CLIPSAMP    Demonstrates use of the clipboard. This illustrates what an application can do to define its own format for cutting and pasting and working with quick transfer.

CUSTGEOM    Shows some of the special positioning directives that you can use to override and guide the Geometry Manager. This application illustrates several of the "hints" used with GenInteractions and other generic objects to force certain arrangements of UI gadgetry.

DBSAMP  Illustrates the use of the database (DB) library. The application displays various pieces of data which are stored in data structures provided by the DB library.

DOCUMENT    This directory contains several subdirectories which deal with the document control objects in one form or another. The most basic of these programs is DocView, and this application is probably the best starting place for learning about document control objects.

>DEFDOC Demonstrates the specification of a "default document" that should be opened or created when the application is invoked without a document to open.

>DOCUI  Shows that you need not use a GenView to display the contents of a document; this application uses other types of objects. As an important side effect, it also demonstrates how UI objects can send messages to the current document via the "model 
hierarchy."

>DOCVIEW    Shows the most common usage of the document control objects; this application shows the display of the document through a GenView, with the GenDocument object acting as the VisContent part of the view/content pair. 

>DOSFILE    Shows how to use a DOS file, rather than a GEOS VM file for the document. The biggest difference when working with DOS files is that the document control cannot automatically provide save or revert functionality for you.

>MULTVIEW   Shows the display of the document through a GenView, with the GenDocument object acting as the VisContent part of the view/content pair. However, this application also includes display control objects which allow the simultaneous display of multiple documents.

>PROCUI Shows how to use your process thread rather than a subclass of GenDocumentClass to display your document. The process thread, here, uses UI objects to display the contents of the document, rather than drawing to a GenView.

>PROCVIEW   Shows how to use your process thread rather than a subclass of GenDocumentClass to display your document. In this application, the process thread draws things through a GenView to display the contents of the document.

>SHAREDDB   Shows how to cope with a VM file that has been marked for multi-user access.

>VIEWER Shows how you might write something that doesn't edit files but only displays them. 

FOCUS   Demonstrates the use of the focus hierarchy. The system maintains the notion of a focus to keep track of which object should receive keyboard events. 

FSELSAMP    Demonstrates the use of a GenFileSelector independent of a GenDocumentControl (normally the document control uses the file selector to allow the user to navigate the file system-this application has a free-standing file selector). Includes several extra file-related gadgets.

FSFILTER    Demonstrates the use of a file selector UI object in filtering its display of files and directories (beyond what is provided by the various file selector filtering attributes).

GENATTRS    Demonstrates what can be done with the GenStates and GenAttrs fields of a generic object. This application illustrates the effects of changing the GenStates and three GenAttrs.

GENDLIST    Demonstrates the use and management of a GenDynamicList object. A dynamic list allows the application to provide the user with a list of items of any size without the overhead of creating an object for each item.

GENDISP Demonstrates the capabilities of the GenDisplay. The GenDisplay object is the top-most visible long-term user interface object in the system. Its subclass, GenPrimary, is the main window the user sees for an application.

GENINTER    Demonstrates the use of a GenInteraction. GenInteractions are used to group other generic gadgets. Here you can see some dialog boxes and menus.

GENITEMG    Demonstrates the various ways you can use the GenItemGroup and GenBooleanGroup objects to make lists from which the user can select items. An object of related usefulness, the GenDynamicList, is demonstrated in the GenDList application.

GENTREE Demonstrates manipulation of an application's generic tree. Includes adding and removing gadgetry to and from the tree.

GLYPH   Shows the usage of a GenGlyph UI object. A GenGlyph is a simple UI object used most often only to display a visual moniker.

GROBJ   This directory contains one subdirectory, DUPGROBJ, which in turn holds the dupgrobj sample application. This application shows the use of the Graphic Object (grobj) library together with document control objects.

GSTEST  Demonstrates some of the GString macros used in the construction of graphics strings in visual monikers. 

GSTEST3 Shows a plethora of bitmap-based GString monikers.

HELLO   Implements a Hello World style application in which colored text is drawn at an angle inside a scrolling window.

HELLO2  Implements an expanded hello program with a text entry dialog that allows the user to change the text displayed.

HELLO3  Implements an expanded hello program with triggers to change the text color.

HELP    This directory contains several sample applications showing how to use the help library.

>HELPSAMP   This sample application shows some simple usage of on-line help in a C application. Note that along with the executable, you will have to download the helpsamp.000, circles.000, and squares.000 help files. These files will go in the target's help directory.

>HELPTRIG   This sample application shows how to introduce Help triggers in different kinds of dialog boxes. Note that along with the executable, you will have to download the helptrig.000 help file. This file will go in the target's help directory.

>HELPVIEW   This application views a help file. It shows how you can use the help system as a HyperText viewer. Note that along with the executable, you will have to download the helpview GeoWrite file. You should use the help editor to generate the corresponding help file before running the program.

IACP    This directory contains a few applications which show how to use the Inter-Application Communication Protocol.

>CLIENT1    This program works together with SERVER, providing the client side of a client/server pair of programs.

>SERVER This program works together with CLIENT1, providing the server side of a client/server pair of programs.

>PALMSHUT   Example of a special purpose IACP application. This one will shut down any running copy of Palm's address book application for the Zoomer.

INKTEST This application demonstrates simple use of an ink object.

LEVELS  Shows the use of the UI levels mechanism (which allows simpler UI for less computer-savvy users).

MONIKER Demonstrates the various types of monikers a generic object can have and some of the various ways one can change the moniker an object displays. Also provides an example of a statically defined graphics string, and how to dynamically create a memory based graphics string.

MULTPRIM    Illustrates an application with more than one primary, as well as some ways an application may manipulate them. The application can change its own name and icons for each primary.

PCCOM   These applications illustrate use of the pccom library.

>PCCOM1 Shows a fairly standard use of the pccom library, with the ability to exchange files over the serial line.

>PCCOM2 This application loads the pccom library dynamically. It illustrates how to load a library dynamically and then use that library's entry points.

PSCTEXT Demonstrates the use of a PointSizeControl with a text object. Note that this same approach can be used to cause almost any text-related controller object to communicate with a text object.

SERIAL  Demonstrates use of the serial driver, including graceful handling of errors.

SIMPWP  Shows how to use a document control and display control to implement a simple word processor application. This is an excellent reference for using simple document and display groups.

SUBUI   Demonstrates the subclassing of a Generic UI gadget. In this case, a new class of trigger has been created which changes its moniker when triggered.

TICTAC  Demonstrates the manipulation of Vis objects, illustrated by a number of game pieces which the user may move with the mouse on the game board.

TRIGGERD    Demonstrates how a simple trigger can be made much more versatile by fitting it with a trigger data area.

TUTORIAL    This directory contains the application developed in the Tutorial book. The tutorial shows how the application was developed in several stages. Each subdirectory shows a stage of development.

>MCHRT1 A primary window.

>MCHRT2 Some Generic gadgetry.

>MCHRT3 Some graphics.

>MCHRT4 The Multiple Document Interface.

TWODLIST    Demonstrates correct implementation of a common task-moving choices between two dynamic lists. Normally this is done when the user is working with a number of things which may fall into two categories. They are presented with two lists, one for each category. Any list item they select will jump to the other category.

UICTRL  Shows the use of a style control as used with a text object, along with a GenToolControl object. The user may hide or show the controller's tools and move them between two tool areas.

VARDATA Demonstrates how to dynamically add and remove variable data fields to and from generic objects. In this case, the fields are geometry hints, so this application also shows the effects of some simple geometry hints on a pair of GenInteractions.

VIEW    This directory contains a number of subdirectories, each of which contains a sample application showing some specialized usage of the GenView object. The most straightforward of these examples is viewsamp, and you might want to start with this one.

>GENCONTE   Demonstrates using a GenContent as a child of a GenView. This is a rather rare usage, and demonstrates some of the more powerful (if esoteric) uses of the Generic UI.

>SCALETOF   Demonstrates how to make a document's contents "scale to fit" when the view size changes.

>SPLITVIE   Demonstrates a split view-a view that has been set up to show more than one piece of a single document.

>VIEWSAMP   Demonstrates some simple manipulations on a view, including scrolling, panning, and zooming.

VIS This directory contains a number of subdirectories, each of which in turn contains a sample application illustrating some facet of Visual objects and the Visual world.

>VISSAMP    General sample of vis objects running in a VisContent under a GenView. It includes examples of geometry management in a visible tree, a simple MSG_VIS_DRAW handler, basic mouse handling in a visible object, setting an object visible/not visible, addition/removal of objects, usage of VisMonikers, a simple MSG_VIS_RECALC_SIZE handler, custom positioning of objects, and marking an object invalid.

>VISSAMP2   Shows relation between a VisContent and its GenView in which the size of VisContent and bounds of its visible children can be set in the object definitions without any geometry management or messages being sent to the objects whatsoever.

>VISSAMP3   Demonstrates how to run a few objects under the content using the geometry manager. The view sizes itself, the content will use the view's size if possible, and the content will full justify its three Vis children horizontally while centering them vertically.

>VISSAMP4   Illustrates dynamically adding and removing objects to and from a Visible tree. Also shows geometry management of these objects and how it is affected when they are added or removed.

VTEXT   Demonstrates simple use of a visible text object.

WAVSAMP This sample application, meant for the Zoomer only, shows how to use the wav library to play sample sounds.

### 2.6 Sample Esp Programs

The SDK_ASM directory contains a number of Esp sample programs. For the most part, these programs are Esp versions of previously described C sample programs. They are listed below.

HELLO  
CLIPSAMP  
FSELSAMP  
SUBUI  
DOCUMENT  
UICTRL  
HELLO2  
GROBJ  
LEVELS  
TEXT  
HELP  
OBJ  
AVOID

[Welcome](twelcome.md) <-- &nbsp;&nbsp; [Table of Contents](../tools.md) &nbsp;&nbsp; --> [Swat Introduction](tswatcm.md)