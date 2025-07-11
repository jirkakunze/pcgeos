## 10 The GEOS User Interface

The GEOS user interface (UI) is an integral part of the system software and 
does an amazing amount of work for applications. The UI is a sophisticated 
and powerful object library that includes everything from simple buttons to 
self-scrolling windows to plug-in objects that provide nearly complete word 
processing.

GEOS also uses a new concept in user interface technology: the generic UI. By 
using the generic UI, GEOS programs simply declare their UI needs and leave 
nearly all drawing, screen management, and input management up to GEOS. 
The generic UI technology also allows a single application executable file to 
run with several different look-and-feel specifications.

This chapter will provide an overview of the specifics of the GEOS User 
Interface. For a general overview of the UI and a high-level description of its 
components, see ["System Architecture", Chapter 3](carch.md).

### 10.1 The GUI

Ever since the first text-based computer application was created, computer 
users have looked for easier, more intuitive interfaces to their programs. If 
the user has to struggle to remember commands or to find the right function, 
the program is of very little use to him. Thus, the user interface of a program, 
the program's look and feel, can often be as important as its feature set.

The graphical user interface (GUI) concept has prompted millions of people 
to begin using computers and has made computer use for millions of others 
easier. Many corporations and school systems even make GUI a requirement 
for all their computer acquisitions.

GUIs, however, are not without problems. With so many hundreds of different 
programs available, even well-designed interfaces can differ significantly 
from other well-defined interfaces. Although users no longer have to 
remember whether the "print" command is Ctrl-Alt-P or F4, they may have 
to search through dozens of menus just to find it. To solve this consistency 
problem, different manufacturers have created GUI specifications that detail 
menu structure, color schemes, input management, and many other aspects 
of the GUI.

GEOS takes this one step further with its Generic User Interface. Application 
programmers do not have to decide beforehand which GUI specification will 
be run by their program; nor do they have to write one version for each GUI 
supported. Instead, they define their generic UI needs and then let GEOS 
determine the manifestation of those needs at run-time.

Another problem with GUIs is their speed. Without powerful hardware, many 
GUI-based systems are sluggish because of high overhead associated with 
geometry management and drawing. GEOS, however, is extremely fast 
because it is entirely object-oriented and is programmed in assembly 
language. And, since most of the user interface is managed and drawn by 
GEOS, applications written for the system automatically take advantage of 
this speed.

### 10.2 The GEOS User Interface

Most programmers who write applications for GEOS will have worked with 
other GUI systems. Some will even have worked with object-oriented 
systems. Using the GEOS UI, however, will be a new experience for nearly 
every programmer.

The GEOS user interface consists of a number of dynamic libraries containing 
object classes. Applications include objects of these classes to gain various 
types of functionality. In general, applications will use two different basic 
types of objects for their UI: Generic objects provide basic UI functionality 
including windows, menus, and dialog boxes, and their implementation on 
the screen is determined by GEOS. Visible objects provide more 
application-specific functionality and offer complete display control to the 
application.

Generic object classes are all based on the class **GenClass**. Applications will 
never use any objects of **GenClass**, but they will use many objects of its 
subclasses. A list of these subclasses is shown in Figure 10-1, and each of 
these classes is described in ["Using the Generic Classes"](#103-using-the-generic-class).

Visible object classes are subclassed from **VisClass**. **VisClass** may be used 
often by applications, as will its subclasses. The subclasses of **VisClass** are 
shown in Figure 10-2, and each is described in ["Using the Visible Classes"]
(104-using-the-visible-classes).

![](Art/figure_10-1.png)

**Figure 10-1** Generic Class Hierarchy  
_All the classes shown have the properties of GenClass and can be used in an 
application's generic UI object tree._

When to use the generic or visible objects is subject to many criteria. In order 
to learn more about each of these components of the UI, you should read the 
two sections following this one.

The GEOS UI also contains many features that will often be used by 
applications but which don't fall into either the generic or the visible world. 
These features are called "mechanisms" and include input management, 
geometry management, general change notification, and the clipboard and 
quick-transfer functions. As these mechanisms are not UI-specific but are 
only implemented within the UI libraries, they are documented elsewhere in 
this book.

![](Art/figure_10-2.png)

**Figure 10-2** Visible Class Hierarchy  
_All the classes shown have the properties of VisClass and can be used in an 
application's visible object tree. Visible objects are more flexible but require 
more programming than do generic objects._

### 10.3 Using the Generic Classes

Most of your application's UI needs will be satisfied by the use of generic UI 
objects. Many applications may need only the generic classes. Generic UI 
objects are powerful and easy to use, and they provide a number of services 
normally left up to application code.

Generic object classes have no inherent visual representation. Rather, each 
generic object represents a certain set of UI functions instead of UI 
components. For example, the GenInteraction class implements grouping 
and organizational functions; it can appear in several forms including menus 
and dialog boxes. It can also have no visual representation but merely 
provide geometry management for other generic objects.

What visual form a generic object takes, if any, is determined by two factors: 
the specific UI library in use, and the instance data of the generic object.

+ A specific UI library is essentially a "UI driver" that translates the 
generic class into its specific representation. Just as each video card has 
its own driver, each UI specification (e.g. OSF/Motif or Presentation 
Manager) has its own specific UI library. This library uses the generic 
object's instance data to determine the exact form of that object's output 
and input. A GenValue object, for example, may be implemented as a 
spinner object in one specific UI but as a slider object in another. 

The generic object's instance data determines the features of the object as 
implemented by the specific UI. Across different GUI specifications, each 
similar object may appear different or handle input differently. For example, 
a button in one GUI may invert itself when pressed while the same button in 
another GUI might simply darken its outline when pressed. However, both 
buttons will do the same task. Therefore, a generic object's instance data 
must be categorized into two types: Attributes are instance data that will 
cause the same results no matter what GUI is used (e.g. the function the 
button performs). Hints are instance data that suggest particular 
implementations that may or may not be implemented by the GUI in use.

The way the generic object gets translated by GEOS into a visible 
representation is a complicated process. All generic classes are subclasses of 
**GenClass**, which is defined as a variant class. This means that **GenClass** 
has no defined superclass, that the superclass can be changed from time to 
time. The superclass is determined when the object is instantiated and 
resolved-when its visual representation is called for. (See ["GEOS 
Programming", Chapter 5](ccoding.md).)

The specific UI library contains objects subclassed off **VisClass**. These 
objects know how to handle input events and have very specific 
representation on the screen. These specific UI classes connect to the generic 
classes through the master/variant mechanism; in this way, the superclass of 
a generic object is assigned to one of the classes in the specific UI library. The 
generic object thus inherits the specific class' visible representation.

The above process happens entirely at run-time. The application does not 
have to have any knowledge of which particular GUI is in use; the same 
application executable code will work with any specific UI library. Because of 
the generic UI of GEOS, a user can run the same application under several 
look-and-feel specifications; this is desirable to the programmer because he 
only has to code the application once to receive the benefits of several 
different GUIs.

#### 10.3.1 The Generic Class Tree

Generic objects have a tremendous amount of built-in functionality. Much of 
this is built into **GenClass**, the topmost class in the generic class tree. For 
full details on **GenClass** and the other generic classes, see the Object 
Reference Book.

Among the features offered by all generic classes are

+ Visual Representation  
Through the specific UI library, every generic object provides its own 
visual representation. The application does not have to do any gadget 
drawing.

+ Monikers  
Every generic object can have a moniker, which is a name or graphic that 
gets displayed along with the object.

+ Input Management  
Through the specific UI library, every generic object properly handles 
mouse and keyboard input.

+ Messaging  
As with all objects in the system, generic objects can receive and send 
messages. In addition, generic objects can pass messages up or down the 
generic object tree automatically.

+ Enabled and Usable States  
Generic objects understand their usable and enabled states. If an object 
is not enabled, it may be visible but the user cannot invoke its action. If 
an object is not usable, it will not be visible on the screen (in most specific 
UIs). Entire object trees can be set usable or enabled with one command.

+ State Saving  
Generic objects automatically save their state when the system shuts 
down. Therefore, when the system comes back up, dialog boxes and 
windows will automatically be restored to the same state they were left 
in when shut down.

+ Object Tree Management  
All generic objects must be part of a generic object tree to be displayed. 
Generic objects inherently understand the tree structures and functions. 
UI gadgetry can dynamically be added, moved, or removed.

Each of the different generic classes is described in overview depth below. 
Note that every one of these classes may be subclassed to add, change, or 
remove functionality. Changing or removing functions from a generic class is 
not encouraged, however, as it can cause a specific UI library to give 
unpredictable results. For a diagram of all the generic classes in their class 
hierarchy, see Figure 10-1.

##### 10.3.1.1 GenClass

**GenClass** provides the functionality basic to all generic objects. **GenClass** 
is not used directly by any applications and has no visible representation. 
Rather, all generic classes are subclassed off **GenClass**. **GenClass** provides 
instance fields common to all of its subclasses. Instance fields of special 
interest include

+ links between parents and children, providing the means of constructing 
a generic tree.

+ text or graphics strings to serve as an object's visual moniker.

+ a keyboard accelerator to activate an object through keyboard events.

+ a state field relating to the usable state of an object.

+ an attributes field relating to other default behavior, such as how the 
object handles busy states when an application is waiting for a routine to 
finish.

**GenClass** also implements scores of hints that can affect UI geometry, visual 
representation, data structures, and functions.

##### 10.3.1.2 GenApplicationClass

**GenApplicationClass** provides the basic functionality to open and close 
applications within GEOS. An object of this class serves as the top object in 
any application for GEOS.

##### 10.3.1.3 GenPrimaryClass

**GenPrimaryClass** is a subclass of **GenDisplayClass**. The GenPrimary is 
the chief UI grouping object of an application, and it usually appears as the 
application's primary window. An application's GenPrimary object manages 
all controls and output areas that are invoked when an application is first 
launched. You will usually create a GenPrimary as the sole child of your 
GenApplication object.

##### 10.3.1.4 GenTriggerClass

A GenTrigger is a simple pushbutton that executes an action when activated 
by the user. Typically, the trigger will have a moniker displayed within it and 
will be activated by a mouse click or by a special keystroke sequence. 
GenTriggers are very common in applications.

##### 10.3.1.5 GenInteractionClass

GenInteraction objects are essentially grouping mechanisms. 
GenInteractions are the key objects for creating both menus and dialog 
boxes, and they can be used to organize the geometry of other generic objects. 
Typically, a GenInteraction will have a number of children, each of which will 
appear within the interaction on the screen. The Interaction itself may or 
may not have a visible representation.

##### 10.3.1.6 GenViewClass

The GenView object provides a scrollable window in which the application 
has complete drawing control. Most applications will use a GenView, and 
many will use it in conjunction with a VisContent object. The View is 
extremely powerful, providing all clipping, scrolling, resizing, and scaling 
automatically. A View can even be splittable or linked to other views. The 
GenView can display either normal graphic documents or hierarchies of 
visible objects. 

##### 10.3.1.7 List Classes

Together, GenBoolean, GenBooleanGroup, GenItem, **GenItemGroup**, and 
GenDynamicList provide many different types of lists. List objects may be 
used to create lists that are dynamic or static; scrollable or not; exclusive, 
non-exclusive, or otherwise. List objects may appear within menus or dialog 
boxes as well as within an application's primary window.

##### 10.3.1.8 GenValueClass

The GenValue object allows the user to set a value within a particular range. 
This may be implemented as a slider, a spinner, or a pair of up/down buttons 
next to the value. Ranges may use scalar or distance values and can have 
their maximum and minimum values set by the application.

##### 10.3.1.9 GenTextClass

**GenTextClass** is tremendously versatile and can be used for text displays or 
text-edit fields. The GenText is used by nearly every application that either 
displays text or requires text input. It is so versatile and powerful that it can 
provide the power of an entire word processor without any additional code in 
the application.

The GenText object supports selection and control of fonts, point sizes, text 
color, paragraph color, paragraph borders, margin settings, tab stops, 
manual leading, and character kerning, as well as several other features. The 
text library also provides several controllers that work with the GenText to 
allow the user to set all these features.

##### 10.3.1.10 Document and Document Control Classes

Together, **GenDocumentClass**, **GenDocumentGroupClass**, and 
**GenDocumentControlClass** provide all the functions necessary to create, 
save, open, and edit document files. These classes provide not only the UI 
menus and tools (the File menu) but also the functions for managing the 
document files. Applications that use these classes never have to call routines 
to open, close, or save files-all that is done automatically, including the file 
selector mechanisms to aid the open and save-as functions.

GenDocument objects are created and managed automatically by the 
GenDocumentGroup. Each document object represents a single file which 
has been opened or newly created by the user.

##### 10.3.1.11 Display and Display Control Classes

Together, the GenDisplay, GenDisplayGroup, and GenDisplayControl 
provide display windows and the UI gadgetry to manage them. Typically, 
these objects will be used in conjunction with the document and document 
control objects to provide one display for each document.

The GenDisplayGroup object creates and manages multiple GenDisplay 
document windows. The GenDisplayControl object creates and maintains a 
Window menu to allow the user to operate on the individual displays. If your 
application will have multiple documents or multiple displays open, you will 
want to use these objects.

##### 10.3.1.12 GenControlClass and UI Controllers

**GenControlClass** is used to create UI controller objects. Applications will 
most likely not use **GenControlClass** directly, though some object libraries 
might. For example, the Text Library uses controller objects for font control, 
point size control, and style control, among other things. Any application that 
uses the Text Library can include the font controller object; the user will then 
be able to select and apply fonts without the application having to do any 
work to support it.

The UI and various libraries provide many controllers you can use 
immediately. Some examples are the **GenEditControlClass**, which creates 
and maintains the Edit menu and tools; **ColorSelectorClass**, which creates 
and displays UI gadgetry to set color information; and 
**GenViewControlClass**, which creates and maintains a View menu 
allowing the user to set scaling and scrolling behavior.

##### 10.3.1.13 GenToolControlClass

**GenToolControlClass** lets the user select which of an application's tools 
are available and where they should be placed (in a toolbox, in a menu, etc.). 
Tools are provided by UI controllers. Typically, an application that uses 
controllers will provide several tool areas and a GenToolControl; the 
GenToolControl will automatically create all the UI gadgetry to let the user 
select which tools are active and where they will appear.

##### 10.3.1.14 GenFileSelectorClass

The GenFileSelector provides user interface to allow the user to navigate 
through his or her file system. It is used most often by the document control 
objects and is used directly by only some applications.

##### 10.3.1.15 GenGlyphClass

GenGlyph displays simple text or graphics strings. Text displayed by a Glyph 
object is not selectable or editable; these objects are typically used for 
labeling areas or items on the screen.

##### 10.3.1.16 GenContentClass

**GenContentClass** is used with a GenView to display other generic UI 
objects within a scrollable window. The GenContent is rarely used because 
having some of an application's UI objects not visible can confuse some users. 
More often, a GenDocument is used as the view's content; 
GenDocumentClass is subclassed from the GenContent.

#### 10.3.2 Creating a Generic Object Tree

You don't have to understand all the generic object classes to create a 
complete generic object tree for your application. For insight into and an 
example of creating a generic tree (including the primary window, a menu, a 
dialog box, and a scrolling view window), see ["First Steps: Hello World", 
Chapter 4](cgetsta.md).

### 10.4 Using the Visible Classes

The visible classes in GEOS provide custom objects that can be used for any 
number of purposes. There are many visible object classes and they are so 
versatile that everything from spreadsheets to drawing programs to 
interactive games can be created from them.

#### 10.4.1 Visible Objects and the GenView

Visible objects are designed for flexibility and for interacting with the user. 
Typically, visible objects will reside in an object tree, the root of which is a 
VisContent or GenDocument object connected to a GenView (this section 
assumes the VisContent, though the GenDocument is roughly equivalent for 
the purposes of overview). The relationship between these objects is simple, 
yet it accomplishes an immense amount of work for the application.

The GenView provides a window that may be scrollable, scalable, and 
resizable. The view is directly connected to the VisContent object and works 
very closely with it to handle input and drawing events. The VisContent acts 
as a manager for the visible object tree; it passes input events (mouse moves 
and mouse clicks) on to the proper visible object directly under the mouse 
pointer, and it makes sure all visible objects in the tree draw themselves at 
the appropriate time.

The view and VisContent may be connected tightly or very loosely for sizing 
purposes. For example, the view may be forced to resize itself to the proper 
size of the VisContent; or, the view might be scrollable and resizable 
independent of the content's size. The VisContent can choose whether mouse 
events are expected or required, and the view will notify the VisContent 
whenever the window has been invalidated and the document needs 
redrawing.

#### 10.4.2 The Visible Object Document

Visible objects exist in an object tree and draw themselves in the GEOS 
graphic space. Every visible object knows where in the graphic coordinate 
space it sits and how big it is. The top object of the object tree, the VisContent, 
manages the entire tree. Composite objects (of class **VisCompClass**) can be 
used as organizational objects that control other objects.

Using the VisContent, the VisComposite, and the standard Vis objects, you 
can create just about any interactive document you want. (There are other 
available object libraries such as the bitmap and the ruler libraries, and they 
are based on these three building blocks.)

Every visible object knows what its bounds are; that is, each object knows 
exactly how big it is and where it sits in the graphics document. The bounds 
are always rectangular in the base **VisClass** format, though a subclass of 
**VisClass** could easily be created to handle more complex shapes. Bounds are 
in the standard graphics coordinates (i.e. 16-bit numbers based on a 72 dpi 
grid). Vis objects may also exist in large documents (32-bit coordinates), but 
the objects must handle the majority of the extra coordinate manipulation on 
their own.

Visible objects do not inherently know how to draw themselves. However, the 
GenView, when resized or scrolled (for example), will send 
MSG_META_EXPOSED to the VisContent. The VisContent object will then 
send a MSG_VIS_DRAW message to itself and to all its visible object children, 
directing them to draw themselves in the proper place. For the visible object 
to draw itself, it must handle MSG_VIS_DRAW. In its handler, it can call any 
normal graphics commands to draw anything wherever it wants (not just 
within its bounds).

For examples of visible objects that know their bounds and that handle 
MSG_VIS_DRAW, see ["A UI Example"](#105-a-ui-example).

#### 10.4.3 Visible Object Abilities

Visible objects can be used in innumerable situations; with a little work from 
the application developer, they provide dozens of useful features including

+ Knowledge of Their Bounds
Every visible object knows exactly where in the application's document it 
sits and exactly how big it is. It can easily change its own bounds, either 
moving itself in the document space or resizing itself.

+ Ability to Draw Themselves
Every visible object is responsible for drawing itself. When it receives a 
drawing message (MSG_VIS_DRAW), it must recognize the context of the 
message and draw itself appropriately. **VisClass** does not have a default 
handler for MSG_VIS_DRAW; each subclass must handle this message 
itself.

+ Input Handling
Pointer and click events as well as keyboard input can be automatically 
transmitted through the view to the VisContent if desired. Several 
combinations of event pass-throughs are available. When the VisContent 
receives a mouse event, it can intercept and handle it or pass it on to 
whatever object is under the pointer. If the VisContent does not handle 
the event (such as MSG_META_PTR) explicitly, the message will 
automatically be passed on to the object under the pointer.

+ Geometry Management
Both the view and the VisContent can interact to provide specific sizing 
behavior for the window and content. Additional geometry management 
is available to help a VisComp composite object manage its children.

+ Object Tree Management
Messages may easily be passed to an object's parent or children. Also, 
objects may be added to or removed from the visible tree without 
difficulty.

+ Use of VisMonikers
In addition to simply drawing itself, a visible object can have a visible 
moniker that it can draw as well. This simplifies drawing text or 
handling MSG_VIS_DRAW in that the moniker can be set beforehand and 
simply drawn with a single call.

Using visible objects allows an application a lot more flexibility in display 
and input handling than is available with the generic objects. It also requires 
more programming; however much of the most difficult work is made easy by 
the view and VisContent objects. The application does not have to worry 
about clipping or scrolling or even determining what type of input event is 
taking place. All this is somewhat automatic and can, for the most part, be 
ignored.

#### 10.4.4 The Vis Class Tree

There are four base visible classes on which the other object libraries are 
founded. **VisClass** is the most basic and at the root of the visible class tree. 
Under it are VisComp and VisText. VisContent is a subclass of VisComp.

**VisTextClass** is special in that it is rarely subclassed as it already contains 
nearly all the functionality an application will need from a visible text object.

The other three classes are typically subclassed by applications. None of the 
three can draw itself; instead, the subclass must handle MSG_VIS_DRAW, the 
message that indicates the object must draw itself. The three different 
classes are used as follows:

+ **VisClass**
**VisClass** is the head of the visible class tree and therefore is the most 
broad-based in functionality. **VisClass** objects can not have children and 
therefore can only exist as the leaves of the object tree.

+ **VisCompClass**
**VisCompClass** provides a composite **VisClass** object. Essentially, this 
class is the same as **VisClass** except that it can have children. It also 
includes several special geometry options that allow it to manage and 
place its children as well as set their bounds.

+ **VisContentClass**
**VisContentClass** is used only as the content of a **GenView**. It is a 
subclass of **VisCompClass** and therefore can have children and manage 
them with the same geometry features available to VisComp. In addition, 
the VisContent interacts with the GenView to handle input and drawing. 
The content can interact with the view to determine sizing behavior as 
well as input behavior.

#### 10.4.5 Creating a Visible Object Tree

You can create a visible object tree either in your Goc source code or at 
run-time. As stated earlier, the visible tree is linked to the generic UI object 
tree through the GenView object. Without a view object, you will not be able 
to display your visible objects.

The visible object tree must have a **VisContentClass** object (or a subclass of 
**VisContentClass**) as its root object. If another class is chosen for the 
topmost object, that class will have to handle all the messages that would 
normally be received by a content; otherwise, results are unpredictable and 
your application will likely not function the way you expect.

VisContent objects are rarely, if ever, used lower in the visible tree than as 
the top node. For levels further down in the tree, you can use **VisCompClass** 
(or, again, a subclass of **VisCompClass**). VisComp allows the object to have 
children and does not incur the same amount of overhead associated with a 
VisContent. Any object in the visible tree that may have children must be a 
"composite" object, or a subclass of **VisCompClass**.

Leaf nodes of the visible tree can be direct subclasses of **VisClass**. Objects of 
**VisClass** can not have children, but they have all the attributes and instance 
data you need for a leaf node. You must subclass **VisClass**, however, if your 
object is ever to be shown on the screen: Since visible objects can have any 
application-defined visible representation, you must write the 
MSG_VIS_DRAW handler yourself; this can be done only in a subclass.

#### 10.4.6 Working with Visible Object Trees

Working with visible object trees is quite easy and provides immense 
flexibility to your applications. Entire groups of objects can be added to, 
removed from, or moved around in the display with a single command. New 
objects can be created during execution, and others can be destroyed at will.

#### 10.4.6.1 Sending Messages in the Tree

Often, an object in the visible tree will need to contact its parent or its 
children. For example, a child that needs to know the state of its parent must 
be able to find and contact that parent. Likewise, a parent that must notify 
all its children to redraw themselves must be able to quickly send out the 
notification.

An object can send a message to its parent by simply specifying **@visParent** 
as the message recipient. That same object could also contact all its children 
by specifying the recipient as **@visChildren**. The message sent (only with 
**@send** since there are multiple destinations) will go to each of the object's 
children.

If you need more complicated message passing, you can build this into your 
Vis subclasses. It is not difficult for an object to retrieve the optr of any of its 
children or of its parent; it can then send the message appropriately. Each 
recipient can handle it in the same manner, forwarding it on to the next layer 
of the tree.

**VisClass** also employs several messages known as "visual upward queries," 
or "VUP" messages. These travel up the visible tree until they encounter an 
object of the proper class, where they will be handled.

#### 10.4.6.2 Altering the Tree

**VisClass** and **VisCompClass** can handle messages that retrieve various 
information about the visible tree. They also have messages for altering the 
visible tree's structure.

### 10.5 A UI Example

This section uses the sample application TicTac, a simple Tic Tac Toe board 
and pieces that can be moved around the board.

#### 10.5.1 What TicTac Illustrates

The TicTac sample application can teach you several things about how the 
visible world works and about how to manage visible objects in an 
application. It is simple but illustrates the following concepts:

+ Drawing Visible Objects
Every visible object must be able to draw itself. **VisClass** does not have 
any inherent code to make visible objects visible; instead, subclasses of 
**VisClass** must handle MSG_VIS_DRAW.

+ Changing an Object's Position
The pieces of the TicTac game allow the user to move them around the 
screen using the mouse. This entails tracking the mouse and then 
changing the object's position in the document.

+ Sending Messages Up and Down the Visible Tree
The pieces must interact with the VisContent object to ensure they stay 
on the game board, and the VisContent must notify the pieces when the 
"New Game" button has been pressed. Therefore, messages must be 
passed both up and down the visible tree.

+ Handling Input Events
In order for the user to move the pieces around the board, the pieces must 
be able to receive mouse events. Several different types of mouse events 
are handled to show the pieces moving around the board and to handle 
input properly.

+ Drawing Background Graphics
Because the game board does not change at all, its background is drawn 
by the content object. The content could change its drawing behavior if 
necessary, but background graphics are most easily drawn by the 
content.

+ Visible Tree Structure
A simple, two-layer visible tree is used in the TicTac application: The 
VisContent is the root of the tree, and each of the pieces is a leaf.

#### 10.5.2 What TicTac Does

The TicTac sample application is extremely simple. It draws a Tic Tac Toe 
board and its outline, and it has ten pieces which the user can move. Five of 
these pieces are gray squares, and five are gray circles. Each of the pieces 
may be moved by clicking and dragging it with the mouse. Any piece may be 
moved anywhere on the board, but pieces may not be moved off the board.

A Game menu with a "New Game" trigger replaces all the pieces to their 
original locations. Because this is a simple example, no actual rules of any 
kind are enforced. Recognition of winning sequences and rules involving turn 
sequencing or playing against the computer are left as exercises for the 
reader.

#### 10.5.3 The Structure of TicTac

The TicTac sample application is coded in two files: The first, tictac.gp, is the 
geode parameters file. The other, tictac.goc, contains all the code for objects 
in the application. The geode parameters file is similar to other .gp files and 
is not discussed in this section.

The application uses two different object trees, one for its generic UI and one 
for the game board and game pieces. The first consists of generic UI objects 
and the second of objects subclassed off **VisClass** and **VisContentClass**. 
The two object trees are diagrammed in Figure 10-3 and are described below.

#### 10.5.3.1 TicTac's Generic Tree

The TicTac application begins with two standard generic objects common to 
all applications: GenApplication and GenPrimary. These two objects are 
described in detail in other sections and are not covered here, though their 
definitions are shown in Code Display 10-1.

The primary object, TicTacPrimary, has two children. One, 
TicTacGameMenu, implements the game menu; the other, TicTacView, 
provides the window through with the user can interact with the visible tree.

![](Art/figure_10-3.png)

**Figure 10-3** The TicTac Object Trees  
_The generic UI object tree has five objects, and the visible object tree has 
eleven. The two trees communicate via the TicTacView/TicTacBoard link, 
determined by the fact that TicTacBoard is set as TicTacView's content object._

---
Code Display 10-1 TicTacApp and TicTacPrimary
~~~
@start AppResource;
    /* The AppResource resource block contains the TicTacApp object only. This
     * object is in its own resource for performance purposes. */

@object GenApplicationClass TicTacApp = {
    GI_visMoniker = list { TicTacTextMoniker }
    GI_comp = TicTacPrimary;
    gcnList(MANUFACTURER_ID_GEOWORKS, GAGCNLT_WINDOWS) = TicTacPrimary;
}

@visMoniker TicTacTextMoniker = "TicTacToe";

@end AppResource

@start Interface;

    /* This is the Primary window of the application. It is not minimizable
     * (since no icon is defined for it). It has two children: The View
     * object and the Menu object. */
@object GenPrimaryClass TicTacPrimary = {
    GI_comp = TicTacView, TicTacGameMenu;
    ATTR_GEN_DISPLAY_NOT_MINIMIZABLE;
    HINT_SIZE_WINDOW_AS_DESIRED;
}
~~~

**TicTacGameMenu**

This object is a standard GenInteraction object set up as a menu. The code 
for the entire menu (including the TicTacNewTrigger) is shown in Code 
Display 10-2 and is heavily commented as to how each attribute is used.

The New Game trigger may be invoked at any time except when a piece is 
being moved. When a piece is being moved, that piece object has the "mouse 
grab," meaning that it will receive all mouse and pointer events until it 
releases the grab. When an object has the mouse grab, no mouse events may 
be sent to another object, and therefore the menu object can not be clicked on 
while the piece has the grab.

When the user presses the New Game trigger, the trigger sends its message, 
MSG_TICTAC_NEW_GAME, to the TicTacBoard object. The TicTacBoard 
object is the top object in the visible tree and, upon receipt of this message, 
resets the game board and notifies all the piece objects of the reset. This 
process is described below in ["TicTacBoard Specifics"](#1054-tictacboard-specifics).

---
Code Display 10-2 The TicTac Game Menu
~~~
/* The TicTacGameMenu object is the only menu of this application. Its only child
 * and only menu entry is the TicTacNewTrigger object. */

@start Interface;       /* In the same resource block as TicTacPrimary. */

@object GenInteractionClass TicTacGameMenu = {
    GI_visMoniker = "Game";                     /* The name of the menu. */
    GI_comp = @TicTacNewTrigger;        /* The only menu item. */
    GII_visibility = GIV_POPUP;         /* This attribute indicates that this
                                         * interaction is a menu rather than
                                         * a dialog. */
}

@object GenTriggerClass TicTacNewTrigger = {
    GI_visMoniker = "New Game";         /* The name of the menu item. */
    GTI_destination = @TicTacBoard;     /* The object to receive the "New Game"
                                         * message: the game board object. */
    GTI_actionMsg = MSG_TICTAC_NEW_GAME;        /* The message to be sent when the trigger
                                         * is pressed. */
}

@end Interface          /* End of the Interface resource block */
~~~

**TicTacView**

The TicTacView object is a standard GenView set up to run the TicTacBoard 
object. The code for TicTacView is shown in Code Display 10-3 and is heavily 
commented to show what each of the view's attributes is used for.

TicTacView's content object is TicTacBoard, the game board object. This 
means that any appropriate input events as well as all messages sent out by 
the view will be passed directly to the TicTacBoard object. The sizing 
attributes and the fact that the view is not marked GVDA_SCROLLABLE in 
either dimension makes sure the view sizes exactly to the game board's 
bounds.

A GenView object is necessary in every case where a visible object tree is 
used. The view not only displays the visible tree but also handles all clipping, 
scaling, scrolling, and sizing if any is desired (none is used in TicTac). It also 
takes and passes on any appropriate mouse or keyboard input events. 
Additionally, it interacts directly with its content object (in this case 
TicTacBoard) to determine proper geometry and sizing behavior of the 
content and the view.

This view also provides the background color of the game board, dark blue.

---
Code Display 10-3 The TicTacView Object
~~~
    /* This object provides the window through which the user interacts with the
     * visible object tree. This object communicates with the game board object (a
     * subclass of VisContentClass) to coordinate drawing, clipping, sizing, and
     * even input handling. */

@start Interface;                       /* In the same resource block as TicTacPrimary. */

@object GenViewClass TicTacView = {
    GVI_content = @TicTacBoard;         /* The content object of this view is the
                                 * TicTacBoard object, the root object of the
                                 * visible object tree. */

    GVI_color = { C_BLUE, 0, 0, 0 };    /* The background color of this view
                                         * should be dark blue. */

    /* The horizontal attributes of this view set it to the same
     * size as the game board, and the view is not scrollable. */

    GVI_horizAttrs = @default   | GVDA_NO_LARGER_THAN_CONTENT
                                | GVDA_NO_SMALLER_THAN_CONTENT;

    /* The vertical attributes of this view set it to the same size
     * as the game board, and the view is not scrollable. */

    GVI_vertAttrs = @default    |        GVDA_NO_LARGER_THAN_CONTENT
                                | GVDA_NO_SMALLER_THAN_CONTENT
                                | GVDA_KEEP_ASPECT_RATIO;

    /* The user won't need to type anything, so there's no need for
     * a floating keyboard. */
    ATTR_GEN_VIEW_DOES_NOT_ACCEPT_TEXT_INPUT;
}
@end Interface                  /* End of the Interface resource block */
~~~

##### 10.5.3.2 TicTac's Visible Tree

The visible tree contains eleven objects. One acts as TicTacView's content and 
is of TicTacBoardClass, a subclass of **VisContentClass**. The other ten are 
all game pieces of class TicTacPieceClass, a subclass of **VisClass**. Both the 
class definitions and the object definitions are given in Code Display 10-4.

![](Art/figure_10-4.png)

**Figure 10-4** The TicTac Game Board  
_The TicTacBoard object draws the game board and manages the game pieces 
on the board. The pieces know their starting locations, shown here._

All eleven of the visible objects remain on the screen and in the view during 
their entire existence. The game board and all its pieces are shown in 
Figure 10-4; this illustration represents the basic configuration of the game 
board when the application first starts or when the user presses the "New 
Game" trigger in the "Game" menu.

The TicTacBoard object draws the border around the board and makes sure 
the view window sizes itself to the same size as the board. The board is 180 
points (2.5 inches) in height and 270 points (3.75 inches) in width; these 
numbers are stored as the constants BOARD_HEIGHT and BOARD_WIDTH. 
TicTacBoard also draws the playing field-this consists of the four white 
lines on the left side of the game board. TicTacBoard's other main function is 
to ensure that all the children (game pieces) behave properly; it makes sure 
the child's bounds are on the game board when the piece is moved, and it 
notifies the game pieces when they must draw themselves due to a view 
exposure. Finally, TicTacBoard receives the "New Game" message from the 
Game menu; it then redraws the game board and notifies each of the game 
pieces that they should return to their initial locations.

Each of the game piece objects knows about its location and status. Each 
piece knows its initial location, current location, and proposed location 
(during a move). Every game piece is an instance of TicTacPieceClass. This 
class is shown in Code Display 10-4; it contains a number of instance data 
fields for these locations. It also has an instance data field indicating what 
type of piece (i.e. "box" or "ring") the object is.

The "box" objects are designated by having the value TTPT_BOX in their 
TTP_pieceType fields; the "ring" objects have TTPT_RING in that field. Both 
types of objects act and react in the same way to various events; the only 
difference is in their shape and color.

---
Code Display 10-4 TicTacBoardClass and TicTacPieceClass
~~~
/* The TicTacPieceTypes enumerated type lists the different types of game pieces a
 * particular piece object can be. In this game, a piece is either a "box" (gray
 * square) or a "ring" (light gray circle).     */

typedef ByteEnum TicTacPieceTypes;
#define TTPT_BOX 0
#define TTPT_RING 1

/***********************************************************************
 * TicTacBoardClass
 * This class is a subclass of VisContentClass and provides the game board
 * for this application. It also manages all the children (piece objects).
 * Because it is a subclass of VisContentClass, it inherits all the instance
 * data fields and messages of that class.
 ***********************************************************************/

@class TicTacBoardClass, VisContentClass;       /* this class is a subclass
                                         * of VisContentClass */

    /* Message definitions for this class */
    @message void MSG_TICTAC_NEW_GAME();
        /* This message is sent by the New Game trigger in the Game menu
         * when the user wants to reset the game. It is sent directly to
         * the game board object and causes the board object first to
         * send the "new game" message to each of its children and then
         * to redraw the game board.    */

    @message Boolean MSG_TICTAC_VALIDATE_BOUNDS(word bottom, word right,
                                            word top, word left);
        /* This message is sent by a game piece that is being moved by the
         * user and is about to be set down. The four parameters are the
         * proposed new bounds of the moved piece; if they are within the
         * game board's limit, this message returns TRUE. If they are at
         * all outside the game board, this message returns FALSE.      */

@endc

/* Declare the class in memory so the method table will be built. */
@classdecl TicTacBoardClass;

/***********************************************************************
 * TicTacPieceClass
 * This class is a subclass of VisClass and provides all the functions
 * necessary for a game piece in this game. Because it is a subclass of
 * VisClass, it inherits all the instance data fields and messages of
 * that class.
 ***********************************************************************/

@class TicTacPieceClass, VisClass;              /* this class is a subclass
                                         * of VisClass */

        /* The instance data fields of this class: */
    @instance TicTacPieceTypes TTP_pieceType;
        /* TTP_pieceType defines whether the object of this class is
         * a "box" or a "ring."                                                 */

    @instance int TTP_vertPos;
        /* TTP_vertPos indicates the current y position of
         * the piece. This does not indicate the piece's actual
         * bounds but rather where its moving outline appears.  */

    @instance int TTP_horizPos;
        /* TTP_horizPos indicates the current x position of
         * the piece. This does not indicate the piece's actual
         * bounds but rather where its moving outline appears.  */

    @instance int TTP_origVertPos;
        /* TTP_origVertPos indicates the y position where this
         * piece should return when the New Game trigger is pushed
         * and the piece goes back to its original location.    */

    @instance int TTP_origHorizPos;
        /* TTP_origHorizPos indicates the x position where this
         * piece should return when the New Game trigger is pushed
         * and the piece goes back to its original location.    */

    @instance Boolean TTP_dragging;
        /* A flag indicating whether the user is in the process of dragging
         * the game piece around the board. */

        /* Message definitions unique to this class. */
    @message void MSG_PIECE_NEW_GAME();
        /* This message notifies the piece object that the user has pushed *
         * the New Game trigger and that the piece should return to its *
         * original position on the board (the TTP_orig(Horiz/Vert)Pos *
         * fields).                                                     */
@endc

/* Declare the class in memory so the method table will be built. */
@classdecl TicTacPieceClass;
~~~

#### 10.5.4 TicTacBoard Specifics

The TicTacBoard object, being of a subclass of **VisContentClass**, handles 
many messages specific to content objects. However, only three messages are 
handled specifically by TicTacBoardClass. These messages are

MSG_VIS_DRAW  
This message notifies the object it must draw itself and any 
accompanying graphics. TicTacBoard responds by drawing the 
game board (the border and crossed lines) and by passing the 
MSG_VIS_DRAW on to all of its children.

MSG_TICTAC_NEW_GAME  
This message is sent by the New Game trigger in the Game 
menu. TicTacBoard responds by sending another new game 
message, MSG_PIECE_NEW_GAME, to each of its children. 
Each child will position itself and draw itself properly; 
TicTacBoard will then redraw the game board by sending itself 
a MSG_VIS_DRAW.

MSG_TICTAC_VALIDATE_BOUNDS  
This message is sent by a game piece object when it is being 
moved. TicTacBoard checks the parameters passed and 
determines whether they are on the game board or not; if they 
are it returns TRUE, and if they aren't it returns FALSE.

Each of the methods for the above messages is shown in Code Display 10-5. 
Each is heavily commented and explains the theory behind the method.

---
Code Display 10-5 Methods of TicTacBoardClass
~~~
/***********************************************************************
 *
 * MESSAGE:             MSG_TICTAC_NEW_GAME for TicTacBoardClass
 *
 * DESCRIPTION:         This method notifies each of the visible children that
 *              a new game is beginning; they should take their places,
 *              and then the board object will redraw itself
 *
 * PARAMETERS:
 *      void ()
 *
 ***********************************************************************/

@method TicTacBoardClass, MSG_TICTAC_NEW_GAME {
    WindowHandle win;                           /* the window to draw to        */
    GStateHandle gstate;                        /* the gstate of the window */

        /* First notify all the children (game pieces)
         * that a new game is beginning.*/

    @send @visChildren::MSG_PIECE_NEW_GAME();

        /* Now initiate a new gstate for the view window.
         * Get the window handle from the view, and then
         * create a new gstate for it.  */

    win = @call TicTacView::MSG_GEN_VIEW_GET_WINDOW();
    gstate = GrCreateState(win);

        /* Invalidate the game board rectangle in the document.
         * This will cause the view object to generate a
         * MSG_META_EXPOSED for the rectangle, causing MSG_VIS_DRAW
         * to be sent to this object (TicTacBoard).     */

    GrInvalRect(gstate, 0, 0, BOARD_WIDTH, BOARD_HEIGHT);

        /* Now destroy the temporary gstate. This is important
         * to keep too many gstate handles from being locked
         * and slowing down the system. */

    GrDestroyState(gstate);
}

/***********************************************************************
 *
 * MESSAGE:             MSG_VIS_DRAW for TicTacBoardClass
 *
 * DESCRIPTION:         This method draws the board's outline and the
 *              lines of the playing field. It is sent each time
 *              a portion of the view window becomes invalid (such
 *              as when the primary is moved).
 * PARAMETERS:
 *      void (word drawFlags, GStateHandle gstate)
 *              gstate is the handle of the graphics state associated
 *              with the exposed portion of the view window
 ***********************************************************************/

@method TicTacBoardClass, MSG_VIS_DRAW {
        /* Set up the graphic state properly. The board
         * lines are to be white and three points thick.*/

    GrSetLineColor(gstate, CF_INDEX, C_WHITE, 0, 0);
    GrSetLineWidth(gstate, 3);

        /* Now draw the border of the game board. It is a
         * rectangle that outlines the entire board.    */

    GrDrawRect(gstate, 0, 0, BOARD_WIDTH, BOARD_HEIGHT);

        /* Set and draw the Tic Tac Toe playing field. The
         * lines are now set to 4 points thickness, and the
         * lines are drawn with HLine and VLine graphics
         * commands. Ideally, preset constants would be used.*/

    GrSetLineWidth(gstate, 4);
    GrDrawHLine(gstate, 5, 60, 175);
    GrDrawHLine(gstate, 5, 120, 175);
    GrDrawVLine(gstate, 60, 5, 175);
    GrDrawVLine(gstate, 120, 5, 175);

        /* When the MSG_VIS_DRAW is received by the game board,
         * it must pass it on to its visible children. It must
         * also pass on the parameters of the message as passed
         * to ensure all drawing is done properly.      */

    @send @visChildren::MSG_VIS_DRAW(drawFlags, gstate);
}

/***********************************************************************
 *
 * MESSAGE:             MSG_TICTAC_VALIDATE_BOUNDS for TicTacBoardClass
 *
 * DESCRIPTION:         This method checks to see if the bounds passed
 *              are on the game board. This is invoked when a game
 *              piece is in motion and receives an END_SELECT message
 *              indicating it is being put down. The piece must
 *              determine whether the suggested bounds are on the
 *              game board; the piece should always query the board
 *              object rather than check directly; if the board were
 *              resizable, the piece could be incorrect sometimes.
 *
 * STRATEGY:            Check the four bounds against the board's edges. If
 *              all four are on the board, return TRUE. If any one
 *              of the four is off the board, return FALSE.
 * PARAMETERS:
 *      void (word bottom, word right, word top, word left)
 ***********************************************************************/

@method TicTacBoardClass, MSG_TICTAC_VALIDATE_BOUNDS {
    if (((bottom < BOARD_HEIGHT) && (top > 0))
                        && ((right < BOARD_WIDTH) && (left >= 0))) {
        return(TRUE);
    } else {
        return(FALSE);
    }
}
~~~

#### 10.5.5 TicTacPiece Specifics

TicTacPieceClass contains most of the game's functionality. Since the user 
interacts directly with each game piece, the piece must know not only how to 
draw itself but also how to react to user input.

MSG_PIECE_NEW_GAME  
This is the only message generated by the game itself that a 
game piece receives; it is sent by the TicTacBoard object when 
the user has pressed the New Game trigger. The game piece 
object responds by resetting its bounds to the original settings. 
It does not have to redraw or invalidate its old bounds because 
the TicTacBoard object will send a MSG_VIS_DRAW later and 
will invalidate the entire board.

MSG_VIS_DRAW  
This message notifies the object that it must draw itself and 
any accompanying graphics. The game piece responds by 
drawing the proper shape in the proper color in the proper 
place. Since every **VisClass** object inherently knows its 
location and bounds, the object already knows where and how 
big the shape should appear. Whether a gray box or circle is 
drawn depends on the TTP_pieceType instance data field.

MSG_META_START_SELECT  
This message is sent by the system when the user clicks a 
mouse button. The UI sends the message to whatever object lies 
under the pointer. The UI objects then pass the message down 
the object tree until it gets handled. The progression sends the 
message to TicTacApp, which passes it to TicTacPrimary, which 
passes it to TicTacView, which passes it to TicTacBoard, which 
(by letting the default **VisContentClass** method handle it) 
passes it to the proper game piece object (if any) under the 
pointer. The game piece responds by grabbing the mouse and 
all subsequent pointer events.

MSG_META_DRAG_SELECT  
This message, like MSG_META_START_SELECT, indicates that 
the user has clicked a mouse button and has initiated a drag 
event. (Normally, this is used to select ranges or groups of 
objects; in TicTac, however, it is treated like 
MSG_META_START_SELECT.)

MSG_META_DRAG  
This message is sent after the user has clicked the mouse 
button and is now moving the mouse pointer (and has not 
released the button yet). 

MSG_META_PTR  
This message is sent when the pointer image is over the bounds 
of the game piece whether or not a mouse button has been 
pressed. (After the object has grabbed the mouse events by 
handling MSG_META_START_SELECT, the pointer event is sent 
whenever the mouse pointer is moved.) The piece determines 
whether or not it is being dragged around the screen. This is 
known as a "drag event," and the game piece responds by 
drawing a piece-shaped outline around the mouse pointer. This 
outline will follow the pointer around the screen until the user 
releases the mouse button (causing a 
MSG_META_END_SELECT, below). The outline is first drawn in 
either the MSG_META_START_SELECT or 
MSG_META_DRAG_SELECT handler (whichever is called to 
start the drag event). MSG_META_PTR and 
MSG_META_END_SELECT erase the outline before drawing a 
new one. The game piece will maintain three locations in its 
instance data: Its VI_bounds field maintains its position when 
selected. Its TTP_orig(horiz/vert)Pos fields maintain its 
original position when the game was first started. Its 
TTP_(horiz/vert)Pos fields maintain the current position of the 
outline and where the object would relocate to if the move was 
ended now. If the event is not a drag, the object will not react 
because it is assumed that no mouse button has been pressed 
and therefore the user is taking no action. See Figure 10-5.

MSG_META_END_SELECT  
This message is sent when the user releases a pressed mouse 
button, ending the select-and-drag process. The game piece 
reacts by checking if the location of the pointer is a legal 
position on the game board. (It does this by sending a 
verification message to the TicTacBoard object to make sure 
the proposed new bounds are on the game board.) If the 
position is legal, the game piece moves itself there, erasing any 
leftover outlines (from the drag sequence) and its original 
image on the board. It then causes itself to draw in the new 
location by sending itself a MSG_VIS_DRAW. If the new location 
is not legally on the game board, then the object will reset all 
its instance data and erase any leftover outlines, causing it to 
revert to its location before the select-and-drag sequence began.

![](Art/figure_10-5.png)

**Figure 10-5** Selection of a Game Piece  
_If the user does not have the mouse button pressed, the game piece (TTX1) will 
ignore the pointer. If the button is pressed now, the game piece will receive a 
MSG_META_START_SELECT._

Each of the methods for the above messages is shown in Code Display 10-6.

---
Code Display 10-6 Methods for TicTacPieceClass
~~~
/***********************************************************************
 *
 * MESSAGE:              MSG_PIECE_NEW_GAME for TicTacPieceClass
 *
 * DESCRIPTION:         This message causes the piece to replace itself
 *              to its original position. It is invoked when the
 *              user presses the New Game trigger; the trigger sends
 *              MSG_TICTAC_NEW_GAME to the TicTacBoard object, and
 *              the board object sends this message to each of
 *              the game piece objects.
 * 
 * PARAMETERS:
 *      void ()
 ***********************************************************************/

@method TicTacPieceClass, MSG_PIECE_NEW_GAME {

        /* Set the current (motion) positions to the original positions. */

    pself->TTP_vertPos = pself->TTP_origVertPos;
    pself->TTP_horizPos = pself->TTP_origHorizPos;

        /* Send a MSG_VIS_BOUNDS_CHANGED to ourselves to make
         * sure the old bounds get redrawn. This message will
         * cause an invalidation of the document where the old
         * (passed) bounds were, causing that portion of the
         * window to be redrawn.                                                */

    @call self::MSG_VIS_BOUNDS_CHANGED(pself->VI_bounds.R_bottom,
                        pself->VI_bounds.R_right, pself->VI_bounds.R_top,
                        pself->VI_bounds.R_left);

        /* Set the bounds of the object (VI_bounds) back to
         * their original values. The Rectangle structure
         * contains four fields, each of which must be set.     */

    pself->VI_bounds.R_left = pself->TTP_origHorizPos;
    pself->VI_bounds.R_top = pself->TTP_origVertPos;
    pself->VI_bounds.R_right = (pself->TTP_origHorizPos + PIECE_WIDTH);
    pself->VI_bounds.R_bottom = (pself->TTP_origVertPos + PIECE_HEIGHT);

        /* This method does not need to invoke a MSG_VIS_DRAW
         * because the TicTacBoard object will do that. The
         * piece object will later receive a MSG_VIS_DRAW that
         * will cause the piece to be redrawn back at its
         * original location (the newly set bounds).*/
}

/***********************************************************************
 *
 * MESSAGE:             MSG_VIS_DRAW for TicTacPieceClass
 *
 * DESCRIPTION:         Draw the piece at the current location. If the piece
 *              is a "box," draw a gray square. If the piece is a
 *              "ring," draw a gray circle. This message is received
 *              whenever a portion of the view window becomes invalid;
 *              TicTacView will send a MSG_META_EXPOSED to TicTacBoard,
 *              which will send itself (by default) a MSG_VIS_DRAW.
 *              The MSG_VIS_DRAW will be handled and then will be
 *              passed on to each of the game pieces. Then each piece
 *              (in this handler) will draw itself at its own bounds.
 *
 * PARAMETERS:
 *      void (word drawFlags GStateHandle gstate)
 *
 ***********************************************************************/

@method TicTacPieceClass, MSG_VIS_DRAW {
        /* Set the mode to MM_COPY; this means that the image
         * drawn now will be drawn over whatever is there now.*/

    GrSetMixMode(gstate, MM_COPY);

        /* If the type is TTPT_BOX, set the color to gray and
         * draw a rectangle the size of the object's bounds.
         * Otherwise (since there are just two types), set the
         * color to gray and draw an ellipse of that size.      */

    if (pself->TTP_pieceType == TTPT_BOX) {
        GrSetAreaColor(gstate, CF_INDEX, C_DARK_GRAY, 0, 0);
        GrFillRect(gstate, pself->VI_bounds.R_left, pself->VI_bounds.R_top,
                        pself->VI_bounds.R_right, pself->VI_bounds.R_bottom);

    } else {
        GrSetAreaColor(gstate, CF_INDEX, C_LIGHT_GRAY, 0, 0);
        GrFillEllipse(gstate, pself->VI_bounds.R_left, pself->VI_bounds.R_top,
                        pself->VI_bounds.R_right, pself->VI_bounds.R_bottom);
    }

        /* After handling the message, call the superclass to
         * ensure that no default behavior has been mucked up.
         * This is actually not necessary in this particular case. */
    @callsuper();
}

/***********************************************************************
 * MESSAGE:             MSG_META_START_SELECT for TicTacPieceClass
 *
 * DESCRIPTION:         Grabs the mouse and calls for future pointer events.
 *              When the user clicks in the view, TicTacView will pass
 *              the click event to TicTacBoard. Since TicTacBoardClass
 *              does not intercept the event, VisContentClass passes
 *              it on to its child object currently under the pointer.
 *
 *              When the piece object receives this message, it means
 *              it has been clicked on by the user and the mouse button
 *              is still down. The piece must grab the mouse so that it
 *              gets all future mouse events, and it must request that
 *              all future mouse events be sent to it. This ensures
 *              that if the pointer leaves the object's bounds while
 *              the button is still pressed, the piece object will still
 *              receive all the pointer events (otherwise they would be
 *              sent to whatever object was under the new pointer
 *              position).
 * PARAMETERS:
 *      void (MouseReturnParams *retVal, word xPosition,
 *                      word yPosition, word inputState)
 ***********************************************************************/

@method TicTacPieceClass, MSG_META_START_SELECT {

        /* First grab the gadget exclusive so we're allowed to
         * grab the mouse. Then grab the mouse, so all future
         * pointer events get passed directly to the game piece. */

    @call @visParent::MSG_VIS_TAKE_GADGET_EXCL(oself);
    @call self::MSG_VIS_GRAB_MOUSE();           /* grab mouse */

        /* Finally, return that this particular click
         * event has been processed. If this flag is
         * not returned, the system will send out the
         * click event again.*/

    retVal->flags = MRF_PROCESSED;              /* this event processed */
}

/***********************************************************************
 *
 * MESSAGE:             MSG_META_DRAG_SELECT for TicTacPieceClass
 *
 * DESCRIPTION:         This message is sent to the piece object when the
 *              select button has been pressed and the mouse has been
 *              moved, resulting in a "drag-select" event.
 *              For event processing from the View, see the header
 *              for MSG_META_START_SELECT.
 *
 * PARAMETERS:
 *      void (MouseReturnParams *retVal, word xPosition,
 *                      word yPosition, word inputState)
 ***********************************************************************/

@method TicTacPieceClass, MSG_META_DRAG_SELECT {
    GStateHandle gstate;                /* temporary gstate to draw to */
    WindowHandle win;                   /* window handle of view window */

        /* Start off by setting the flag indicating that
         * the piece is being dragged around the screen. */
    pself->TTP_dragging = TRUE;

        /* Next, get the window handle of the view window.
         * Then, create a new, temporary gstate to draw into
         * for that window.     */

    win = @call TicTacView::MSG_GEN_VIEW_GET_WINDOW();
    gstate = GrCreateState(win);

        /* Now, set the current position of the game piece
         * to be centered on the pointer.*/

    pself->TTP_vertPos = yPosition - (PIECE_HEIGHT/2);
    pself->TTP_horizPos = xPosition - (PIECE_WIDTH/2);

        /* Now, set the drawing mode of the game piece
         * to MM_INVERT to draw a new game piece outline.
         * MM_INVERT is chosen so the outline can be redrawn
         * in invert mode later to erase it and not destroy
         * anything under it.*/

    GrSetMixMode(gstate, MM_INVERT);

        /* Now draw the outline. If the game piece is of type
         * TTPT_BOX, draw a rectangle outline. Otherwise, draw
         * an ellipse outline.*/

    if (pself->TTP_pieceType == TTPT_BOX) {
        GrDrawRect(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                (pself->TTP_horizPos + PIECE_WIDTH),
                                (pself->TTP_vertPos + PIECE_HEIGHT));

    } else {
        GrDrawEllipse(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                (pself->TTP_horizPos + PIECE_WIDTH),
                                (pself->TTP_vertPos + PIECE_HEIGHT));
    }

        /* Next, destroy the temporary gstate. This is important
         * to make sure the gstate does not stay in memory and
         * begin to slow down the system as more and more
         * temporary gstates are created but not destroyed.     */

    GrDestroyState(gstate);

        /* Finally, return that this event has been processed
         * by this method.      */

    retVal->flags = MRF_PROCESSED;
}

/***********************************************************************
 *
 * MESSAGE:             MSG_META_PTR for TicTacPieceClass
 *
 * DESCRIPTION:         This message is received whenever the pointer passes
 *              over this game piece object's bounds (and another
 *              game piece is not sitting directly on top of it).
 *              See MSG_META_START_SELECT for a description of how the event
 *              gets passed from TicTacView to this object.
 *
 *              This message can be either a drag event or a simple
 *              pointer event. If the latter, we want to do nothing
 *              because no mouse button is pressed. If the latter,
 *              we want to execute the same function as MSG_META_DRAG.
 *
 * PARAMETERS:
 *      void (MouseReturnParams *retVal, word xPosition,
 *                      word yPosition, word inputState)
 *
 ***********************************************************************/

@method TicTacPieceClass, MSG_META_PTR {
    GStateHandle gstate;                                /* temporary gstate to draw to */
    WindowHandle win;                                   /* window handle of view window */

        /* First check if this is a drag event. If not, do
         * nothing. If so, then draw a new outline and erase
         * the old outline.             */

    if (pself->TTP_dragging) {

        /* Get the view's window handle and create a
         * temporary gstate for drawing into.*/

        win = @call TicTacView::MSG_GEN_VIEW_GET_WINDOW();
        gstate = GrCreateState(win);

        /* Set the drawing mode of the game piece to 
         * MM_INVERT for outline drawing.       */

        GrSetMixMode(gstate, MM_INVERT);

        /* Erase the old outline by drawing an inverse
         * outline at the old bounds.   */

        if (pself->TTP_pieceType == TTPT_BOX) {
            GrDrawRect(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                        (pself->TTP_horizPos + PIECE_WIDTH),
                                        (pself->TTP_vertPos + PIECE_HEIGHT));

        } else {
            GrDrawEllipse(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                        (pself->TTP_horizPos + PIECE_WIDTH),
                                        (pself->TTP_vertPos + PIECE_HEIGHT));
        }

        /* Now set the current motion position to be
         * centered on the pointer.             */

        pself->TTP_vertPos = yPosition - (PIECE_HEIGHT/2);
        pself->TTP_horizPos = xPosition - (PIECE_WIDTH/2);

        /* Draw the new outline at the current position.*/

        if (pself->TTP_pieceType == TTPT_BOX) {
            GrDrawRect(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                        (pself->TTP_horizPos + PIECE_WIDTH),
                                        (pself->TTP_vertPos + PIECE_HEIGHT));

        } else {
            GrDrawEllipse(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                        (pself->TTP_horizPos + PIECE_WIDTH),
                                        (pself->TTP_vertPos + PIECE_HEIGHT));
        }

        /* Destroy the temporary gstate and return that
         * this event has been processed.*/

        GrDestroyState(gstate);
    }
    retVal->flags = MRF_PROCESSED;
}

/***********************************************************************
 *
 * MESSAGE:             MSG_META_END_SELECT for TicTacPieceClass
 *
 * DESCRIPTION:         This message is received when the selection button has
 *              been released and this game piece had the mouse grab.
 *              All it does is release the gadget exclusive, which will
 *              cause us to end any dragging in progress and release
 *              the mouse.
 *              When we release the gadget exclusive, the UI will then
 *              sent MSG_VIS_LOST_GADGET_EXCL to this piece, which will
 *              tell us to erase the outline and draw the game piece.
 * PARAMETERS:
 *      void (MouseReturnParams *retVal, word xPosition,
 *                      word yPosition, word inputState);
 ***********************************************************************/

@method TicTacPieceClass, MSG_META_END_SELECT {
        /* Release the gadget exclusive, then return that the
         * event has been processed. */
    @call @visParent::MSG_VIS_RELEASE_GADGET_EXCL(oself);
    retVal->flags = MRF_PROCESSED; /* this event processed */
}

/***********************************************************************
 *
 * MESSAGE:             MSG_VIS_LOST_GADGET_EXCL for TicTacPieceClass
 *
 * DESCRIPTION:         This message is received when the piece lots go of the
 *              gadget exclusive (see MSG_META_END_SELECT, above).
 *              It first checks to see if the new, proposed bounds are
 *              on the game board. If the bounds are valid, then
 *              it sets the objects VI_bounds field to the new values
 *              and causes the object to erase its original drawing
 *              and draw itself at its new bounds. If the bounds are
 *              not on the game board, it will retain the original bounds
 *              and redraw using them.
 *
 * PARAMETERS:
 *      void ()
 *
 ***********************************************************************/

@method TicTacPieceClass, MSG_VIS_LOST_GADGET_EXCL {
    WindowHandle win;                                   /* window handle of view window */
    GStateHandle gstate;                                /* temporary gstate to draw to */

        /* First check if the piece was being dragged.
         * If not, we don't have to do anything.*/
    if (pself->TTP_dragging) {

        /* Get the window handle of the view window and
         * create a temporary gstate for it to draw to. */

        win = @call TicTacView::MSG_GEN_VIEW_GET_WINDOW();
        gstate = GrCreateState(win);

        /* Set the mode for drawing the outline.*/

        GrSetMixMode(gstate, MM_INVERT);

        /* If the game piece type is TTPT_BOX, draw a rectangle
         * outline. Otherwise draw an ellipse outline.  */

        if (pself->TTP_pieceType == TTPT_BOX) {
            GrDrawRect(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                ((pself->TTP_horizPos) + PIECE_WIDTH),
                                ((pself->TTP_vertPos) + PIECE_HEIGHT));

        } else {
            GrDrawEllipse(gstate, pself->TTP_horizPos, pself->TTP_vertPos,
                                ((pself->TTP_horizPos) + PIECE_WIDTH),
                                ((pself->TTP_vertPos) + PIECE_HEIGHT));
        }

        /* Check to see if the new bounds are on the game
         * board. If they are, set the object's bounds to the
         * new values. If they are not, retain the original
         * values and set the values to those last stored.      */

        if (@call TicTacBoard::MSG_TICTAC_VALIDATE_BOUNDS(
                                        ((pself->TTP_vertPos) + PIECE_HEIGHT),
                                        ((pself->TTP_horizPos) + PIECE_WIDTH),
                                        pself->TTP_vertPos,
                                        pself->TTP_horizPos)) {

        /* Invalidate the original drawing of the game piece.
         * Send the VI_bounds rectangle as the parameters
         * because they have not been changed since the
         * START_SELECT. This message is the equivalent of
         * calling GrInvalRect() with the same bounds.  */

            @call self::MSG_VIS_BOUNDS_CHANGED(pself->VI_bounds.R_bottom,
                                        pself->VI_bounds.R_right,
                                        pself->VI_bounds.R_top,
                                        pself->VI_bounds.R_left);

        /* Now set the current position to be centered
         * on the pointer image.        */

            pself->TTP_vertPos = yPosition - (PIECE_HEIGHT/2);
            pself->TTP_horizPos = xPosition - (PIECE_WIDTH/2);

        /* Set the game piece object's bounds to
         * the new coordinates.         */

            pself->VI_bounds.R_left = pself->TTP_horizPos;
            pself->VI_bounds.R_right = (pself->TTP_horizPos) + PIECE_WIDTH;
            pself->VI_bounds.R_top = pself->TTP_vertPos;
            pself->VI_bounds.R_bottom = (pself->TTP_vertPos) + PIECE_HEIGHT;

        } else {

        /* If the bounds are not on the game board, then reset
         * the current positions to be the original bounds. */

            pself->TTP_horizPos = pself->VI_bounds.R_left;
            pself->TTP_vertPos = pself->VI_bounds.R_top;
        }

        /* Now, the game piece must draw itself at its newly-
         * set bounds (will draw itself over its original
         * picture if the new bounds were invalid).     */

        @call self::MSG_VIS_DRAW(0, gstate);

        /* Destroy the temporary gstate used for drawing. */

        GrDestroyState(gstate);

        /* Finally, clear the dragging flag to indicate that
         * no drag event is in progress. */

        pself->TTP_dragging = FALSE;
    }

        /* Release the mouse grab now that the move has
         * finished. Other objects in the view (other game
         * pieces, for example) may now receive pointer,
         * select, and drag events.     */

    @call self::MSG_VIS_RELEASE_MOUSE();
}
~~~

[General Change Notification](cgcn.md) <-- &nbsp;&nbsp; [table of contents](../concepts.md) &nbsp;&nbsp; --> [Input](cinput.md)
