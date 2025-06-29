## 3.3 Structures G-G

----------
#### GadgetSizeHintArgs
    GadgetSizeHintArgs          struct
        GSHA_width          SpecWidth <>        ;Width of the composite
        GSHA_height         SpecHeight <>       ;Height of each child
    GadgetSizeHintArgs          ends

**Library:** Objects/genC.def

----------
#### GCCFeatures
    GCCFeatures     record
        GCCF_HORIZONTAL_GUIDES          :1
        GCCF_VERTICAL_GUIDES            :1
    GCCFeatures     end

**Library:** ruler.def

----------
#### GCMIcon
    GCMIcon etype   byte, 0
        GCMI_NONE       enum GCMIcon
        GCMI_EXIT       enum GCMIcon
        GCMI_HELP       enum GCMIcon

**Library:** Objects/genC.def

----------
#### GCM_info
    GCM_info    etype   word, 0, 2
        GCMI_MIN_X      enum GCM_info   ;min x (left side bearing)
        GCMI_MIN_Y      enum GCM_info   ;min y (descent)
        GCMI_MAX_X      enum GCM_info   ;max x
        GCMI_MAX_Y      enum GCM_info   ;max y (ascent)

**Library:** font.def

----------
#### GCNDriveChangeNotificationType
    GCNDriveChangeNotificationType          etype word
        GCNDCNT_CREATED         enum GCNDriveChangeNotificationType
        GCNDCNT_DESTROYED       enum GCNDriveChangeNotificationType

**Library:** gcnlist.def

----------
#### GCNExpressMenuNotificationType
    GCNExpressMenuNotificationType          etype word
        GCNEMNT_CREATED         enum GCNExpressMenuNotificationType
        GCNEMNT_DESTROYED       enum GCNExpressMenuNotificationType

**Library:** gcnlist.def

----------
#### GCNListBlockHeader
    GCNListBlockHeader      struct
        GCNLBH_lmemHeader           LMemBlockHeader
        GCNLBH_listOfLists          lptr.GCNListOfListsHeader
    GCNListBlockHeader      ends

This structure begins a kernel's GCN list block.

**Library:** gcnlist.def

----------
#### GCNListElement
    GCNListElement      struct
        GCNLE_item          optr
    GCNListElement      ends

This structure stores an element within a GCN list.

**Library:** Objects/metaC.def

----------
#### GCNListHeader
    GCNListHeader       struct
        GCNLH_meta              ChunkArrayHeader
        GCNLH_statusEvent       hptr
        GCNLH_statusData        hptr
        GCNLH_statusCount       word
    GCNListHeader       ends

This structure defines a single GCN list (which resides in a chunk).

*GCNLH_statusEvent* stores a copy of the last notification event sent to this 
list via **GCNListSendStatus**. This event will be sent automatically to any 
object adding itself to the list. (This functionality is not yet used.)

*GCNLH_statusData* stores a copy of the extra data block, if any, passed in the 
above status event. This data block must be sharable, & have a reference 
count.

*GCNLH_statusCount* is incremented each time status is set for this list. This 
status count is used in the UI to avoid setting a status of NULL between 
changes in the target. If GCNLSF_IGNORE_IF_STATUS_TRANSITIONING is 
set in a `Send' request, the GenApplication object will only set a NULL status 
if no status updates have been made after the time it takes to clear the 
process's queue when an object loses the target.

**Library:** Objects/metaC.def

----------
#### GCNListMessageParams
    GCNListMessageParams            struct
        GCNLMP_ID               GCNListType
        GCNLMP_block            hptr.GCNDataBlockHeader
        GCNLMP_event            hptr
        GCNLMP_flags            GCNListSendFlags
    GCNListMessageParams            ends

*GCNLMP_ID* stores a list identifier - a combination of a **ManufacturerID** 
and a Manufacturer list type.

*GCNLMP_block* stores the handle of the extra data block, if used. (If there is 
no extra data block, this should be 0.) Blocks of this type must have a 
reference count, which may be initialized with **MemInitRefCount** and 
incremented for any new usage with **MemIncRefCount**. Methods in which 
the blocks are passed are considered a new usage and must have **MetaClass** 
handlers which call **MemDecRefCount**. Current messages supporting this 
behavior:  
MSG_META_NOTIFY_WITH_DATA_BLOCK  
MSG_NOTIFY_FILE_CHANGE.

*GCNLMP_event* stores a classed event to send to the list.

*GCNLMP_flags* stores the flags to pass on to **GCNListSend** or a similar 
primitive routine.

**Library:** Objects/metaC.def

----------
#### GCNListOfListsElement
    GCNListOfListsElement           struct
        GCNLOLE_ID              GCNListType
        GCNLOLE_list            lptr.GCNListHeader
    GCNListOfListsElement           ends

This structure defines an element in a GCN list of lists.

**Library:** Objects/metaC.def

----------
#### GCNListOfListsHeader
    GCNListOfListsHeader            struct
        GCNLOL_meta             ChunkArrayHeader
        GCNLH_data              label GCNListOfListsElement
    GCNListOfListsHeader            ends

This structure starts a GCN lists of lists (and resides in a chunk). The label 
marks the start of multiple **GCNListOfListsElement** structures.

**Library:** Objects/metaC.def

----------
#### GCNListParams
    GCNListParams       struct
        GCNLP_ID            GCNListType
        GCNLP_optr          optr
    GCNListParams       ends

*GCNLP_ID* stores the list identifier, which consists of a **ManufacturerID** 
and its associated Manufacturer list type.

*GCNLP_optr* stores the optr of the object to be added or removed from the list.

**Library:** Objects/metaC.def

----------
#### GCNListSendFlags
    GCNListSendFlags        record
        GCNLSF_SET_STATUS                       :1
        GCNLSF_IGNORE_IF_STATUS_TRANSITIONING   :1
        GCNSLF_FORCE_QUEUE                      :1
                                                :13
    GCNListSendFlags        end

GCNLSF_SET_STATUS  
During a **GCNListSend**, this flag additionally saves the message as the list's 
current "status". This "status" message will be automatically sent to any 
object adding itself to the list at a later point in time.

GCNLSF_IGNORE_IF_STATUS_TRANSITIONING  
This flag is an optimization bit used to avoid a lull in status when 
transitioning between two different sources. This case may arise when the 
source is the current target object and one has just lost and another may soon 
gain the exclusive. (The bit should be set only when sending the 
"null"/"lost"/"not selected" status, as this is the event that should be tossed if 
another non-null status comes along shortly.) 

Implementation is *not* provided by the kernel primitive routines, which 
ignore this bit, but may be provided by objects managing their own GCN lists. 
GenApplication objects respond to this bit by delaying the request until after 
the UI and application queues have been cleared; then they only set the 
status as indicated if no other status has been set since the first request. 
Other objects may use their own logic to implement this optimization as is 
appropriate. Mechanisms which can not tolerate the delayed nature of this 
optimization, or require that all changes be registered, should not pass this 
bit set. 

GCNLSF_FORCE_QUEUE  
This flag informs **GCNListSend** to place the message on the event queue for 
the destination, even if the destination is run by the same thread as that 
sending the message.

**Library:** Objects/metaC.def

----------
#### GCNListType
    GCNListType     struct
        GCNLT_manuf         ManufacturerID
        GCNLT_type          word
    GCNListType     ends

This structure defines a specific GCN list type. A GCN list type consists of a 
manufacturer ID describing each unique manufacturer and a specific list 
type defined for that manufactuer ID.

**Library:** Objects/metaC.def

----------
#### GCNListTypeFlags
    GCNListTypeFlags        record
        ; high bits hold the list type.
                                :15
        GCNLTF_SAVE_TO_STATE    :1  ; set to indicate that list should be 
                                    ; saved to state.
    GCNListTypeFlags        end

**Library:** Objects/metaC.def

----------
#### GCNShutdownControlType
    GCNShutdownControlType          etype word
        GCNSCT_SUSPEND          enum GCNShutdownControlType
        ; Task-switcher wishes to suspend the system.
        GCNSCT_SHUTDOWN         enum GCNShutdownControlType
        ; Task-switcher or other entity wishes to shut the system down to state.
        GCNSCT_UNSUSPEND        enum GCNShutdownControlType
        ; System has been unsuspended. No acknowledgement required.

**Library:** gcnlist.def

----------
#### GCNStandardListType
    GCNStandardListType         etype word, 0, 2
        GCNSLT_FILE_SYSTEM          enum GCNStandardListType
        ; This notification is sent out when the file system changes.

**Library:** gcnlist.def

----------
#### GDCFeatures
    GDCFeatures     record
        GDCF_NEW                    :1  ;replaced with switch documents in
                                        ;transparent mode
        GDCF_OPEN_CLOSE             :1
        GDCF_QUICK_BACKUP           :1
        GDCF_SAVE                   :1
        GDCF_SAVE_AS                :1
        GDCF_COPY                   :1
        GDCF_EXPORT                 :1
        GDCF_REVERT                 :1
        GDCF_RENAME                 :1  ;requires an auto-savable file
        GDCF_EDIT_USER_NOTES        :1
        GDCF_SET_TYPE               :1
        GDCF_SET_PASSWORD           :1
        GDCF_SAVE_AS_TEMPLATE       :1
        GDCF_SET_EMPTY_DOCUMENT     :1
        GDCF_SET_DEFAULT_DOCUMENT   :1
    GDCFeatures     end

**Library:** Objects/gDocCtrl.def

----------
#### GDCTask
    GDCTask etype byte
        GDCT_NONE               enum GDCTask
        GDCT_NEW                enum GDCTask
        GDCT_OPEN               enum GDCTask
        GDCT_USE_TEMPLATE       enum GDCTask
        GDCT_SAVE_AS            enum GDCTask
        GDCT_COPY_TO            enum GDCTask
        GDCT_DIALOG             enum GDCTask
        GDCT_TYPE               enum GDCTask
        GDCT_PASSWORD           enum GDCTask

**Library:** gDocCtrl.def

----------
#### GDCToolboxFeatures
    GDCToolboxFeatures      record
        GDCTF_NEW_EMPTY         :1
        GDCTF_USE_TEMPLATE      :1
        GDCTF_OPEN              :1
        GDCTF_CLOSE             :1
        GDCTF_SAVE              :1
        GDCTF_QUICK_BACKUP      :1
    GDCToolboxFeatures      end

**Library:** Objects/gDocCtrl.def

----------
#### GDF_saved
    GDF_saved       struct
        GDFS_nChars     word        ; Number of characters to draw
        GDFS_drawPos    PointWBFixed ; X/Y position to draw at
        GDFS_baseline   WBFixed     ; Baseline for text
        GDFS_limit      word        ; Limit for underline or strike-through
        GDFS_flags      HyphenFlags
        align           word
    GDF_saved       ends 

This structure stores information about a graphics string and is used in the 
**GrDrawTextString** operation.

**Library:** text.def

----------
#### GDF_vars
    GDF_vars        struct
        GDFV_saved              GDF_saved
        GDFV_styleCallback      fptr.far
        GDFV_textOffset         dword
        GDFV_other              dword
        GDFV_textPointer        dword
        align                   word
    GDF_vars        ends

This structure is passed to **GrDrawTextField**.

*GDFV_saved* stores the information to save for this graphics strings.

*GDFV_styleCallback* stores the callback routine for style changes.

**Callback Routine Specifications:**  
**Passed:**  
ss:bp - GDF_vars  
bx:di - TextAttr buffer to fill in  
si - Offset into the field.  
cx - Zero if this is the first call  
**Return:**  
Buffer pointed at by **bx:di** filled in.  
cx - Number of characters in this run  
ds:si - Pointer to the text at offset si in the field.  
**May Destroy:**  
Nothing

*GDFV_textOffset* stores the offset to the start of the text to draw.

*GDFV_other* stores application specific data.

*GDFV_textPointer* stores the current text pointer (set by callback).

**Library:** text.def

----------
#### GDICFeatures
    GDICFeatures        record
        GDCF_OVERLAPPING_MAXIMIZED      :1
        GDCF_TILE                       :1
        GDCF_DISPLAY_LIST               :1
    GDICFeatures        end

**Library:** Objects/gDCtrlC.def

----------
#### GDICToolboxFeatures
    GDICToolboxFeatures         record
        GDCTF_OVERLAPPING_MAXIMIZED     :1
        GDCTF_TILE                      :1
        GDCTF_DISPLAY_LIST              :1
    GDICToolboxFeatures         end

**Library:** Objects/gDCtrlC.def

----------
#### GECFeatures
    GECFeatures     record
        GECF_UNDO           :1
        GECF_CUT            :1
        GECF_COPY           :1
        GECF_PASTE          :1
        GECF_SELECT_ALL     :1
        GECF_DELETE         :1
    GECFeatures     end

**Library:** Objects/gEditCC.def

----------
#### GECToolboxFeatures
    GECToolboxFeatures      record
        GECTF_UNDO              :1
        GECTF_CUT               :1
        GECTF_COPY              :1
        GECTF_PASTE             :1
        GECTF_SELECT_ALL        :1
        GECTF_DELETE            :1
    GECToolboxFeatures      end

**Library:** Objects/gEditCC.def

----------
#### GenAppDoDialogParams
    GenAppDoDialogParams            struct
        GADDP_dialog        StandardDialogParams
        GADDP_finishOD      optr                ; OD to send method to.
        GADDP_message       word                ; method to send.
    GenAppDoDialogParams            ends

**Library:** Objects/gAppC.def

----------
#### GenAppIACPConnection
    GenAppIACPConnection    struc
        GAIACPC_connection      IACPConnection
        ; The IACP connection
        GAIACPC_appMode         word
        ; The type of connection - MSG_GEN_PROCESS_OPEN_APPLICATION or engine
        ; mode message)
    GenAppIACPConnection    ends

**Library:** 

----------
#### GenAppUpdateFeaturesParams
    GenAppUpdateFeaturesParams              struct
        GAUFP_featuresOn        word
        GAUFP_featuresChanged   word
        GAUFP_level             UIInterfaceLevel
        GAUFP_oldLevel          UIInterfaceLevel
        GAUFP_appOpening        word
        GAUFP_table             fptr    ; table of fptrs to GenAppUsabilityTuple
        GAUFP_tableLength       word
        GAUFP_levelTable        fptr.GenAppUsabilityTuple
        GAUFP_reparentObject    optr
        GAUFP_unReparentObject  optr
    GenAppUpdateFeaturesParams  ends

**Library:** Objects/gAppC.def

----------
#### GenAppUsabilityCommand
    GenAppUsabilityCommand          etype byte
        GAUC_USABILITY              enum GenAppUsabilityCommand
        GAUC_RECALC_CONTROLLER      enum GenAppUsabilityCommand
        GAUC_REPARENT               enum GenAppUsabilityCommand
        GAUC_POPUP                  enum GenAppUsabilityCommand
        GAUC_TOOLBAR                enum GenAppUsabilityCommand
        GAUC_RESTART                enum GenAppUsabilityCommand

GAUC_USABILITY  
Indicates that the controller should be usable if the feature is ON. (This is the 
default behavior.)

GAUC_RECALC_CONTROLLER  
Indicates that the controller needs to have its features recalculated if the 
feature bit this table represents changes.

GAUC_REPARENT  
Indicates that the controller should be moved to the *GAUFP_reparentObject*.

GAUC_POPUP  
Indicates that the controller should be made a popup menu if the feature is 
ON (unless reverse is set).

GAUC_TOOLBAR  
Indicates that the controller is a GenBoolean that corresponds to a toolbar 
state. Turning the feature off or on forces the GenBoolean to send an apply 
in addition to the normal behavior.

GAUC_RESTART  
Indicates that this generic object needs to be restarted by setting it 
not-usable and then setting it usable.

**Library:** gAppC.def

----------
#### GenAppUsabilityTuple
    GenAppUsabilityTuple            struct
        GAUT_flags              GenAppUsabilityTupleFlags
        GAUT_objChunk           lptr
        GAUT_objResId           word
    GenAppUsabilityTuple        ends

**Library:** Objects/gAppC.def

----------
#### GenAppUsabilityTupleFlags
    GenAppUsabilityTupleFlags               record
                                    :2
        GAUTF_END_OF_LIST           :1
        GAUTF_OFF_IF_BIT_ON         :1
        GAUTF_COMMAND               GenAppUsabilityCommand:4
    GenAppUsabilityTupleFlags               end

**Library:** Objects/gAppC.def

----------
#### GenAttrs
    GenAttrs        record
        GA_SIGNAL_INTERACTION_COMPLETE          :1
        GA_INITIATES_BUSY_STATE                 :1
        GA_INITIATES_INPUT_HOLD_UP              :1
        GA_INITIATES_INPUT_IGNORE               :1
        GA_READ_ONLY                            :1
        GA_KBD_SEARCH_PATH                      :1
        GA_TARGETABLE                           :1
        GA_NOTIFY_VISIBILITY                    :1
    GenAttrs        end

GA_SIGNAL_INTERACTION_COMPLETE  
This flag is set to indicate that this GenTrigger completes user 
interaction with the associated GenInteraction when activated. 
This causes a MSG_GEN_GUP_INTERACTION_COMMAND with 
IC_INTERACTION_COMPLETE to be sent to the GenTrigger 
itself (eventually making its way up to the associated 
GenInteraction) after the trigger's action message is sent out. 
The specific UI (in the 
MSG_GEN_GUP_INTERACTION_COMMAND handler) will then 
determine whether this dialog should be dismissed or not.

This should be set for any ATTR_GEN_TRIGGER_INTERACTION_COMMAND 
trigger with IC_APPLY, IC_OK, IC_YES, IC_NO, or IC_STOP. 
(This flag should not be set for IC_RESET triggers, as their 
usefulness depends on the dialog staying on-screen after their 
activation.) GA_SIGNAL_INTERACTION_COMPLETE should 
also be set for any other HINT_SEEK_REPLY_BAR triggers that 
should dismiss the dialog after usage.

This flag should not be used for GIA_INITIATED_VIA_USER_DO_DIALOG 
GenInteractions as the command triggers in those dialogs, by 
definition, signal interaction completion.

GA_INITIATES_BUSY_STATE  
Set for gadgets whose invocation starts a long enough 
operation that we'd like to change the cursor to show busy. 
Results in a MSG_GEN_APPLICATION_MARK_BUSY being sent 
to the app object, followed by a 
MSG_GEN_APPLICATION_MARK_NOT_BUSY being sent to the 
same object but via the application's queue.

GA_INITIATES_INPUT_HOLD_UP  
Set for gadgets whose invocation results in the application 
thread modifying the UI gadgtry slightly (typically enabling 
and disabling options). This flag causes input to be held up 
until the application has completed whatever its response is, so 
that the user cannot click twice on something that the app will 
disable after processing the first click. 

Note: This functions stops input from being processed for all applications, so 
when using this bit, be sure that the gadget's application 
method handler is quick, or at least does not perform any 
prolonged operation.

Initiating a trigger with this flag results in a 
MSG_GEN_APPLICATION_HOLD_UP_INPUT being sent to the 
application object, followed by a 
MSG_GEN_APPLICATION_RESUME_INPUT being sent to the 
same object but via the application's queue.

GA_INITIATES_INPUT_IGNORE  
This flags is set for gadgets whose invocation starts a long 
enough operation that we want to change the cursor to show 
that the app is busy and cannot take input. This flag causes the 
application to enter a modal state even if there isn't an 
application-modal dialog box up (i.e. any activity is eaten with 
a beep).

Initiating a trigger with this flag results in a 
MSG_GEN_APPLICATION_IGNORE_INPUT being sent to the 
app object, followed by a 
MSG_GEN_APPLICATION_ACCEPT_INPUT being sent to the 
same object but via the app's queue.

GA_READ_ONLY  
If set, the generic object is presumed to be a read-only version 
of the gadget (i.e. a text object that is not editable, a scrolling 
list whose items cannot be selected, a non-editable GenRange, 
without up/down arrows, etc. 

GA_KBD_SEARCH_PATH  
Set if there is a reason to look for keyboard accelerators along 
this section of the generic tree.

GA_TARGETABLE  
Set if this object is a target of some sort and can receive the 
"Target" exclusive within its target level. If set, most specific 
UI's will automatically grab the Target for the object whenever 
the user interacts with it in some way, such as clicking on it.

GA_NOTIFY_VISIBILITY  
Set if this object should send notification when it becomes 
visible and not visible. See the documentation with 
ATTR_GEN_VISIBILITY_DATA for more details.

**Library:** Objects/genC.def

----------
#### GenBranchInfo
    GenBranchInfo       record
        GBI_USABLE              :1
        GBI_BRANCH_MINIMIZED    :1
                                :14
    GenBranchInfo       end

GBI_USABLE  
This bit is cleared if any generic parent found is not usable.

GBI_BRANCH_MINIMIZED  
This bit is set if the object is within a branch which the specific UI has set the 
SA_BRANCH_MINIMIZED in. (This flag is only valid if GBI_USABLE is set.)

**Library:** Objects/visC.def

----------
#### GenControlBuildFlags
    GenControlBuildFlags            record
        GCBF_SUSPEND_ON_APPLY                               :1
        GCBF_USE_GEN_DESTROY                                :1
        GCBF_SPECIFIC_UI                                    :1
        GCBF_CUSTOM_ENABLE_DISABLE                          :1
        GCBF_ALWAYS_UPDATE                                  :1
        GCBF_EXPAND_TOOL_WIDTH_TO_FIT_PARENT                :1
        GCBF_ALWAYS_INTERACTABLE                            :1
        GCBF_ALWAYS_ON_GCN_LIST                             :1
        GCBF_MANUALLY_REMOVE_FROM_ACTIVE_LIST               :1
        GCBF_IS_ON_ACTIVE_LIST                              :1
        GCBF_IS_ON_START_LOAD_OPTIONS                       :1
        GCBF_NOT_REQUIRED_TO_BE_ON_SELF_LOAD_OPTIONS_LIST   :1
        GCBF_DO_NOT_DESTROY_CHILDREN_WHEN_CLOSED            :1
                                                            :3
    GenControlBuildFlags            end

GCBF_SUSPEND_ON_APPLY  
This flag indicates that the object should be sent 
MSG_META_{SUSPEND,UNSUSPEND} at the beginning and end 
of MSG_GEN_APPLY.

GCBF_USE_GEN_DESTROY  
This flag specifies that unused objects cannot be destroyed 
using **LMemFree**.

GCBF_SPECIFIC_UI  
This flag specifies that the controller is at least partly 
implemented in the specific UI and therefore needs special 
treatment.

GCBF_CUSTOM_ENABLE_DISABLE  
This flag specifies that the GenControl should not set itself 
enabled or disabled based on 
MSG_GEN_CONTROL_ENABLE_DISABLE.

Note: controllers that have this bit set and contain keyboard 
shortcuts must be marked GS_ENABLED initially.

GCBF_ALWAYS_UPDATE  
This flag forces MSG_GEN_CONTROL_UPDATE_UI to always be 
sent, even if the data block is 0.

GCBF_EXPAND_TOOL_WIDTH_TO_FIT_PARENT  
This flag expands the width of the tool control so that children 
can take advantage of extra space.

GCBF_ALWAYS_INTERACTABLE  
This flag indicates that the controller has set its interactable 
flag; this forces the controller to remain on its GCN lists, even 
if no part of it is visible. This flag must be set in conjunction 
with GCBF_IS_ON_ACTIVE_LIST.

GCBF_ALWAYS_ON_GCN_LIST  
This flag specifies that the controller should remain on the 
specified GCN lists at all times. This flag must be set in 
conjunction with GCBF_IS_ON_ACTIVE_LIST.

GCBF_MANUALLY_REMOVE_FROM_ACTIVE_LIST  
This flag specifies that the controller should not be removed 
from the active list in the MSG_META_DETACH handler.

GCBF_IS_ON_ACTIVE_LIST  
This flag specifies that this controller is on the 
MGCNLT_ACTIVE_LIST.

GCBF_IS_ON_START_LOAD_OPTIONS_LIST  
This flag is set if the controller is on the 
GAGCNLT_STARTUP_LOAD_OPTIONS list.

GCBF_NOT_REQUIRED_TO_BE_ON_SELF_LOAD_OPTIONS_LIST  
This flag is set if the controller does not have to be on the 
GAGCNLT_SELF_LOAD_OPTIONS GCN list.

GCBF_DO_NOT_DESTROY_CHILDREN_WHEN_CLOSED  
This controller's children will not be discarded when it is 
closed.

**Library:** Objects/gCtrlC.def

----------
#### GenControlBuildInfo
    GenControlBuildInfo         struct
        ;
        ; General information
        ;
        GCBI_flags              GenControlBuildFlags
        GCBI_initFileKey        fptr.char           ;key to store data in
        GCBI_gcnList            fptr.GCNListType    ;list of gcn lists to add to
        GCBI_gcnCount           word                ;size of gcn list
        GCBI_notificationList   fptr.NotificationType ;list of supported types
        GCBI_notificationCount  word
        GCBI_controllerName     optr
        ;
        ; Information for building normal visual representation
        ;
        GCBI_dupBlock           hptr                ;handle of UI resource to
                                                    ;duplicate or 0 for none
        GCBI_childList          fptr.GenControlChildInfo
        GCBI_childCount         word                ;number of children to add
        GCBI_featuresList       fptr.GenControlFeaturesInfo
        GCBI_featuresCount      word                ;size of features list
        GCBI_features           word                ;bitmask for default features
        ;
        ;Information for building toolbox
        ;
        GCBI_toolBlock          hptr                ;handle of UI resource 
                                                    ;containing tools
        GCBI_toolList           fptr.GenControlChildInfo
        GCBI_toolCount          word                ;number of tools to add
        GCBI_toolFeaturesList   fptr.GenControlFeaturesInfo
        GCBI_toolFeaturesCount  word                ;size of tools features list
        GCBI_toolFeatures       word                ;bitmask for default features
        GCBI_helpContext        fptr.char           ;if non-zero then add 
                                                    ;ATTR_GEN_HELP_CONTEXT with
                                                    ;this string being the context
        GCBI_reserved   byte 8 dup (0)              ;reserved for future expansion
    GenControlBuildInfo         ends

**Library:** Objects/gCtrlC.def

----------
#### GenControlChildFlags
    GenControlChildFlags            record
        GCCF_NOTIFY_WHEN_ADDING         :1
        GCCF_ALWAYS_ADD                 :1
        GCCF_IS_DIRECTLY_A_FEATURE      :1
    GenControlChildFlags            end

**Library:** Objects/gCtrlC.def

----------
#### GenControlChildInfo
    GenControlChildInfo             struct
        GCCI_object             lptr
        GCCI_featureMask        word
        GCCI_flags              GenControlChildFlags
    GenControlChildInfo             ends

*GCCI_featureMask* stores a bitmask of the feature that this object exhibits or 
a bitmask of the combination of tools that compose this object.

**Library:** Objects/gCtrlC.def

----------
#### GenControlFeatureFlags
    GenControlFeatureFlags          record
                                :8
    GenControlFeatureFlags          end

**Library:** Objects/gCtrlC.def

----------
#### GenControlFeaturesInfo
    GenControlFeaturesInfo          struct
        GCFI_object             lptr
        GCFI_name               optr
        GCFI_flags              GenControlFeatureFlags
    GenControlFeaturesInfo          ends

*GCFI_object* stores the lptr of the controller's associated object.

*GCFI_name* stores an optr to a reference chunk. This chunk contains a 
reference to the name of the feature (if the feature is allowed be changed).

**Library:** Objects/gCtrlC.def

----------
#### GenControlInteractableFlags
    GenControlInteractableFlags             record
        GCIF_CONTROLLER     :1  ;Controller object itself is interactable and
                                ;may need to be enabled/disabled
                            :13
        GCIF_TOOLBOX_UI     :1  ;Toolbox UI is interactable
        GCIF_NORMAL_UI      :1  ;Normal UI is interactable
    GenControlInteractableFlags             end

**Library:** Objects/gCtlC.def

----------
#### GenControlScalableUICommand
    GenControlScalableUICommand             etype byte
        CSUIC_SET_NORMAL_FEATURES_IF_APP_FEATURE_ON enum GenControlScalableUICommand
        ; if (GCSUIE_appFeature is ON)
        ;   menu features = GCSUIE_newFeatures
        ;
        GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_FEATURE_ON enum 
        GenControlScalableUICommand
        ; if (GCSUIE_appFeature is ON)
        ;   tool features = GCSUIE_newFeatures
        ;
        GCSUIC_SET_NORMAL_FEATURES_IF_APP_FEATURE_OFF enum 
        GenControlScalableUICommand
        ; if (GCSUIE_appFeature is OFF)
        ;   menu features = GCSUIE_newFeatures
        ;
        GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_FEATURE_OFF enum 
        GenControlScalableUICommand
        ; if (GCSUIE_appFeature is OFF)
        ;   tool features = GCSUIE_newFeatures
        ;
        GCSUIC_SET_NORMAL_FEATURES_IF_APP_LEVEL enum GenControlScalableUICommand
        ; if (app level >= GCSUIE_appFeature)
        ;   menu features = GCSUIE_newFeatures
        ;
        GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_LEVEL enum GenControlScalableUICommand
        ; if (app level >= GCSUIE_appFeature)
        ;   tool features = GCSUIE_newFeatures
        ;
        GCSUIC_ADD_NORMAL_FEATURES_IF_APP_FEATURE_ON enum 
        GenControlScalableUICommand
        ; if (GCSUIE_appFeature is ON)
        ;   menu features |= GCSUIE_newFeatures
        ;
        GCSUIC_ADD_TOOLBOX_FEATURES_IF_APP_FEATURE_ON enum 
        GenControlScalableUICommand
        ; if (GCSUIE_appFeature is ON)
        ;   tool features |= GCSUIE_newFeatures

This type is passed with the **GenControlScalableUIEntry** structure. 

GCSUIC_SET_NORMAL_FEATURES_IF_APP_FEATURE_ON  
If the particular feature within *GCSUIE_appFeature* is set, 
then the normal (menu) features within *GCSUIE_newFeatures* 
are set absolutely. If you would rather have these new features 
**added to already existing features, use** 
GCSUIC_ADD_NORMAL_FEATURES_IF_APP_FEATURE_ON.

GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_FEATURE_ON  
If the particular feature within *GCSUIE_appFeature* is set, 
then the toolbox features within *GCSUIE_newFeatures* are set 
absolutely. If you would rather have these new features added 
to already existing features, use 
GCSUIC_ADD_TOOLBOX_FEATURES_IF_APP_FEATURE_ON.

GCSUIC_SET_NORMAL_FEATURES_IF_APP_FEATURE_OFF  
If the particular feature within *GCSUIE_appFeature* is clear, 
then the normal (menu) features within *GCSUIE_newFeatures* 
are set absolutely.

GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_FEATURE_OFF  
If the particular feature within *GCSUIE_appFeature* is clear, 
then the toolbox features within *GCSUIE_newFeatures* are set 
absolutely.

GCSUIC_SET_NORMAL_FEATURES_IF_APP_LEVEL  
If (app level >= *GCSUIE_appFeature*) then the normal (menu) 
features within *GCSUIE_newFeatures* are set absolutely.

GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_LEVEL  
If (app level >= *GCSUIE_appFeature*) then the toolbox 
features within *GCSUIE_newFeatures* are set absolutely.

GCSUIC_ADD_NORMAL_FEATURES_IF_APP_FEATURE_ON  
If the particular feature within *GCSUIE_appFeature* is set, 
then the normal (menu) features within *GCSUIE_newFeatures* 
are added to any already existing features. If you would rather 
have these new features set absolutely, use 
GCSUIC_SET_NORMAL_FEATURES_IF_APP_FEATURE_ON.

GCSUIC_ADD_TOOLBOX_FEATURES_IF_APP_FEATURE_ON  
If the particular feature within *GCSUIE_appFeature* is set, 
then the toolbox features within *GCSUIE_newFeatures* are 
added to any already existing features. If you would rather 
have these new features set absolutely, use 
GCSUIC_SET_TOOLBOX_FEATURES_IF_APP_FEATURE_ON.

**Library:** gCtlC.def

----------
#### GenControlScalableUIEntry
    GenControlScalableUIEntry               struct
        GCSUIE_command          GenControlScalableUICommand
        GCSUIE_appFeature       word            ;feature bit to check.
        GCSUIE_newFeatures      word            ;new features bits to use.
    GenControlScalableUIEntry               ends

**Library:** Objects/gCtrlC.def

----------
#### GenControlScanInfo
    GenControlScanInfo          struct
        GCSI_userAdded              word
        GCSI_userRemoved            word
        GCSI_appRequired            word
        GCSI_appProhibited          word
    GenControlScanInfo          ends

**Library:** Objects/gCtrlC.def

----------
#### GenControlStatusChange
    GenControlStatusChange          record
                                                :13
        GCSF_HIGHLIGHTED_TOOLGROUP_SELECTED     :1
        GCSF_TOOLBOX_FEATURES_CHANGED           :1
        GCSF_NORMAL_FEATURES_CHANGED            :1
    GenControlStatusChange          end

GCSF_HIGHLIGHTED_TOOLGROUP_SELECTED  
Set if user has clicked, or in some other manner, "selected" the toolgroup of a 
particular controller. This flag is used by GenToolControl to provide the 
shortcut for the user of scrolling the ToolGroup list to this selection.

GCSF_TOOLBOX_FEATURES_CHANGED  
This flag is set if toolbox features have been added or removed

GCSF_NORMAL_FEATURES_CHANGED  
This flag is set if normal features have been added or removed.

**Library:** Objects/gCtrlC.def

----------
#### GenControlUIType
    GenControlUIType        etype word
        GCUIT_NORMAL            enum    GenControlUIType
        GCUIT_TOOLBOX           enum    GenControlUIType

GCUIT_NORMAL  
This type indicates that the "normal UI" components are set. Generally, this 
includes menu items or features within a dialog box.

GCUIT_TOOLBOX  
This type indicates that the toolbox components are set. A toolbox generally 
consists of "Tiny" sized triggers or items within popup lists.

**Library:** gCtlC.def

----------
#### GenControlUpdateUIParams
    GenControlUpdateUIParams        struct
        GCUUIP_manufacturer             ManufacturerID
        GCUUIP_changeType               word
        GCUUIP_dataBlock                hptr
        GCUUIP_features                 word
        GCUUIP_toolboxFeatures          word
        GCUUIP_childBlock               hptr
        GCUUIP_toolBlock                hptr
    GenControlUpdateUIParams        ends

*GCUUIP_features* stores the features list from the GenControl's temporary 
instance data TEMP_GEN_CONTROL_INSTANCE. This entry is clear if 
GCIF_NORMAL_UI is not set in TEMP_GEN_CONTROL_INSTANCE.

*GCUUIP_toolboxFeatures* stores the tools features list from the GenControl's 
TEMP_GEN_CONTROL_INSTANCE. This entry is clear if GCIF_TOOLBOX_UI 
is not set in TEMP_GEN_CONTROL_INSTANCE.

*GCUUIP_childBlock* stores the optr of the child block (from 
TEMP_GEN_CONTROL_INSTANCE).

**Library:** Objects/gCtrlC.def

----------
#### GenControlUserData
    GenControlUserData          struc
        GCUD_flags                      GenControlUserFlags
        GCUD_userAddedUI                word
        GCUD_userRemovedUI              word
        GCUD_userAddedToolboxUI         word
        GCUD_userRemovedToolboxUI       word
    GenControlUserData          ends

**Library:** Objects/gCtrlC.def

----------
#### GenControlUserFlags
    GenControlUserFlags         record
                                    :14
        GCUF_USER_TOOLBOX_UI        :1
        GCUF)USER_UI                :1
    GenControlUserFlags         end

**Library:** Objects/gCtrlC.def

----------
#### GenDefaultMonikerType
    GenDefaultMonikerType       etype word
        ; monikers used for various levels in the Set User Level dialog box.
        GDMT_LEVEL_0                enum GenDefaultMonikerType
        GDMT_LEVEL_1                enum GenDefaultMonikerType
        GDMT_LEVEL_2                enum GenDefaultMonikerType
        GDMT_LEVEL_3                enum GenDefaultMonikerType
        ;
        ; moniker used for Help triggers in dialog boxes, etc.
        GDMT_HELP                   enum GenDefaultMonikerType
        ;
        ; moniker used for Help triggers in the title bar of the primary.
        GDMT_HELP_PRIMARY           enum GenDefaultMonikerType

**Library:** Objects/genC.def

----------
#### GenDisplayAttrs
    GenDisplayAttrs     record
        GDA_USER_DISMISSABLE        :1
                                    :7
    GenDisplayAttrs     end

GDA_USER_DISMISSABLE  
This flag is set if user is allowed to dismiss this window. Dismissing the 
display will close the window. A GenDisplay's user dismissable behavior does 
not affect iconification operations. This attribute is implemented in some 
specific UIs (e.g. Open Look) by providing a push-pin which may be 
unpinned. Other specific UIs (e.g. CUA) provide a "CLOSE" option in the 
system menu.

**Library:** Objects/gDispC.def

----------
#### GenDisplayControlAttributes
    GenDisplayControlAttributes             record
        GDCA_MAXIMIZED_NAME_ON_PRIMARY          :1
                                                :7
    GenDisplayControlAttributes             end

GDCA_MAXIMIZED_NAME_ON_PRIMARY  
This flag sets the moniker of a maximized display is the long term moniker 
of the primary.

**Library:** Objects/gDCtrlC.def

----------
#### GenDocumentAttrs
    GenDocumentAttrs        record
        ;
        ; These bits reflect permanent attributes of the document
        ;
        GDA_READ_ONLY               :1  ;File is opened read-only
        GDA_READ_WRITE              :1  ;File is opened read-write
        GDA_FORCE_DENY_WRITE        :1  ;File is opened "force deny write"
        GDA_SHARED_MULTIPLE         :1  ;File opened "shared multiple"
        GDA_SHARED_SINGLE           :1  ;File opened "shared single"
        ;
        ; These bits reflect temporary states of the document - these bits are set
        ; by the document control object
        ;
        GDA_UNTITLED                :1  ;File does not have a real (user) name
        GDA_DIRTY                   :1  ;File has been modified
        GDA_CLOSING                 :1  ;File is being closed
        GDA_ATTACH_TO_DIRTY_FILE    :1  ;File is attached to a dirty file
        GDA_SAVE_FAILED             :1  ;"Save" failed, revert is not possible
        GDA_OPENING                 :1  ;Document is being opened
        GDA_AUTO_SAVE_STOPPED       :1  ;Auto-save has been stopped
        GDA_MODEL                   :1  ;Document has the model exclusive
        GDA_ON_WRITABLE_MEDIA       :1
        GDA_BACKUP_EXISTS           :1  ;A document backup file exists
        ;
        ; These bits reflect temporary states of the document - these bits are set
        ; by the application
        ;
        GDA_PREVENT_AUTO_SAVE       :1  ;Do not auto save (temporary state set
                                        ;by the application)
    GenDocumentAttrs        end

**Library:** Objects/gDocC.def

----------
#### GenDocumentChangePasswordParams
    GenDocumentChangePasswordParams                 struct
        GDCPP_password          char (MAX_PASSWORD_SIZE+2) dup (?)
    GenDocumentChangePasswordParams                 ends

**Library:** Objects/gDocC.def

----------
#### GenDocumentControlAttrs
    GenDocumentControlAttrs         record
        ;
        ; File attributes
        ;
        GDCA_MULTIPLE_OPEN_FILES    :1  ; Allows multiple files to be opened
        GDCA_MODE                   GenDocumentControlMode:2
        GDCA_DOS_FILE_DENY_WRITE    :1  ; If GDCA_VM_FILE is not set, then open
                                        ; a standard DOS file deny-write
        GDCA_VM_FILE                :1  ; Documents stored in VM files
        GDCA_NATIVE                 :1  ; If GDCA_VM_FILE is not set, documents
                                        ; are stored in a format native to the
                                        ; file system
        GDCA_SUPPORTS_SAVE_AS_REVERT :1 ; Document uses "save as"
        ;
        ; Current state
        ;
        GDCA_DOCUMENT_EXISTS        :1  ; At least one document exists
        GDCA_CURRENT_TASK GDCTask   :4  ; Current task being performed
        GDCA_DO_NOT_SAVE_FILES      :1  ; Working model support...
        GDCA_FORCE_DEMAND_PAGING    :1  ; Forces demand-paging of documents, 
                                        ; even on systems that force documents
                                        ; completely into memory.
                                    :2
    GenDocumentControlAttrs         end

**Library:** Objects/gDocCtrl.def

----------
#### GenDocumentControlFeatures
    GenDocumentControlFeatures              record
        ; File features
        GDCF_READ_ONLY_SUPPORTS_SAVE_AS_REVERT          :1
        GDCF_SINGLE_FILE_CLEAN_CAN_NEW_OPEN             :1
        GDCF_SUPPORTS_TEMPLATES                         :1
        GDCF_SUPPORTS_USER_SETTABLE_EMPTY_DOCUMENT      :1
        GDCF_SUPPORTS_USER_SETTABLE_DEFAULT_DOCUMENT    :1
        GDCF_SUPPORTS_USER_MAKING_SHARED_DOCUMENTS      :1
        GDCF_NAME_ON_PRIMARY                            :1
                                                        :9
    GenDocumentControlFeatures              end

GDCF_READ_ONLY_SUPPORTS_SAVE_AS_REVERT  
If set, the document control allows read-only files to be edited.

GDCF_SINGLE_FILE_CLEAN_CAN_NEW_OPEN  
If set, the document control allows the user to use "new" or "open" to create 
another document even if multiple files are not allowed. The current 
document must be clean.

GDCF_SUPPORTS_TEMPLATES  
If set, the document control supports template documents.

**Library:** Objects/gDocCtrl.def

----------
#### GenDocumentControlMode
    GenDocumentControlMode      etype byte
        GDCM_VIEWER                 enum GenDocumentControlMode
        GDCM_SHARED_SINGLE          enum GenDocumentControlMode
        GDCM_SHARED_MULTIPLE        enum GenDocumentControlMode

**Library:** Objects/gDocCtrl.def

----------
#### GenDocumentGetVariableParams
    GenDocumentGetVariableParams                struct
        GDGVP_position          PointDWord              ;object position
        GDGVP_buffer            fptr.char               ;buffer for result
        GDGVP_graphic           fptr.VisTextGraphic     ;graphic
        GDGVP_object            optr                    ;source object
    GenDocumentGetVariableParams                ends

**Library:** Objects/gDocC.def

----------
#### GenDocumentGroupAttrs
    GenDocumentGroupAttrs           record
        GDGA_VM_FILE                            :1  ;Documents stored in VM files
        GDGA_NATIVE                             :1  ;If document not in VM file,
                                                    ;then should be in format
                                                    ;native to file system.
        GDGA_SUPPORTS_AUTO_SAVE                 :1  ;Use auto-save
        GDGA_AUTOMATIC_CHANGE_NOTIFICATION      :1  ;Automatically provide change
                                                    ;notification
        GDGA_AUTOMATIC_DIRTY_NOTIFICATION       :1  ;Use automatic mechanism for
                                                    ;VM dirty notification
        GDGA_APPLICATION_THREAD                 :1  ;Set if AppDocumentControl runs
                                                    ;in the application thread
        GDGA_VM_FILE_CONTAINS_OBJECTS           :1  ;Set if appropriate VM
                                                    ;attributes for storing objects
                                                    ;should be set in the VM file
        GDGA_CONTENT_DOES_NOT_MANAGE_CHILDREN   :1  ;VisContent does not manage its
                                                    ;children
        GDGA_LARGE_CONTENT                      :1  ;VisContent uses large model
        GDGA_AUTOMATIC_UNDO_INTERACTION         :1  ;Sends out undo set-context
                                                    ; messages automatically
                                                :6
    GenDocumentGroupAttrs           end

**Library:** Objects/gDocGrpC.def

----------
#### GenDocumentOperation
    GenDocumentOperation            etype word
        GDO_NORMAL              enum GenDocumentOperation
        GDO_SAVE_AS             enum GenDocumentOperation
        GDO_REVERT              enum GenDocumentOperation
        GDO_REVERT_QUICK        enum GenDocumentOperation
        GDO_ATTACH              enum GenDocumentOperation
        GDO_DETACH              enum GenDocumentOperation
        GDO_NEW                 enum GenDocumentOperation
        GDO_OPEN                enum GenDocumentOperation
        GDO_SAVE                enum GenDocumentOperation
        GDO_CLOSE               enum GenDocumentOperation
        GDO_AUTO_SAVE           enum GenDocumentOperation

**Library:** Objects/gDocC.def

----------
#### GenDocumentType
    GenDocumentType     etype word
        GDT_NORMAL                  enum GenDocumentType
        GDT_READ_ONLY               enum GenDocumentType
        GDT_TEMPLATE                enum GenDocumentType
        GDT_READ_ONLY_TEMPLATE      enum GenDocumentType
        GDT_PUBLIC                  enum GenDocumentType
        GDT_MULTI_USER              enum GenDocumentType

**Library:** Objects/gDocC.def

----------
#### GenDynamicListPosition
    GenDynamicListPosition          etype word
        GDLP_FIRST          enum GenDynamicListPosition, 00000h
        GDLP_LAST           enum GenDynamicListPosition, 0ffffh

**Library:** Objects/gDListC.def

----------
#### GeneralConsumerModeFlags
    GeneralConsumerModeFlags        record
                                        :2
        GCMF_LEFT_ICON      GCMIcon:3   ; Indicates which icon to show on 
                                        ; the left side of the title bar.
        GCMF_RIGHT_ICON     GCMIcon:3   ; Indicates which icon to show on 
                                        ; the right side of the title bar.
    GeneralConsumerModeFlags        end

**Library:** Objects/genC.def

----------
#### GeneralEvent
    GeneralEvent        etype word, 0, 2
        GE_NO_EVENT                 enum GeneralEvent
        GE_END_OF_SONG              enum GeneralEvent
        GE_SET_PRIORITY             enum GeneralEvent
        GE_SET_TEMPO                enum GeneralEvent
        GE_SEND_NOTIFICATION        enum GeneralEvent
        GE_V_SEMAPHORE              enum GeneralEvent

This types stores events that are required in the sound stream, but are not 
actually involved in the generation of specific sounds.

GE_NO_EVENT  
This event generates exceptionally long durations.

GE_END_OF_SONG  
This event marks the end of the song. Any event or delta-time 
after an EOS mark will be ignored.

GE_SET_PRIORITY  
This event changes the priority of the stream. All following 
events will be evaluated at that priority.

GE_SET_TEMPO  
This event changes the tempo of the song from that point 
onward. Any delta-Tempo following that event will use the new 
value as the # of msec per 64th beats.

GE_SEND_NOTIFICATION  
This event causes the stream to send a given message to a 
given object. The message will be placed at the end of the 
queue.

GE_V_SEMAPHORE  
This event causes the stream to V the semaphore handle.

**Library:** sound.def

----------
#### GenFieldFlags
    GenFieldFlags       record
        GFF_DETACHING               :1
        GFF_LOAD_BITMAP             :1
        GFF_RESTORING_APPS          :1
        GFF_NEEDS_WORKSPACE_MENU    :1
        GFF_HAS_DEFAULT_LAUNCHER    :1
        GFF_NEED_DEFAULT_LAUNCHER   :1
        GFF_QUIT_ON_CLOSE           :1
        GFF_LOAD_DEFAULT_LAUNCHER_WHEN_NEXT_PROCESS_EXITS   :1
    GenFieldFlags       end

These flags affect one of the system objects - the GenField object. As such, 
there will be no need for your application to set or alter these flags.

GFF_DETACHING  
This flag is set if MSG_META_DETACH has been sent to the 
GenField object. This flag is cleared when the detach is 
complete.

GFF_LOAD_BITMAP  
This flag is set if we want to draw a bitmap on this field.

GFF_RESTORING_APPS  
This flag is set if we are currently restoring applications.

GFF_NEEDS_WORKSPACE_MENU  
This flag is set if an application express menu is needed for the 
field.

GFF_HAS_DEFAULT_LAUNCHER  
This flag is set if this field should start a default launcher. The 
name of this launcher is stored in GEOS.INI file under key 
'defaultLauncher' and category specified by 
ATTR_GEN_INIT_FILE_CATEGORY.  

GFF_NEED_DEFAULT_LAUNCHER  
Set if the field detached because it had no focusable apps 
available, so we need to start the default launcher when we 
restore it.

GFF_QUIT_ON_CLOSE  
Set if the field is in the process of doing a `quitOnClose'. 

GFF_LOAD_DEFAULT_LAUNCHER_WHEN_NEXT_PROCESS_EXITS  
We tried to load the default launcher, but couldn't because the 
system was too busy - wait until a process exits, then try again.

**Library:** Objects/gFieldC.def

----------
#### GenFilePath
    GenFilePath     struct
        GFP_disk        word SP_TOP
        GFP_path        PathName
    GenFilePath     ends

*GFP_disk* stores the handle of the disk on which the path resides. This may 
be initialized to a **StandardPath** constant.

*GFP_path* stores the absolute path (or relative path if GFP_disk is a 
**StandardPath** constant) to the directory.

**Library:** Objects/genC.def

----------
#### GenFileSelectorEntryFlags
    GenFileSelectorEntryFlags               record
        GFSEF_TYPE              GenFileSelectorEntryType:2
        GFSEF_OPEN              :1
        GFSEF_NO_ENTRIES        :1
        GFSEF_ERROR             :1
        GFSEF_TEMPLATE          :1
        GFSEF_SHARED_MULTIPLE   :1
        GFSEF_SHARED_SINGLE     :1
        GFSEF_READ_ONLY         :1
        GFSEF_PARENT_DIR        :1
                                :6
    GenFileSelectorEntryFlags               end

GFSEF_TYPE  
This flags stores the type of entry selected.

GFSEF_OPEN  
The selection should be opened. (User has double-clicked).

GFSEF_NO_ENTRIES  
No entries are within the file selector's list.

GFSEF_ERROR  
The file selector encountered an error opening a selection entry 
(through MSG_GEN_FILE_SELECTOR_OPEN_ENTRY or a 
double-click).

GFSEF_TEMPLATE  
This flag is set if the file is a template (from GFHF_TEMPLATE).

GFSEF_SHARED_MULTIPLE  
This flag is set if the file is shared with multiple writers (from 
GFHF_SHARED_MULTIPLE).

GFSEF_SHARED_SINGLE  
This flag is set if the file is shared with a single writer (from 
GFHF_SHARED_SINGLE).

GFSEF_READ_ONLY  
This flag is set if the file is read-only (from FA_RDONLY).

GFSEF_PARENT_DIR  
This flag is set if the current selection is the parent directory 
entry (first entry).

**Library:** Objects/gFSelC.def

----------
#### GenFileSelectorEntryType
    GenFileSelectorEntryType            etype byte, 0
        GFSET_FILE              enum GenFileSelectorEntryType
        GFSET_SUBDIR            enum GenFileSelectorEntryType
        GFSET_VOLUME            enum GenFileSelectorEntryType

**Library:** Objects/gFSelC.def

----------
#### GenFileSelectorFileAttrs
    GenFileSelectorFileAttrs                struct
        GFSFA_match         FileAttrs   ; Attributes that must match
        GFSFA_mismatch      FileAttrs   ; Attributes that must not match
    GenFileSelectorFileAttrs                ends

**Library:** Object/gFSelC.def

----------
#### GenFileSelectorGeodeAttrs
    GenFileSelectorGeodeAttrs               struct
        GFSGA_match         GeodeAttrs  ; Attributes that must match
        GFSGA_mismatch      GeodeAttrs  ; Attributes that must not match
    GenFileSelectorGeodeAttrs               ends

**Library:** Objects/gFSelC.def

----------
#### GenFileSelectorScalableUICommand
    GenFileSelectorScalableUICommand                    etype byte
        GFSSUIC_SET_FEATURES_IF_APP_FEATURE_ON      enum 
        GenFileSelectorScalableUICommand
        GFSSUIC_SET_FEATURES_IF_APP_FEATURE_OFF     enum 
        GenFileSelectorScalableUICommand
        GFSSUIC_ADD_FEATURES_IF_APP_FEATURE_ON      enum 
        GenFileSelectorScalableUICommand
        GFSSUIC_SET_FEATURES_IF_APP_LEVEL           enum 
        GenFileSelectorScalableUICommand
        GFSSUIC_ADD_FEATURES_IF_APP_LEVEL           enum 
        GenFileSelectorScalableUICommand

**Library:** Objects/gFSelcC.def

----------
#### GenFileSelectorScalableUIEntry
    GenFileSelectorScalableUIEntry                  struct
        GFSSUIE_command             GenFileSelectorScalableUICommand
        GFSSUIE_appFeature          word
        GFSSUIE_fsFeatures          FileSelectorAttrs
    GenFileSelectorScalableUIEntry                  ends

**Library:** Objects/gFSelC.def

----------
#### GenFileSelectorType
    GenFileSelectorType         etype byte
        GFST_DOCUMENTS              enum GenFileSelectorType
        GFST_EXECUTABLES            enum GenFileSelectorType
        GFST_NON_GEOS_FILES         enum GenFileSelectorType
        GFST_ALL_FILES              enum GenFileSelectorType

**Library:** Objects/gDocCtrl.def

----------
#### GenFindObjectWithMonikerFlags
    GenFindObjectWithMonikerFlags               record
        GFOWMF_EXACT_MATCH              :1
        GFOWMF_SKIP_THIS_NODE           :1
                                        :14
    GenFindObjectWithMonikerFlags               end

GFOWMF_EXACT_MATCH  
If set, text within the searched moniker must match the passed text 
completely. The passed text will not match if it represents only a portion of 
an object's moniker text.

GFOWMF_SKIP_THIS_NODE  
If set, the search operation will skip this object and just check objects below 
it in the generic tree.

**Library:** Objects/genC.def

----------
#### GenGadgetAttributes
    GenGadgetAttributes         record
        GGA_COMPOSITE       :1
                            :7
    GenGadgetAttributes         end

GGA_COMPOSITE  
This flag is set if gadget object should become a VisComp. If set then all 
generic children will become visual children.

**Library:** Objects/gGadgetC.def

----------
#### GenInteractionAttrs
    GenInteractionAttrs         record
        GIA_NOT_USER_INITIATABLE            :1
        GIA_INITIATED_VIA_USER_DO_DIALOG    :1
        GIA_MODAL                           :1
        GIA_SYS_MODAL                       :1
                                            :4
    GenInteractionAttrs         end

GIA_NOT_USER_INITIATABLE  
This flag is set to indicate that a dialog GenInteraction should 
build an activation trigger that brings up the dialog. Instead, 
the dialog must be brought up with 
MSG_GEN_INTERACTION_INITIATE. In this case, the 
GenInteraction should be a child of GenPrimary or 
GenApplication.

GIA_INITIATED_VIA_USER_DO_DIALOG  
This flag is set to indicate that a dialog GenInteraction will be 
displayed using the routine **UserDoDialog**. Input hold up, 
ignore, & busy states are overridden by default to allow users 
to interact with this type of GenInteraction (and prevent 
user-lock-out which could potentially occur in these cases).

GIA_MODAL  
This flag is set to indicate that a dialog GenInteraction needs 
to be modal. This modality indicates that the application has 
been coded in such a way that the it cannot allow a dialog box 
to stay on-screen while allowing the user to work in other areas 
of the application. (E.g. selection information in the dialog box 
is not updated if the user were to change the selection). 

GIA_SYS_MODAL  
This flag sets a dialog GenInteraction modal at the system 
level. Only use this flag if no other way can be found to perform 
the required operation, as it will halt input to all other parts of 
the system.

**Library:** Objects/gInterC.def

----------
#### GenInteractionDiscardInfo
    GenInteractionDiscardInfo       struct
        GIDI_inUse              word
        ; If non-zero, the interaction is onscreen, or is about to go
        ; onscreen, so it should not be discarded.
        GIDI_discardCount       word
        ; Count of MSG_GEN_DESTROY_AND_DISCARD_BLOCK messages that have to
        ; come in before the block is discarded.
    GenInteractionDiscardInfo       ends

**Library:** gInterC.def

----------
#### GenInteractionGroupType
    GenInteractionGroupType         etype byte
    GIGT_FILE_MENU          enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the File menu. Can
    ; contain DocumentControl or other file-related commands.
    ;
    GIGT_EDIT_MENU          enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the Edit menu. Can
    ; contain EditControl or other edit commands.
    ;
    GIGT_VIEW_MENU          enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the View menu. Can
    ; contains ViewControl or other view commands.
    ;
    GIGT_OPTIONS_MENU       enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the Options menu. Can
    ; contain application options.
    ;
    GIGT_WINDOW_MENU        enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the Window menu. Can
    ; contain GenDisplayControl or other window commands.
    ;
    GIGT_HELP_MENU          enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the Help menu.
    ;
    GIGT_PRINT_GROUP        enum        GenInteractionGroupType
    ; Set to indicate that this GenInteraction is the Print group. Can
    ; contain PrintControl or other print commands.
    ;

GIGT_FILE_MENU  
This indicates that this GenInteraction acts as the File menu. 
The GenInteraction can contain a DocumentControl or other 
file-related commands. 

GIGT_EDIT_MENU  
This indicates that this GenInteraction acts as the Edit menu. 
The GenInteraction can contain an EditControl or other edit 
commands.

GIGT_VIEW_MENU  
This indicates that this GenInteraction acts as the View 
menu.The GenInteraction can contain a ViewControl or other 
view commands.

GIGT_OPTIONS_MENU  
This indicates that this GenInteraction acts as the Options 
menu. The GenInteraction can contain application options.

GIGT_WINDOW_MENU  
This indicates that this GenInteraction acts as the Window 
menu. The GenInteraction can contain a GenDisplayControl or 
other window commands.

GIGT_HELP_MENU  
This indicates that this GenInteraction acts as the Help menu.

GIGT_PRINT_GROUP  
This indicates that this GenInteraction acts as the Print group. 
The GenInteraction can contain a PrintControl or other print 
commands.

**Library:** Objects/gInterC.def

----------
#### GenInteractionType
    GenInteractionType      etype byte
        GIT_ORGANIZATIONAL          enum GenInteractionType
        GIT_PROPERTIES              enum GenInteractionType
        GIT_PROGRESS                enum GenInteractionType
        GIT_COMMAND                 enum GenInteractionType
        GIT_NOTIFICATION            enum GenInteractionType
        GIT_AFFIRMATION             enum GenInteractionType
        GIT_MULTIPLE_RESPONSE       enum GenInteractionType

GIT_ORGANIZATIONAL  
This indicates that this GenInteraction is only used for grouping its children. 
This type has two chief uses:

1. For geometry purposes.  
Collecting a group of generic objects together under a 
GenInteraction allows you to specify geometry for that group 
via hints. The group will be dealt with as a single entity for 
geometry purposes.

2. For holding other GenInteractions.  
Organizational interactions can act as a place-holder to put 
non-user-initiatable dialogs. This leaves the dialogs in the 
generic tree in a non-visible location where they can be 
initiated. As you generally don't want this organizational 
interaction to be visible itself, you will most likely want to mark 
it as a GIV_DIALOG and GIA_NOT_USER_INITIATABLE as well. 
(Optimization note: This also has the side effect of avoiding the 
loading in of the resources that the child dialog boxes reside in, 
should this object receive MSG_SPEC_BUILD.) ;

This type does not support a reply bar, even if GIV_DIALOG is used to specify 
that this GenInteraction should become a dialog box. OSF/Motif will ensure 
that there is a way for the user to dismiss any GIT_ORGANIZATIONAL 
interaction that does becomes a visible dialog.

GIT_PROPERTIES  
This indicates that this interaction contains UI used for object properties. 
This type supports the IC_APPLY and IC_RESET **InteractionCommand** 
types which can be used for apply and reset functionality. If the interaction 
is built as a dialog box, the specific UI (such as OSF/Motif) will create "Apply" 
and "Close" triggers that reference the IC_APPLY and IC_DISMISS interaction 
commands.

In OSF/Motif, the following table shows the standard triggers that will be 
provided with various hints (given a GIT_PROPERTIES GenInteraction that 
is built as a dialog box):

**Table 12-1** Default Triggers supplied with a GIT_PROPERTIES dialog box.

    Hint                                    Triggers Supplied

    None (delayed properties)               "Apply" "Close"
    _UNRELATED_PROPERTIES or
    _FAST_RESPONSE_PROPERTIES               None (Immediate properties)
    _RELATED_PROPERTIES,
    _SLOW_RESPONSE_PROPERTIES or
    _REQUIRES_VALIDATION                    "Apply" "Close"
    _COMPLEX_PROPERTIES *and*
    (_RELATED_PROPERTIES,
    _SLOW_RESPONSE_PROPERTIES or
    _REQUIRES_VALIDATION)                   "Apply" "Reset" "Close"
    _SINGLE_USAGE *and*
    (_RELATED_PROPERTIES,
    _SLOW_RESPONSE_PROPERTIES or
    _REQUIRES_VALIDATION)                   "OK" "Cancel"
    _SINGLE_USAGE *and* 
    (_COMPLEX_PROPERTIES,
    _SLOW_RESPONSE_PROPERTIES or
    _REQUIRES_VALIDATION)                   "OK" "Reset" "Cancel"
    Modal Dialog Box                        "OK" "Cancel"
    Modal Dialog and _COMPLEX_PROPERTIES    "OK" "Reset" "Cancel"

GIT_PROGRESS  
This indicates that this interaction is used to report progress information 
relating to an operation. If the interaction is built as a dialog box, the specific 
UI (such as OSF/Motif) may create a "Stop" trigger that contains the IC_STOP 
interaction command. Input hold up, ignore, & busy states are overridden by 
default to allow users to interact with modal implementations of this style 
GenInteraction.

GIT_COMMAND  
This indicates that this interaction contains commands to other parts of the 
application. If the interaction is built as a dialog box, the specific UI (such as 
OSF/Motif) may create a "Close" trigger that contains the IC_DISMISS 
interaction command. Any additional command triggers must be provided 
with ATTR_GEN_TRIGGER_INTERACTION_COMMAND and 
HINT_SEEK_REPLY_BAR.

GIT_NOTIFICATION  
This indicates that this interaction is used to report notification of events. 
When built as a dialog box, certain specific UIs (such as OSF/Motif) may 
create an "OK" trigger that contains the IC_OK interaction command. The 
specific UI will not provide "Close" or "Cancel" triggers to dismiss the 
interaction without a response from the user. Input hold up, ignore, and busy 
states are overridden by default to allow users to interact with modal 
implementations of this type of GenInteraction.

GIT_AFFIRMATION  
This indicates that this interaction is used to ask the user to confirm an 
operation. The interaction should include a prompt in the form of a question. 
When built as a dialog box, certain specific UIs (such as OSF/Motif) may 
create "Yes" and "No" triggers that contain the IC_YES and IC_NO interaction 
commands. The specific UI will not provide "Close" or "Cancel" triggers to 
dismiss the interaction without a user response. Input hold up, ignore, and 
busy states are overridden by default to allow users to interact with modal 
implementations of this type of GenInteraction.

GIT_MULTIPLE_RESPONSE  
This indicates that this interaction contains multiple responses (two or more) 
of which one must be chosen. When built as a dialog box, you will need to add 
custom response triggers with 
ATTR_GEN_TRIGGER_INTERACTION_COMMAND and 
HINT_SEEK_REPLY_BAR. The specific UI will not provide "Close" or "Cancel" 
triggers to dismiss the interaction without a user response. Input hold up, 
ignore, and busy states are overridden by default to allow users to interact 
with modal implementations of this type of GenInteraction.

**Library:** Objects/gInterC.def

----------
#### GenInteractionVisibility
    GenInteractionVisibility            etype byte
        GIV_NO_PREFERENCE       enum GenInteractionVisibility
        GIV_POPUP               enum GenInteractionVisibility
        GIV_SUB_GROUP           enum GenInteractionVisibility
        GIV_CONTROL_GROUP       enum GenInteractionVisibility
        GIV_DIALOG              enum GenInteractionVisibility
        GIV_POPOUT              enum GenInteractionVisibility

GIV_NO_PREFERENCE  
This type specifies no visual preference for this interaction. The 
specific UI will determine how this GenInteraction should 
appear depending on hints, location in the visual/generic tree, 
and the type of generic children, etc.

GIV_POPUP  
This type specifies that this interaction should appear as a 
popup (menu or popup list). Popups are normally hidden from 
view until activated and then only remain on-screen for the 
duration of the operation. They are not, in general, 
independently displayable windows.

GIV_SUB_GROUP  
This type specifies that this interaction should appear as a 
sub-group within a larger window. The specific visual 
implementation of a sub-group depends on the visual 
implementation of the parent.

GIV_CONTROL_GROUP  
This type specifies that this interaction contains controls and 
therefore should not be placed directly within a popup 
interaction. The interaction may be built out as a sub-group 
within a larger window or as a separate dialog box.

GIV_DIALOG  
This type specifies that this interaction should appear as a 
dialog box. The specific UI will create an activation trigger that 
brings up the dialog box unless GIA_NOT_USER_INITIATABLE 
is set. This trigger will appear in the location a normal child 
would appear.

GIV_POPOUT  
This type specifies that this interaction can be 'popped out' into 
a dialog box from a sub-group implementation. The interaction 
will normally behave as a GIV_SUB_GROUP until it is 'popped 
out'. Then it behaves as a GIV_DIALOG. 
MSG_GEN_INTERACTION_POP_IN and 
MSG_GEN_INTERACTION_POP_OUT may be used to 'pop' the 
interaction in and out. 
ATTR_GEN_INTERACTION_POPPED_OUT is used to indicate 
that the interaction is popped out. If set within the .ui file, the 
interaction will be popped out upon startup.

**Library:** Objects/gInterC.def

----------
#### GenItemGroupBehaviorType
GenItemGroupBehaviorType            etype byte, 0
GIGBT_EXCLUSIVE                 enum GenItemGroupBehaviorType
GIGBT_EXCLUSIVE_NONE            enum GenItemGroupBehaviorType
GIGBT_EXTENDED_SELECTION        enum GenItemGroupBehaviorType
GIGBT_NON_EXCLUSIVE             enum GenItemGroupBehaviorType

GIGBT_EXCLUSIVE  
This type specifies an exclusive selection list. In this mode, one 
and only one item may be selected at any time; anytime the 
user selects one item, any other will become deselected. The 
user may also not deselect a currently selected item.

GIGBT_EXCLUSIVE_NONE  
This type specifies an exclusive-none selection list. In this 
mode, a user can de-select an already selected item, leaving the 
list with no items selected. GenItemGroups can show a 
none-selected state by returning GIGS_NONE (-1) in place of the 
selected item's identifier.

GIGBT_EXTENDED_SELECTION  
This type specifies an exclusive selection list, but also contains 
the ability to extend the selection of items. If the user drags 
across items, or extends the selection (usually done by holding 
a key down while clicking) several items can be selected. This 
is sometimes useful for selecting a target for an operation 
where choosing one item is good enough for a novice but 
selecting multiple items can be useful for an experienced user. 
As is the case with GIGBT_NON_EXCLUSIVE, an application 
will need to deal with sending and receiving item groups of 
identifiers for multiply selected items.

GIGBT_NON_EXCLUSIVE  
This type specifies a non-exclusive selection list, allowing the 
user to select multiple items with no constraints. If you have 
less than 16 GenItems, you may want to consider a 
GenBooleanGroup instead. Use 
MSG_GEN_ITEM_GROUP_GET_MULTIPLE_SELECTIONS and 
MSG_GEN_ITEM_GROUP_SET_MULTIPLE_SELECTIONS to 
handle multiple selections.

**Library:** Objects/gItemGC.def

----------
#### GenItemGroupStateFlags
    GenItemGroupStateFlags          record
        GIGSF_INDETERMINATE     :1
        GIGSF_MODIFIED          :1
                                :6
    GenItemGroupStateFlags          end

GIGSF_INDETERMINATE  
This flag is set if the current selection is indeterminate. The 
current selection in this case refers to the initial state at the 
beginning of the data being represented. This indeterminate 
state refers to the item group as a whole even if in 
non-exclusive mode.

GIGSF_MODIFIED  
As stored in instance data, and sent in MSG_GEN_APPLY:

This flag is set whenever the group itself should be marked as 
"modified." This flag is cleared anytime the state is set and the 
flag should be set anytime the user changes the state of the 
group. Redundant selections will not change an item group's 
state in GIGBT_EXCLUSIVE or GIGBT_EXTENDED_SELECTION 
style groups. This flag is also cleared when the item group 
receives MSG_GEN_APPLY or any selection setting message 
from the application. This state may further be set using 
MSG_GEN_ITEM_GROUP_SET_MODIFIED_STATE. It may be 
checked using MSG_GEN_ITEM_GROUP_IS_MODIFIED. The 
apply message is normally only sent out on MSG_GEN_PPLY if 
this bit is non-zero, though this behavior can be overridden 
using 
ATTR_GEN_SEND_APPLY_MSG_ON_APPLY_EVEN_IF_NOT_MO
DIFIED.

As sent in status message:

GIGSF_MODIFIED will be set if the user has done something to 
change the state of the item group. If the user just clicks on the 
current selection in a GIGBT_EXCLUSIVE or 
GIGBT_EXTENDED_SELECTION item group, then this bit will 
be clear. If message is the result of a 
MSG_GEN_ITEM_GROUP_SEND_STATUS_MSG being sent, then 
this bit will be the value passed in that message.

**Library:** Objects/gItemGC.def

----------
#### GenItemGroupUpdateExtSelParams
    GenItemGroupUpdateExtSelParams                  struc
        GIGUESP_anchorItem      word        ;anchor item
        GIGUESP_extentItem      word        ;extent item
        GIGUESP_prevExtentItem  word        ;previous extent item
        GIGUESP_setSelMsg       word        ;message to send to change items 
                                            ; dh non-zero to select
                                            ; dh zero to de-select.
        GIGUESP_flags           ExtSelFlags
        GIGUESP_passFlags       byte        ;internal: flags to pass to
                                            ; message in dl
    GenItemGroupUpdateExtSelParams                  ends

**Library:** Objects/gItemGC.def

----------
#### GenMonikerMessageFrame
    GenMonikerMessageFrame          struct
        GMMF_xInset             word
        GMMF_yInset             word
        GMMF_xMaximum           word
        GMMF_yMaximum           word
        GMMF_gState             hptr.GState
        GMMF_textHeight         word
        GMMF_monikerFlags       DrawMonikerFlags
    GenMonikerMessageFrame          ends

This structure stores the parameters used in MSG_GEN_DRAW_MONIKER 
and MSG_GEN_GET_MONIKER_POS.

*GMMF_xInset* stores the x inset to the start of where to draw the moniker, if 
top or bottom justifying.

*GMMF_yInset* stores the y inset to the start of where to draw the moniker, if 
left or right justifying.

*GMMF_xMaximum* and *GMMF_yMaximum* store the maximum size of the 
moniker, if VMF_CLIP_TO_MAXIMUM_WIDTH is set in the 
*GMFP_monikerFlags*. Moniker will be clipped to that width.

*GMMF_gState* stores the **GState** to use to draw the moniker.

*GMMF_textHeight* stores the height of the system text, if we happen to have 
this information available. (This enables the messages to be processed faster; 
otherwise pass 0.)

*GMMF_monikerFlags* stores the various justification and miscellaneous flags 
for drawing the moniker.

**Library:** Objects/genC.def

----------
#### GenOptionsParams
    GenOptionsParams        struct
        GOP_category    char INI_CATEGORY_BUFFER_SIZE dup (?)
        GOP_key         char INI_CATEGORY_BUFFER_SIZE dup (?)
    GenOptionsParams        ends

**Library:** Objects/genC.def

----------
#### GenPathDiskRestoreArgs
    GenPathDiskRestoreArgs      struct
        GPDRA_pathType              word
        GPDRA_savedDiskType         word
        GPDRA_driveName             fptr.char
        GPDRA_diskName              fptr.char
        GPDRA_errorCode             DiskRestoreError
    GenPathDiskRestoreArgs      ends

*GPDRA_pathType* stores the vardata tag under which the path itself is stored.

*GPDRA_savedDiskType* stores the vardata tag under which the disk handle is 
saved.

*GPDRA_driveName* stores the drive name (in a null-terminated text string 
with a trailing ':'.

*GPDRA_diskName* stores the disk name in a null-terminated text string.

*GPDRA_errorCode* stores the error code that will be returned to 
**DiskRestore**.

**Library:** Objects/genC.def

----------
#### GenSaveWindowInfo
    GenSaveWindowInfo       struct
        GSWI_winPosition            SpecWinSizePair
        GSWI_winSize                SpecWinSizePair
        GSWI_winPosSizeState        word
    GenSaveWindowInfo       ends

**Library:** Objects/genC.def

----------
#### GenScanItemsFlags
    GenScanItemsFlags       record
        GSIF_FROM_START                         :1
        GSIF_FORWARD                            :1
        GSIF_WRAP_AROUND                        :1
        GSIF_INITIAL_ITEM_FOUND                 :1
        GSIF_USABLE_AND_ENABLED_ITEM_FOUND      :1
        GSIF_EXISTING_ITEMS_ONLY                :1
        GSIF_DYNAMIC_LIST                       :1
                                                :1
    GenScanItemsFlags       end

GSIF_FROM_START  
Set if there is no initial item. This allows us to easily get to the 
beginning (with GSIF_FORWARD set) or end (with 
GSIF_FORWARD clear) of the list by passing zero for the scan 
amount.

GSIF_FORWARD  
Direction of scan. The scan amount passed will be negated if 
this is set.

GSIF_WRAP_AROUND  
Flag for wrapping around to the beginning if we go past the 
end, or to the end if we go past the beginning. 

GSIF_INITIAL_ITEM_FOUND  
Internal.

GSIF_USABLE_AND_ENABLED_ITEM_FOUND  
Internals.

GSIF_EXISTING_ITEMS_ONLY  
Look through currently existing items only, even if in a 
dynamic list.

GSIF_DYNAMIC_LIST  
Set if item group handler is dealing with a dynamic list, so that 
when the scan fails, it returns various data back to the dynamic 
list rather than returning the first or last item.

**Library:** Objects/gItemGC.def

----------
#### GenStates
    GenStates       record
        GS_USABLE       :1
        GS_ENABLED      :1
                        :6
    GenStates       end

GS_USABLE  
This flag is setable by the application and indicates whether the entire 
generic branch starting with this object should be considered part of the 
application's user interface at this time. If this bit is clear, then neither the 
object nor any of its children will appear or may be interacted with. The 
specific UI and visual state of any object which is made NOT_USABLE will be 
destroyed and the object will be treated as if it were in generic form only.

GS_ENABLED  
This flag indicates whether the user can directly interact with an object. This 
flag is used in generic objects to show options not currently available, which 
is typically represented by "greying out" the object's moniker.

**Library:** Objects/genC.def

----------
#### GenTextAttrs
    GenTextAttrs        record
        GTA_SINGLE_LINE_TEXT                :1
        GTA_USE_TAB_FOR_NAVIGATION          :1
        GTA_INIT_SCROLLING                  :1
        GTA_NO_WORD_WRAPPING                :1
        GTA_ALLOW_TEXT_OFF_END              :1
        GTA_TAIL_ORIENTED                   :1
        GTA_DONT_SCROLL_TO_CHANGES          :1
                                            :1
    GenTextAttrs        end

GTA_SINGLE_LINE_TEXT  
This flag indicates that the text has zero or only one carriage 
return. Scrolling in some specific UIs (such as OpenLook) gets 
implemented horizontally if this is set.

GTA_USE_TAB_FOR_NAVIGATION  
This flag indicates that TAB is used to navigate around your 
text's parent window, rather than inserted in the text field. For 
simple text objects you will nearly always want this.

GTA_INIT_SCROLLING  
This flag forces the text object into a scrolling text area. This 
flag supersedes other size flags. The scrolling box is allowed to 
be resized vertically from a height of one upward. 

GTA_NO_WORD_WRAPPING  
This flag disallows word wrapping. 

GTA_ALLOW_TEXT_OFF_END  
This flag is set if text may overflow past the end of the text box. 

GTA_TAIL_ORIENTED  
This flag is set if we prefer the tail end of the text to be visible 
over the top end, given that option. In a scrolling text box, this 
means we always keep the end of the tail visible while text is 
being added or deleted at the bottom of the text field, if the end 
of the text field is currently visible.

GTA_DONT_SCROLL_TO_CHANGES  
Usually if there is a scrollable text field, any insertion or 
deletion of text will be made visible, by scrolling the text if 
necessary. Setting this flag will turn this behavior off; text can 
be getting inserted at the end of a document without 
automatically scrolling there.

**Library:** Objects/gTextC.def

----------
#### GenTextCustomMargins
    GenTextCustomMargins            struc
        GTCM_lrMargin       byte    ;margin on the left and right of the text.
        GTCM_rbMargin       byte    ;margin on the top and bottom of the text.
    GenTextCustomMargins            ends

**Library:** Objects/gTextC.def

----------
#### GenTextStateFlags
    GenTextStateFlags       record
        GTSF_INDETERMINATE      :1
        GTSF_MODIFIED           :1
                                :6
    GenTextStateFlags       end

GTSF_INDETERMINATE 
This flag is set if the current text is indeterminate. This means 
that for whatever data is being represented, there is more than 
one text. *GTXI_text* in this case should refer to the state at the 
beginning of the data being represented.

GTSF_MODIFIED  
This flag stores a GenText's modified state:

As stored in instance data and sent out in MSG_GEN_APPLY:

This flag is cleared anytime the object's state is set and this flag 
is set anytime the user changes the state of the object . The flag 
is also automatically cleared on MSG_GEN_APPLY or 
MSG_GEN_TEXT_SET_TEXT. This state may be manually 
modified using MSG_GEN_TEXT_SET_MODIFIED_STATE. It 
may be checked using MSG_GEN_TEXT_IS_MODIFIED. The 
apply message is normally only sent out on MSG_GEN_APPLY if 
this bit is non-zero, though this behavior can be overridden 
using 
ATTR_GEN_SEND_APPLY_MSG_ON_APPLY_EVEN_IF_NOT_MO
DIFIED.

As sent in status message: 

GVSF_MODIFIED will be set if the user has done something to 
change the state of the item group. If message is the result of a 
MSG_GEN_TEXT_SEND_STATUS_MSG being sent, then this bit 
will be passed in that message.

**Library:** Objects/gTextC.def

----------
#### GenUpwardQueryType
    GenUpwardQueryType      etype word
        GUQT_UI_FOR_APPLICATION         enum GenUpwardQueryType
        GUQT_UI_FOR_SCREEN              enum GenUpwardQueryType
        GUQT_UI_FOR_FIELD               enum GenUpwardQueryType
        GUQT_UI_FOR_MISC                enum GenUpwardQueryType

**Library:** Objects/genC.def

----------
#### GenValueDisplayFormat
    GenValueDisplayFormat           etype byte
        GVDF_INTEGER                    enum GenValueDisplayFormat
        GVDF_DECIMAL                    enum GenValueDisplayFormat
        GVDF_POINTS                     enum GenValueDisplayFormat
        GVDF_INCHES                     enum GenValueDisplayFormat
        GVDF_CENTIMETERS                enum GenValueDisplayFormat
        GVDF_MILLIMETERS                enum GenValueDisplayFormat
        GVDF_PICAS                      enum GenValueDisplayFormat
        GVDF_EUR_POINTS                 enum GenValueDisplayFormat
        GVDF_CICEROS                    enum GenValueDisplayFormat
        GVDF_POINTS_OR_MILLIMETERS      enum GenValueDisplayFormat
        GVDF_INCHES_OR_CENTIMETERS      enum GenValueDisplayFormat

GVDF_INTEGER  
Value will be displayed as an integer value. The value will be 
displayed with no decimal places, regardless of any fraction in 
the current value or the presence of 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_DECIMAL  
Value will be displayed as a decimal value. By default, the 
value's fraction, if any, will be displayed using 3 decimal places. 
This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_POINTS  
Value will be displayed using distance units, in points, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch). By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_INCHES  
Value will be displayed using distance units, in inches, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch) but is translated into inches for 
display in the value's text field. By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_CENTIMETERS  
Value will be displayed using distance units, in centimeters, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch) but is translated into centimeters 
for display in the value's text field. By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_MILLIMETERS  
Value will be displayed using distance units, in millimeters, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch) but is translated into millimeters 
for display in the value's text field. By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_PICAS  
Value will be displayed using distance units, in picas, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch) but is translated into picas for 
display in the value's text field. By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_EUR_POINTS  
Value will be displayed using distance units, in European 
points, regardless of whether metric or US units are set for the 
given application. The stored value is considered to be in units 
of "Points" (i.e. 1/72 of an inch) but is translated into European 
points for display in the value's text field. By default, the value 
will be displayed with up to 3 decimal places. This can be 
changed with ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_CICEROS  
Value will be displayed using distance units, in ciceros, 
regardless of whether metric or US units are set for the given 
application. The stored value is considered to be in units of 
"Points" (i.e. 1/72 of an inch) but is translated into Ciceros for 
display in the value's text field. By default, the value will be 
displayed with up to 3 decimal places. This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_POINTS_OR_MILLIMETERS  
Value will be displayed using distance units, as points or 
millimeters, depending on whether metric or US units are set 
for the given application. The stored value is considered to be 
in units of "Points" (i.e. 1/72 of an inch) but is translated into 
points of millimeters for display in the value's text field. By 
default, the value will be displayed with up to 3 decimal places. 
This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

GVDF_INCHES_OR_CENTIMETERS  
Value will be displayed using distance units, as inches or 
centimeters, depending on whether metric or US units are set 
for the given application. The stored value is considered to be 
in units of "Points" (i.e. 1/72 of an inch) but is translated into 
inches or millimeters for display in the value's text field. By 
default, the value will be displayed with up to 3 decimal places. 
This can be changed with 
ATTR_GEN_VALUE_DECIMAL_PLACES.

**Library:** Objects/gValueC.def

----------
#### GenValueIntervals
    GenValueIntervals       struc
        GVI_numMajorIntervals   word    ; Total number of major intervals
                                        ; to display over the range.
        GVI_numMinorIntervals   word    ; Total number of minor intervals
                                        ; to display over the range.
    GenValueIntervals       ends

**Library:** Objects/gValueC.def

----------
#### GenValueStateFlags
    GenValueStateFlags      record
        GVSF_INDETERMINATE      :1
        GVSF_MODIFIED           :1
        GVSF_OUT_OF_DATE        :1
                                :5
    GenValueStateFlags      end

GVSF_INDETERMINATE  
This flag is set if the current value is indeterminate. 
*GVLI_value* in this case should refer to the initial state of the 
GenValue before it was set indeterminate.

GVSF_MODIFIED  
This flag is set if the value has been modified.

GVSF_OUT_OF_DATE  
This flag is set when the GenValue object's internal value is out 
of date with what has been typed by the user. The GenValue 
object does not update its internal value on every user key 
press, since the typed value may be temporarily out-of-range as 
they type. The GenValue is then updated on a return press ;by 
the user, a query for the value, on an increment or decrement, 
or as it is being taken offscreen. The GVSF_OUT_OF_DATE flag 
is most useful on status messages, to instruct the status 
message recipient to not try to process the value passed. This 
allows the GenValue to send a status message when the user 
first types in a GenValue, in order to let the recipient know that 
the GenValue has been modified.

As stored in instance data, and sent in MSG_GEN_APPLY:

This modified bit is cleared anytime the object's state is set; the 
flag is set anytime the user changes the state of the object. The 
flag is automatically cleared on MSG_GEN_APPLY or 
MSG_GEN_VALUE_SET_VALUE. This state may further be 
modified using MSG_GEN_VALUE_SET_MODIFIED_STATE. It 
may be checked using MSG_GEN_VALUE_IS_MODIFIED. The 
apply message is normally only sent out on MSG_GEN_APPLY if 
this bit is non-zero, though this behavior can be overridden 
using 
ATTR_GEN_SEND_APPLY_MSG_ON_APPLY_EVEN_IF_NOT_MO
DIFIED

As sent in status message:

GVSF_MODIFIED will be set if the user has done something to 
change the state of the GenValue. If the status message is the 
result of a MSG_GEN_VALUE_SEND_STATUS_MSG being sent, 
then this bit will be the value passed in that message.

GVSF_OUT_OF_DATE  
This flag is set when the GenValue object's internal value is out 
of date with what has been typed by the user. The GenValue 
object does not update its internal value on every user key 
press, since the typed value may be temporarily out-of-range as 
they type. The GenValue is then updated on a return press by 
the user, a query for the value, on an increment or decrement, 
or as it is being taken offscreen. The GVSF_OUT_OF_DATE flag 
is most useful on status messages, to instruct the status 
message recipient to not try to process the value passed. This 
allows the GenValue to send a status message when the user 
first types in a GenValue, in order to let the recipient know that 
the GenValue has been modified.

**Library:** Objects/gValueC.def

----------
#### GenValueType
    GenValueType        etype word
        GVT_VALUE           enum GenValueType   ; The current value 
        GVT_MINIMUM         enum GenValueType   ; The minimum value
        GVT_MAXIMUM         enum GenValueType   ; The maximum value 
        GVT_INCREMENT       enum GenValueType   ; The increment value 
        GVT_LONG            enum GenValueType   ; The longest value we can
                                                ; create
        GVT_RANGE_LENGTH    enum GenValueType   ; The end of the displayed
                                                ; range, if applicable
        GVT_RANGE_END       enum GenValueType   ; The last value in the range,
                                                ; if applicable
        GVT_VALUE_AS_RATIO_OF_AVAILABLE_RANGE
                             enum GenValueType  ; The current value, expressed
                                                ; as a ratio of max-min-
                                                ; range (if any).

**Library:** Objects/gValueC.def

----------
#### GenViewAttrs
    GenViewAttrs        record
        ;
        ; General GenView Attributes
        ;
        GVA_CONTROLLED                          :1
        GVA_GENERIC_CONTENTS                    :1
        GVA_TRACK_SCROLLING                     :1
        GVA_DRAG_SCROLLING                      :1
        GVA_NO_WIN_FRAME                        :1
        GVA_SAME_COLOR_AS_PARENT_WIN            :1
        GVA_VIEW_FOLLOWS_CONTENT_GEOMETRY       :1
        ;
        ; Attributes that follow may only be used if GVA_GENERIC_CONTENTS is
        ; not set. 
        ;
        GVA_WINDOW_CCORDINATE_MOUSE_EVENTS      :1
        GVA_DONT_SEND_PTR_EVENTS                :1
        GVA_DONT_SEND_KBD_RELEASES              :1
        GVA_SEND_ALL_KBD_CHARS                  :1
        GVA_FOCUSABLE                           :1
        GVA_SCALE_TO_FIT                        :1
        GVA_ADJUST_FOR_ASPECT_RATIO             :1
                                                :2
    GenViewAttrs        end

GVA_CONTROLLED  
This flag is set if the view is connected to a controller object and 
therefore should send out notification as appropriate and 
should add itself to the appropriate GCN list.

GVA_GENERIC_CONTENTS  
This flag is set if the content of the GenView is a 
GenContentClass object (and its children are therefore generic 
objects, not visual objects). If this bit is set, the mouse grab 
mode and pointer event sending mode is set by the specific UI, 
overriding whatever is passed in the instance data. Other 
generic messages (such as MSG_SPEC_SPEC_BUILD_BRANCH) 
will be sent to the content as appropriate.

GVA_TRACK_SCROLLING  
This flag is set if scrolling events should be sent to the content, 
so it can control more carefully where scrolling leaves the 
document origin. See 
MSG_META_CONTENT_TRACK_SCROLLING for more info.

GVA_DRAG_SCROLLING  
This flag is set if so that the user can select and drag out of a 
subview window scrolling the window appropriately. This drag 
scrolling operates independently of the objects in the window; 
no special handling by the output objects should be needed.

GVA_NO_WIN_FRAME  
This flag is set if the specific UI shouldn't draw a frame around 
the subview window.

GVA_SAME_COLOR_AS_PARENT_WIN  
This flag is set is the background color of the view should be 
whatever color the parent window's background is. This flag 
should nearly always be set if GVA_GENERIC_CONTENTS is 
set, so the generic objects appear correctly underneath the 
view!

GVA_VIEW_FOLLOWS_CONTENT_GEOMETRY  
This flag is set if the view, in a non-scrollable direction, should 
follow the size of the content. Content object must be running 
in the same thread as the view, so that MSG_VIS_RECALC_SIZE 
can be called on the content.

Attributes that may be used when GVA_GENERIC_CONTENTS is not set.

GVA_WINDOW_COORDINATE_MOUSE_EVENTS  
This flags is set if mouse events should be sent in window coordinates instead 
of document coordinates (that is, as the offset in screen pixels from the upper 
left-hand corner of the view). This bit may be used transparently in 
conjunction with a VisContentClass content, by setting 
VCNA_WINDOW_COORDINATE_MOUSE_EVENTS in the VisContent. This 
arrangement allows the VisContent to provide both 32-bit and fractional 
mouse data to visible objects within the content.

Alternatively, if the content is a process, then the process is responsible for 
converting incoming mouse data back into document coordinates. This may 
be done by storing the current document origin and scale factor as sent out 
by the View, and then using the equation:

![image info](ASMRefFigures/14.png)

View Window Coordinate is the coordinate value passed in the 16-bit mouse 
event when GVA_WINDOW_COORDINATE_MOUSE_EVENTS is set in the 
GenView.

Scale Factor is current view scale factor, as sent to the process in 
MSG_META_CONTENT_VIEW_SCALE_FACTOR_CHANGED.

View Origin is the location in the document that currently appears at the 
upper left corner of the view window, as sent to the process in 
MSG_META_CONTENT_VIEW_ORIGIN_CHANGED.

Note: If *GVI_docBounds* lies outside of the 16-bit graphics space, then this 
flag MUST be used, since standard mouse events cannot pass 32-bit document 
positions.

GVA_DONT_SEND_PTR_EVENTS  
This flag reflects an optimization to avoid sending pointer events to the 
application, if not needed.

GVA_DONT_SEND_KBD_RELEASES  
This flag reflects an optimization to avoid sending keyboard releases to the 
application, if not needed.

GVA_SEND_ALL_KBD_CHARS  
This flag forces all key presses to go to the content (if it has the focus), 
regardless of what those keypresses are. Usually the view will check first for 
mnemonics, accelerators, or other special specific UI keys, and will not pass 
the key press down if it gets handled by the UI in one of these ways. If 
applications set this flag, it is their responsibility in their 
MSG_META_KBD_CHAR handler to return a MSG_FUP_KBD_CHAR back to 
the view so it can finish the keyboard handling. Also, applications (such as 
GeoWrite) that intermix keypresses and other functions (such as changing 
the font or style) may have problems getting these messages in the correct 
order if the keypress has to go across threads to the content first, then back 
to the UI to check for accelerators. For this reason, a lot of applications may 
not want to use this flag. This flag also allows odd keyboard characters to be 
allowed as accelerators in generic objects. (Usually, only ctrl characters and 
a few others are acceptable as accelerators) In summary, the differences 
between each approach:

    SEND_ALL_KBD_CHARS clear            SEND_ALL_KBD_CHARS set
    GenView gets key press,             GenView gets key press,
    Checks ctrl chars for accelerators  Sends to content.
    and other specific-UI actions.      Checks *all* chars FUPped back
    If char not used in these ways,     by content for accelerators
    sends it to content.                and other specific-UI actions.

GVA_FOCUSABLE  
This flag indicates that the view is allowed to have the focus. This flag is set 
by default upon instantiation or declaration in the .ui file. In general, you 
will only want to clear this flag for custom gadgets which should not be 
keyboard navigable.

GVA_SCALE_TO_FIT  
This flag indicates that the view is operating in "scale to fit" mode. In this 
mode the y scale factor will be adjusted so that the entire document fits in the 
view, and the x scale factor will be adjusted accordingly. This behavior can be 
modified by either giving a page size (via ATTR_GEN_VIEW_PAGE_SIZE) to 
scale into the view, specifying that the document should fit entirely in x with 
the y scale factor following (via 
ATTR_GEN_VIEW_SCALE_TO_FIT_BASED_ON_X) or that both the document 
should fit in both x and y (via 
ATTR_GEN_VIEW_SCALE_TO_FIT_BOTH_DIMENSIONS).

GVA_ADJUST_FOR_ASPECT_RATIO  
This flag indicates that scaling is adjusted to match the aspect ratio of the 
screen.

**Library:** Objects/gViewC.def

----------
#### GenViewControlAttrs
    GenViewControlAttrs         record
        GVCA_ADJUST_ASPECT_RATIO        :1
        GVCA_APPLY_TO_ALL               :1
        GVCA_SHOW_HORIZONTAL            :1
        GVCA_SHOW_VERTICAL              :1
                                        :12
    GenViewControlAttrs         end

**Library:** Objects/gViewCC.def

----------
#### GenViewControlSpecialScaleFactor
    GenViewControlSpecialScaleFactor            etype word
        GVCSSF_TO_FIT           enum GenViewControlSpecialScaleFactor

**Library:** Objects/gViewCC.def

----------
#### GenViewDimensionAttrs
    GenViewDimensionAttrs           record
        GVDA_SCROLLABLE                     :1
        GVDA_SPLITTABLE                     :1
        GVDA_TAIL_ORIENTED                  :1
        GVDA_DONT_DISPLAY_SCROLLBAR         :1
        GVDA_NO_LARGER_THAN_CONTENT         :1
        GVDA_NO_SMALLER_THAN_CONTENT        :1
        GVDA_SIZE_A_MULTIPLE_OF_INCREMENT   :1
        GVDA_KEEP_ASPECT_RATIO              :1
    GenViewDimensionAttrs           end

GVDA_SCROLLABLE  
This flag is set if the view is scrollable in the given dimension. 
The view will force itself to be as big as its document size, so 
that nothing is obscured.

GVDA_SPLITTABLE  
This flag is set if the view is splittable in the given dimension. 

GVDA_TAIL_ORIENTED  
This flag is set if the document prefers to be displayed at its 
end. The window will scroll to stay at the bottom of the 
document when you resize or change the document length, but 
only if you are currently at the bottom. If you move to the top 
or middle of the document no scrolling will be done. Currently, 
tail orientation does not work across threads; if you want to do 
this, you can try doing it via a tracking.

GVDA_DONT_DISPLAY_SCROLLBAR  
This flag instructs the view to hide any scrollers in the given 
dimension, even if the view is scrollable.

GVDA_NO_LARGER_THAN_CONTENT  
This flag is set if the view will not get larger than is needed to 
fit the content in the given dimension, based on the current 
value of *GVI_docBounds*. By default there are no restrictions on 
the size of the view.

GVDA_NO_SMALLER_THAN_CONTENT  
This flag is set if the view will stay large enough to display the 
entire content in the given dimension, based on the current 
value of *GVI_docBounds*. By default there are no restrictions on 
the size of the view.

GVDA_SIZE_A_MULTIPLE_OF_INCREMENT  
This flag is set if we want to truncate the view window's size in 
this direction to a multiple of the increment amount in this 
direction. Subclass MSG_GEN_VIEW_CALC_WIN_SIZE if you 
need finer adjustments of the view window size.

GVDA_KEEP_ASPECT_RATIO  
This flag is set if we want to keep the aspect ratio of the port 
windows the same as they are in the open size. If set in 
vertAttrs, then we'll keep the width and use the ratio to 
calculate the height; and vice versa for horizAttrs.

**Library:** Objects/gViewC.def

----------
#### GenViewInkType
    GenViewInkType      etype byte
        GVIT_PRESSES_ARE_NOT_INK            enum GenViewInkType
        GVIT_INK_WITH_STANDARD_OVERRIDE     enum GenViewInkType
        GVIT_PRESSES_ARE_INK                enum GenViewInkType
        GVIT_QUERY_OUTPUT                   enum GenViewInkType

GVIT_PRESSES_ARE_NOT_INK  
The type specifies that the output of the view cannot handle 
ink. If you are using the large document model, you should 
subclass MSG_NOTIFY_DATA_GROUP with the notification type 
GWNT_INK on the VisContent attached to the view. Otherwise, 
ink will be sent to the first child with the target.

GVIT_INK_WITH_STANDARD_OVERRIDE  
This type specifies that the output of the view can handle ink, 
but the user can override it by holding the mouse down for 
some user-specified amount of time before moving the mouse. 

GVIT_PRESSES_ARE_INK  
This type specifies that any mouse presses are ink and the 
output of the view can handle ink. Note: If you use the following 
value, you must handle MSG_META_QUERY_IF_PRESS_IS_INK. 
Note also that a MSG_NOTIFY_DATA_GROUP withe notification 
type of GWNT_INK can come even if no 
MSG_META_QUERY_IF_PRESS_IS_INK has been received. (This 
can happen if the user starts drawing just outside of the view 
but then draws inside the view.)

GVIT_QUERY_OUTPUT  
This type specifies that the output of the view only wants 
presses to be ink under certain conditions. 
MSG_META_QUERY_IF_PRESS_IS_INK is still sent to the 
output.

**Library:** Objects/gViewC.def

----------
#### GeodeAttrs
    GeodeAttrs      record
        GA_PROCESS                      :1  ; Has initial thread
        GA_LIBRARY                      :1  ; Exports routines
        GA_DRIVER                       :1  ; Has DriverTable
        GA_KEEP_FILE_OPEN               :1  ; .geo file must stay open (resource(s)
                                            ; discardable or initially discarded)
        GA_SYSTEM                       :1  ; Compiled into kernel
        GA_MULTI_LAUNCHABLE             :1  ; May be loaded more than once
        GA_APPLICATION                  :1  ; A user-launched application
        GA_DRIVER_INITIALIZED           :1  ; If DRIVER aspect initialized (DR_INIT
                                            ; sent to strategy routine)
        GA_LIBRARY_INITIALIZED          :1  ; If LIBRARY aspect initialized
                                            ; (library entry point called)
        GA_GEODE_INITIALIZED            :1  ; If all aspects initialized.
        GA_USES_COPROC                  :1  ; Uses coprocessor if available
        GA_REQUIRES_COPROC              :1  ; Requires coprocessor/emulator to run
        GA_HAS_GENERAL_CONSUMER_MODE    :1  ; Can be run in GCM mode
        GA_ENTRY_POINTS_IN_C            :1  ; Library/driver entry point in C
                                        :2
    GeodeAttrs      end

**Library:** geode.def

----------
#### GeodeDefaultDriverType
    GeodeDefaultDriverType          etype word, 0, 2
        GDDT_FILE_SYSTEM        enum GeodeDefaultDriverType
        GDDT_KEYBOARD           enum GeodeDefaultDriverType
        GDDT_MOUSE              enum GeodeDefaultDriverType
        GDDT_VIDEO              enum GeodeDefaultDriverType
        GDDT_MEMORY_VIDEO       enum GeodeDefaultDriverType
        GDDT_POWER_MANAGEMENT   enum GeodeDefaultDriverType
        GDDT_TASK               enum GeodeDefaultDriverType

**Library:** driver.def

----------
#### GeodeGetInfoType
    GeodeGetInfoType        etype word, 0, 2
        GGIT_ATTRIBUTES             enum GeodeGetInfoType
        GGIT_TYPE                   enum GeodeGetInfoType
        GGIT_GEODE_RELEASE          enum GeodeGetInfoType
        GGIT_GEODE_PROTOCOL         enum GeodeGetInfoType
        GGIT_TOKEN_ID               enum GeodeGetInfoType
        GGIT_PERM_NAME_AND_EXT      enum GeodeGetInfoType
        GGIT_PERM_NAME_ONLY         enum GeodeGetInfoType

**Library:** geode.def

----------
#### GeodeGrab
    GeodeGrab       struct
        GG_OD           optr
        GG_geode        hptr
    GeodeGrab       ends

This structure stores a top-level grab for controlling input flow to the geode.

**Library:** Objects/uiInputC.def

----------
#### GeodeHeapVars
    GeodeHeapVars   struc
        GHV_heapSpace           word
        ;
        ; Heap space requirement, as copied from EFH_heapSpace from the
        ; ExecutableFileHeader of applications. Roughly, the amount of space
        ; on the heap that this application uses, in paragraphs. The system
        ; sums the total of all "heapSpace" requirements when trying to decide
        ; whether to let another app load or not.

**Library:** geode.def
----------
#### GeodeLoadError
    GeodeLoadError      etype word
        GLE_PROTOCOL_IMPORTER_TOO_RECENT        enum GeodeLoadError
        GLE_PROTOCOL_IMPORTER_TOO_OLD           enum GeodeLoadError
        GLE_FILE_NOT_FOUND                      enum GeodeLoadError
        GLE_LIBRARY_NOT_FOUND                   enum GeodeLoadError
        GLE_FILE_READ_ERROR                     enum GeodeLoadError
        GLE_NOT_GEOS_FILE                       enum GeodeLoadError
        GLE_NOT_GEOS_EXECUTABLE_FILE            enum GeodeLoadError
        GLE_ATTRIBUTE_MISMATCH                  enum GeodeLoadError
        GLE_MEMORY_ALLOCATION_ERROR             enum GeodeLoadError
        GLE_NOT_MULTI_LAUNCHABLE                enum GeodeLoadError
        GLE_LIBRARY_PROTOCOL_ERROR              enum GeodeLoadError
        GLE_LIBRARY_LOAD_ERROR                  enum GeodeLoadError
        GLE_DRIVER_INIT_ERROR                   enum GeodeLoadError
        GLE_LIBRARY_INIT_ERROR                  enum GeodeLoadError
        GLE_DISK_TOO_FULL                       enum GeodeLoadError
        GLE_FIELD_DETACHING                     enum GeodeLoadError
        GLE_INSUFFICIENT_HEAP_SPACE             enum GeodeLoadError

**Library:** geode.def
----------
#### GeodeToken
    GeodeToken      struct
        GT_chars            TokenChars
        GT_manufID          ManufacturerID
    GeodeToken      ends

This structure defines a token identifier for an application. Together the two 
fields uniquely identify the application.

*GT_chars* stores the four character identifier.

*GT_manufID* stores the **ManufacturerID**.

**Library:** geode.def

----------
#### GeosFileHeaderFlags
    GeosFileHeaderFlags             record
        GFHF_TEMPLATE           :1
        GFHF_SHARED_MULTIPLE    :1  ; Also called "multi-user"
        GFHF_SHARED_SINGLE      :1  ; Also called "public"
                :1  
        GFHF_HIDDEN             :1  ; This file is hidden. This flag does
                                    ; not replace the DOS "hidden"
                                    ; attribute -- the two may be
                                    ; set/cleared independently of
                                    ; each-other.
        GFHF_DBCS               :1  ;TRUE: DBCS filename, etc.
                                :10
    GeosFileHeaderFlags             end

**Library:** file.def

----------
#### GeosFileType
    GeosFileType        etype word
        GFT_NOT_GEOS_FILE   enum GeosFileType   ; Not a geos file. defined as 
                                                ; 0 so one can reasonably 
                                                ; look at FEA_FILE_TYPE
        GFT_EXECUTABLE      enum GeosFileType   ; Something we can execute
        GFT_VM              enum GeosFileType   ; Managed by VMem
        GFT_DATA            enum GeosFileType   ; Raw byte-stream of data
        GFT_DIRECTORY       enum GeosFileType   ; Directory
        GFT_OLD_VM          enum GeosFileType   ; VM file from PC/GEOS 1.X.
                                                ; Only FEA_NAME and 
                                                ; FEA_FILE_TYPE are supported 
                                                ; from the set of 
                                                ; GEOS-specific extended attrs

**Library:** file.def

----------
#### GeoWorksGenAppGCNListType
    GeoWorksGenAppGCNListType               etype word, FIRST_GEN_APP_GCN_LIST_TYPE,2
        ;
        ; GenToolControl/GenControl communication related
        ;
        GAGCNLT_SELF_LOAD_OPTIONS                       enum GeoWorksGenAppGCNListType
        GAGCNLT_GEN_CONTROL_NOTIFY_STATUS_CHANGE        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SELECT_STATE_CHANGE   enum GeoWorksGenAppGCNListType
        GAGCNLT_EDIT_CONTROL_NOTIFY_UNDO_STATE_CHANGE   enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_CHAR_ATTR_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_PARA_ATTR_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_TYPE_CHANGE      enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_SELECTION_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_COUNT_CHANGE     enum GeoWorksGenAppGCNListType
        AGCNLT_APP_TARGET_NOTIFY_STYLE_TEXT_CHANGE      enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_STYLE_SHEET_TEXT_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_STYLE_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FONT_CHANGE           enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_POINT_SIZE_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FONT_ATTR_CHANGE      enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_JUSTIFICATION_CHANGE  enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_FG_COLOR_CHANGE  enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_BG_COLOR_CHANGE  enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_PARA_COLOR_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_BORDER_COLOR_CHANGE   enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SEARCH_SPELL_CHANGE   enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SEARCH_REPLACE_CHANGE enum GeoWorksGenAppGCNListType
        ;
        ; Chart related
        GAGCNLT_APP_TARGET_NOTIFY_CHART_TYPE_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_CHART_GROUP_FLAGS     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_CHART_AXIS_ATTRIBUTES enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_CHART_MARKER_SHAPE    enum GeoWorksGenAppGCNListType
        ;
        ; GrObj related
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_CURRENT_TOOL_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_BODY_SELECTION_STATE_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_AREA_ATTR_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_LINE_ATTR_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_TEXT_ATTR_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_STYLE_GROBJ_CHANGE    enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_STYLE_SHEET_GROBJ_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_BODY_INSTRUCTION_FLAGS_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_GROBJ_GRADIENT_ATTR_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        ;
        ; Ruler related
        GAGCNLT_APP_TARGET_NOTIFY_RULER_TYPE_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_RULER_GRID_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_TEXT_RULER_OBJECTS                      enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_RULER_GUIDE_CHANGE    enum GeoWorksGenAppGCNListType
        ;
        ; VisBitmap related
        GAGCNLT_APP_TARGET_NOTIFY_BITMAP_CURRENT_TOOL_CHANGE 
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_BITMAP_CURRENT_FORMAT_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        ;
        ; Flat file library related
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_FIELD_PROPERTIES_STATUS_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_FIELD_LIST_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_RCP_STATUS_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_FIELD_APPEARANCE_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_DUMMY_CHANGE_2
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_FLAT_FILE_DUMMY_CHANGE_3
                                                        enum GeoWorksGenAppGCNListType
        ;
        ; Spool library related
        GAGCNLT_APP_NOTIFY_DOC_SIZE_CHANGE              enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_NOTIFY_PAPER_SIZE_CHANGE            enum GeoWorksGenAppGCNListType
        ;
        ; Used in GenViewControl
        GAGCNLT_APP_TARGET_NOTIFY_VIEW_STATE_CHANGE     enum GeoWorksGenAppGCNListType
        GAGCNLT_CONTROLLED_GEN_VIEW_OBJECTS             enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_INK_STATE_CHANGE      enum GeoWorksGenAppGCNListType
        GAGCNLT_CONTROLLED_INK_OBJECTS                  enum GeoWorksGenAppGCNListType
        ;
        ; Float library related
        GAGCNLT_APP_TARGET_NOTIFY_PAGE_STATE_CHANGE     enum GeoWorksGenAppGCNListType
        ;
        ; GenDocumentControl related
        GAGCNLT_APP_TARGET_NOTIFY_DOCUMENT_CHANGE       enum GeoWorksGenAppGCNListType
        ;
        ; GenDisplayControl related
        GAGCNLT_APP_TARGET_NOTIFY_DISPLAY_CHANGE        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_DISPLAY_LIST_CHANGE   enum GeoWorksGenAppGCNListType
        ;
        ; Spline Library Notification Lists
        GAGCNLT_APP_TARGET_NOTIFY_SPLINE_MARKER_SHAPE   enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPLINE_POINT          enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPLINE_POLYLINE       enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPLINE_SMOOTHNESS     enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPLINE_OPEN_CLOSE_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        ;
        ; Spreadsheet Library Notification Lists
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_ACTIVE_CELL_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_EDIT_BAR_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_SELECTION_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_CELL_WIDTH_HEIGHT_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_DOC_ATTR_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_CELL_ATTR_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_CELL_NOTES_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_DATA_RANGE_CHANGE
                                                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_TEXT_NAME_CHANGE      enum GeoWorksGenAppGCNListType
        GAGCNLT_FLOAT_FORMAT_CHANGE                     enum GeoWorksGenAppGCNListType
        GAGCNLT_DISPLAY_OBJECTS_WITH_RULERS             enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_APP_CHANGE            enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_LIBRARY_CHANGE        enum GeoWorksGenAppGCNListType
        ;
        ; UI Notification Lists
        GAGCNLT_WINDOWS                                 enum GeoWorksGenAppGCNListType
        GAGCNLT_STARTUP_LOAD_OPTIONS                    enum GeoWorksGenAppGCNListType
        ;
        ; CARD LIbrary Notification Lists
        GAGCNLT_APP_TARGET_NOTIFY_CARD_BACK_CHANGE      enum GeoWorksGenAppGCNListType
        ;
        GAGCNLT_NOTIFY_FOCUS_TEXT_OBJECT                enum GeoWorksGenAppGCNListType
        GAGCNLT_NOTIFY_TEXT_CONTEXT                     enum GeoWorksGenAppGCNListType
        ;
        ; Help Notification Lists
        GAGCNLT_NOTIFY_HELP_CONTEXT_CHANGE              enum GeoWorksGenAppGCNListType
        ;
        GAGCNLT_FLOAT_FORMAT_INIT                       enum GeoWorksGenAppGCNListType
        GAGCNLT_ALWAYS_INTERACTABLE_WINDOWS             enum GeoWorksGenAppGCNListType
        GAGCNLT_USER_DO_DIALOGS                         enum GeoWorksGenAppGCNListType
        GAGCNLT_MODAL_WIN_CHANGE                        enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_NAME_CHANGE enum GeoWorksGenAppGCNListType 
        GAGCNLT_CONTROLLERS_WITHIN_USER_DO_DIALOGS      enum GeoWorksGenAppGCNListType
        GAGCNLT_FOCUS_WINDOW_KBD_STATUS                 enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_PAGE_INFO_STATE_CHANGE enum GeoWorksGenAppGCNListType
        GAGCNLT_APP_TARGET_NOTIFY_CURSOR_POSITION_CHANGE enum GeoWorksGenAppGCNListType

The UI library's GenApplicationClass supports its very own GCN (General 
Change Notification) system separate from the kernel's. Lists within this 
system are identified by a **GAGCNListType**, whose enumerations are 
separate from that of the kernel's GCN system.

This section contains the enumerations of the *GCNLT_type* field for 
**GCNListType** used within a GenApplication for the case of *GCNLT_manuf* 
= MANUFACTURER_ID_GEOWORKS.

GAGCNLT_SELF_LOAD_OPTIONS  
Objects on this list don't need to receive 
MSG_META_LOAD_OPTIONS on startup, but do need to receive 
MSG_META_SAVE_OPTIONS. (MSG_META_SAVE_OPTIONS will 
be sent when the GenApplication itself receives 
MSG_META_SAVE_OPTIONS.) This must be done by the 
application, usually when a "Save Options" trigger is activated. 
Objects on this list can be of any class, as MetaClass defines the 
options behavior, though objects will most likely be of a Generic 
UI class (GenClass provides the .ini file behavior. 

Any controller not on the GAGCNLT_STARTUP_LOAD_OPTIONS 
list will need to be placed on this list. A controller should not 
appear in both lists.

Other objects on this list will be those that support options like 
GenBooleanGroups, GenItemGroups, or a GenInteraction with 
ATTR_GEN_INIT_FILE_PROPAGATE_TO_CHILDREN. These 
should have the appropriate ATTR_GEN_INIT_FILE_KEY. The 
GenApplication object should have an appropriate 
ATTR_GEN_INIT_FILE_CATEGORY.

GAGCNLT_GEN_CONTROL_NOTIFY_STATUS_CHANGE  
Objects on this list (generally GenToolControl objects) are kept 
up to date on the status of all GenControl objects. Any specific 
status events are *not* sent with this list -- only notification that 
a change has occurred. This notification passes the data type 
*NotifyGenControlStatusChange*.

GAGCNLT_APP_TARGET_NOTIFY_SELECT_STATE_CHANGE  
Objects on this list receive notification about the selection state 
of the current target object. This notification passes the data 
type **NotifySelectStateChange**.

GCNLT_EDIT_CONTROL_NOTIFY_UNDO_STATE_CHANGE  
Objects on this list receive notification of the undo state of the 
current process. This notification passes the data type 
**NotifyUndoStateChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_CHAR_ATTR_CHANGE  
This notification passes the data type 
**VisTextNotifyCharAttrChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_PARA_ATTR_CHANGE  
This notification passes the data type 
**VisTextNotifyParaAttrChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_TYPE_CHANGE  
This notification passes the data type 
**VisTextNotifyTypeChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_SELECTION_CHANGE  
This notification passes the data type 
**VisTextNotifySelectionChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_COUNT_CHANGE  
This notification passes the data type 
**VisTextNotifyCountChange**.

GAGCNLT_APP_TARGET_NOTIFY_STYLE_TEXT_CHANGE  
Objects on this list receive notification when the current style 
could have changed (i.e. a different style could be the current 
style). This notification passes the data type 
**NotifyStyleChange**.

GAGCNLT_APP_TARGET_NOTIFY_STYLE_SHEET_TEXT_CHANGE  
Objects on this list receive notification when the style sheet 
could have changed (i.e. a style was created, deleted or 
modified). This notification passes the data type 
**NotifyStyleSheetChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_STYLE_CHANGE  
This notification passes the data type 
**NotifyTextStyleChange**.

GAGCNLT_APP_TARGET_NOTIFY_FONT_CHANGE  
This notification passes the data type **NotifyFontChange**.

GAGCNLT_APP_TARGET_NOTIFY_POINT_SIZE_CHANGE  
This notification passes the data type 
**NotifyPointSizeChange**.

GAGCNLT_APP_TARGET_NOTIFY_FONT_ATTR_CHANGE  
This notification passes the data type 
**NotifyFontAttrChange**.

GAGCNLT_APP_TARGET_NOTIFY_JUSTIFICATION_CHANGE  
This notification passes the data type 
**NotifyJustificationChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_FG_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_BG_COLOR_CHANGE    
This notification passes the data type **NotifyColorChange**.

GAGCNLT_APP_TARGET_NOTIFY_PARA_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GAGCNLT_APP_TARGET_NOTIFY_BORDER_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GAGCNLT_APP_NOTIFY_DOC_SIZE_CHANGE  
This notification passes the data type 
**NotifyPageSetupChange**.

GAGCNLT_APP_NOTIFY_PAPER_SIZE_CHANGE  
This notification passes the data type 
**NotifyPageSetupChange**.

GAGCNLT_CONTROLLED_GEN_VIEW_OBJECTS  
Objects on this list are GenView objects that have the 
ATTR_GEN_VIEW_INTERACT_WITH_CONTROLLER attribute. 
Note: If an object is not a GenView object, it shouldn't be on this 
list.

GAGCNLT_APP_TARGET_NOTIFY_INK_STATE_CHANGE  
Objects on this list (controllers) are notified when ink objects 
come up.

GAGCNLT_CONTROLLED_INK_OBJECTS  
Objects on this list are Ink objects that have the 
IF_CONTROLLED bit set. Note: If you aren't an Ink object, you 
shouldn't be on this list.

GAGCNLT_APP_TARGET_NOTIFY_PAGE_STATE_CHANGE  
Objects on this list are notified when the selection state of the 
current target object changes. This notification passes the data 
type **NotifyPageStateChange**

GAGCNLT_APP_TARGET_NOTIFY_DOCUMENT_CHANGE  
Objects on this list are notified about the state of the current 
target document. This notification passes the data type 
**NotifyPageStateChange**.

GAGCNLT_APP_TARGET_NOTIFY_DISPLAY_CHANGE  
Objects on this list are notified about the state of the current 
target display. This notification passes the data type 
**NotifyDisplayChange**.

GAGCNLT_APP_TARGET_NOTIFY_DISPLAY_LIST_CHANGE  
Objects on this list are notified about the state of the current 
target display. This notification passes the data type 
**NotifyDisplayChange**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_ACTIVE_CELL_CHANGE  
This notification passes the data type 
**NotifySSheetActiveCellChanged**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_EDIT_BAR_CHANGE  
This notification passes the data 
**NotifySSheetEditBarChanged**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_SELECTION_CHANGE  
This notification passes the data type 
**NotifySSheetSelectionChanged**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_CELL_WIDTH_HEIGHT_
CHANGE  
This notification passes the data type 
**NotifySSheetCellWidthHeightChange**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_DOC_ATTR_CHANGE  
This notification passes the data type 
**NotifySSheetDocAttrChange**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_CELL_ATTR_CHANGE  
This notification passes the data type 
**NotifySSheetCellAttrChange**.

GAGCNLT_APP_TARGET_NOTIFY_SPREADSHEET_DATA_RANGE_CHANGE  
This notification passes the data type 
**NotifySSheetDataRangeChange**.

GAGCNLT_APP_TARGET_NOTIFY_TEXT_NAME_CHANGE  
This notification passes the data type 
**VisTextNotifyNameChange**.

GAGCNLT_DISPLAY_OBJECTS_WITH_RULERS  
Objects on this list should include all GenDisplays that have 
rulers.

GAGCNLT_WINDOWS  
Objects on this list are windowed objects that should be 
displayed on-screen. In your **.ui** file, any windows that you 
want to appear when the application starts up should be 
included on this list. In most cases, this will be only be a 
GenPrimary. When windows are opened, they are 
automatically added to this list. The list is saved to state and 
used to re-open all windows that were on-screen when the 
application shuts down. The only message sent to this list is 
MSG_META_UPDATE_WINDOW. The classes of objects on this 
list are GenPrimaryClass, GenDisplayClass, and 
GenInteractionClass. GenDisplayGroup does not need to be on 
this list, as it is not an independently displayable window.

GAGCNLT_STARTUP_LOAD_OPTIONS  
Objects on this list need to receive MSG_META_LOAD_OPTIONS 
on startup. MSG_META_LOAD_OPTIONS is automatically sent 
by the UI when the application starts up. 
MSG_META_SAVE_OPTIONS will be sent to the objects on the 
list when the GenApplicationitself receives a 
MSG_META_SAVE_OPTIONS. This must be done by the 
application, usually when a "Save Options" trigger is activated. 
Objects on this list can be of any class, as MetaClass defines the 
options behavior, though objects will most likely be generic 
objects (GenClass provides the .ini file behavior).

The GenViewControl must be placed on this list.

Other objects on this list will be those that support options like 
GenBooleanGroups, GenItemGroups, or a GenInteractions 
with ATTR_GEN_INIT_FILE_PROPAGATE_TO_CHILDREN. 
These should have the appropriate ATTR_GEN_INIT_FILE_KEY. 
The GenApplication object should have an appropriate 
ATTR_GEN_INIT_FILE_CATEGORY.

GAGCNLT_ALWAYS_INTERACTABLE_WINDOWS  
Objects on this are windows that should always remain 
interactable (even when modal windows are on screen).

GAGCNLT_USER_DO_DIALOGS  
Objects on this list include all dialog boxes initiated via 
**UserDoDialog**. 
  
GAGCNLT_MODAL_WIN_CHANGE  
Objects on this list need notification upon modal window 
changes within the application.

**Library:** geoworks.def

----------
#### GeoWorksMetaGCNListType
    GeoWorksMetaGCNListType         etype word, FIRST_META_GCN_LIST_TYPE, 2
        MGCNLT_ACTIVE_LIST              enum GeoWorksMetaGCNListType
        MGCNLT_APP_STARTUP              enum GeoWorksMetaGCNListType

MGCNLT_ACTIVE_LIST  
Objects on this list need to receive MSG_META_ATTACH, 
MSG_META_DETACH, and/or MSG_META_QUIT. Currently, this 
represents only certain controllers. These controllers need 
MSG_META_ATTACH, so they should be placed on the 
MGCNLT_ACTIVE_LIST list in the **.ui** file. Other objects may 
add themselves dynamically to this list, if they only need to 
receive MSG_META_DETACH or MSG_META_QUIT.

These are the current controllers that need to be on the list:
GenToolControl  
GenDocumentControl  
GenDisplayControl  
TextRulerControl  
TabControl

Aside from controller-specific reasons, a controller would need 
to receive MSG_META_ATTACH (and thus would need to be on 
the active list) if it is always interactable 
(GCBF_ALWAYS_INTERACTABLE) or if it is always on its GCN 
lists (GCBF_ALWAYS_ON_GCN_LIST). Objects on this list may 
be of any class, as MSG_META_ATTACH, MSG_META_DETACH, 
and MSG_META_QUIT are defined for MetaClass.

MGCNLT_APP_STARTUP  
Objects on this list need to receive MSG_META_APP_STARTUP 
and MSG_META_APP_SHUTDOWN to know when the 
application has been started or is about to exit regardless of 
whether the app will become/was available to the user. For 
example, the GenDocumentControl needs to receive these 
messages so it can open and manipulate a passed document 
even when the application is launched in engine mode to 
perform some query on the document. Objects on this list may 
be of any class, as MSG_META_APP_STARTUP and 
MSG_META_APP_SHUTDOWN are defined for MetaClass.

**Library:** geoworks.def

----------
#### GeoWorksNotificationType
    GeoWorksNotificationType            etype word
        GWNT_INK                                enum GeoWorksNotificationType
        GWNT_GEN_CONTROL_NOTIFY_STATUS_CHANGE   enum GeoWorksNotificationType
        ;
        ;GenEditControl related.
        ;
        GWNT_SELECT_STATE_CHANGE                enum GeoWorksNotificationType
        GWNT_UNDO_STATE_CHANGE                  enum GeoWorksNotificationType
        ;
        ;StyleSheetControl related.
        ;
        GWNT_STYLE_CHANGE                       enum GeoWorksNotificationType
        GWNT_STYLE_SHEET_CHANGE                 enum GeoWorksNotificationType
        ;
        ;High-level types for the Text object
        ;
        GWNT_TEXT_CHAR_ATTR_CHANGE              enum GeoWorksNotificationType
        GWNT_TEXT_PARA_ATTR_CHANGE              enum GeoWorksNotificationType
        GWNT_TEXT_TYPE_CHANGE                   enum GeoWorksNotificationType
        GWNT_TEXT_SELECTION_CHANGE              enum GeoWorksNotificationType
        GWNT_TEXT_COUNT_CHANGE                  enum GeoWorksNotificationType
        ;
        ;Low-level types for the Text object
        ;
        GWNT_TEXT_STYLE_CHANGE                  enum GeoWorksNotificationType
        GWNT_FONT_CHANGE                        enum GeoWorksNotificationType
        GWNT_POINT_SIZE_CHANGE                  enum GeoWorksNotificationType
        GWNT_FONT_ATTR_CHANGE                   enum GeoWorksNotificationType
        GWNT_JUSTIFICATION_CHANGE               enum GeoWorksNotificationType
        GWNT_TEXT_FG_COLOR_CHANGE               enum GeoWorksNotificationType
        GWNT_TEXT_BG_COLOR_CHANGE               enum GeoWorksNotificationType
        GWNT_TEXT_PARA_COLOR_CHANGE             enum GeoWorksNotificationType
        GWNT_TEXT_BORDER_COLOR_CHANGE           enum GeoWorksNotificationType
        GWNT_SEARCH_REPLACE_ENABLE_CHANGE       enum GeoWorksNotificationType
        GWNT_SPELL_ENABLE_CHANGE                enum GeoWorksNotificationType
        ;
        ;Chart library notification types
        ;
        GWNT_CHART_TYPE_CHANGE                  enum GeoWorksNotificationType
        GWNT_CHART_GROUP_FLAGS                  enum GeoWorksNotificationType
        GWNT_CHART_AXIS_ATTRIBUTES              enum GeoWorksNotificationType
        ;
        ;GrObj library notification types
        ;
        GWNT_GROBJ_CURRENT_TOOL_CHANGE          enum GeoWorksNotificationType
        GWNT_GROBJ_BODY_SELECTION_STATE_CHANGE  enum GeoWorksNotificationType
        GWNT_GROBJ_AREA_ATTR_CHANGE             enum GeoWorksNotificationType
        GWNT_GROBJ_LINE_ATTR_CHANGE             enum GeoWorksNotificationType
        GWNT_GROBJ_TEXT_ATTR_CHANGE             enum GeoWorksNotificationType
        GWNT_GROBJ_BODY_INSTRUCTION_FLAGS_CHANGE enum GeoWorksNotificationType
        GWNT_GROBJ_GRADIENT_ATTR_CHANGE         enum GeoWorksNotificationType
        ;
        ;Ruler library notification types
        ;
        GWNT_RULER_TYPE_CHANGE                  enum GeoWorksNotificationType
        GWNT_RULER_GRID_CHANGE                  enum GeoWorksNotificationType
        GWNT_RULER_GUIDE_CHANGE                 enum GeoWorksNotificationType
        ;
        ;Bitmap library notification types
        ;
        GWNT_BITMAP_CURRENT_TOOL_CHANGE         enum GeoWorksNotificationType
        GWNT_BITMAP_CURRENT_FORMAT_CHANGE       enum GeoWorksNotificationType
        ;
        ;Flat File library notification types
        ;
        GWNT_FLAT_FILE_FIELD_PROPERTIES_STATUS_CHANGE enum GeoWorksNotificationType
        GWNT_FLAT_FILE_FIELD_LIST_CHANGE        enum GeoWorksNotificationType
        GWNT_FLAT_FILE_RCP_STATUS_CHANGE        enum GeoWorksNotificationType
        GWNT_FLAT_FILE_FIELD_APPEARANCE_CHANGE  enum GeoWorksNotificationType
        GWNT_FLAT_FILE_DUMMY_CHANGE_2           enum GeoWorksNotificationType
        GWNT_FLAT_FILE_DUMMY_CHANGE_3           enum GeoWorksNotificationType
        ;
        ;Spool library notification types
        ;
        GWNT_SPOOL_DOC_OR_PAPER_SIZE            enum GeoWorksNotificationType
        ;
        ;View control notification types
        ;
        GWNT_VIEW_STATE_CHANGE                  enum GeoWorksNotificationType
        ;
        ;Ink control notification types
        ;
        GWNT_INK_HAS_TARGET                     enum GeoWorksNotificationType
        ;
        ;Page control notification types
        ;
        GWNT_PAGE_STATE_CHANGE                  enum GeoWorksNotificationType
        ;
        ;Document control notification types
        ;
        GWNT_DOCUMENT_CHANGE                    enum GeoWorksNotificationType
        ;
        ;Display control notification types
        ;
        GWNT_DISPLAY_CHANGE                     enum GeoWorksNotificationTyp
        GWNT_DISPLAY_LIST_CHANGE                enum GeoWorksNotificationType
        ;
        ;Spline library notification types
        ;
        GWNT_SPLINE_MARKER_SHAPE                enum GeoWorksNotificationType
        GWNT_SPLINE_POINT                       enum GeoWorksNotificationType
        GWNT_SPLINE_POLYLINE                    enum GeoWorksNotificationType
        GWNT_SPLINE_SMOOTHNESS                  enum GeoWorksNotificationType
        GWNT_SPLINE_OPEN_CLOSE_CHANGE           enum GeoWorksNotificationType
        ;
        GWNT_UNUSED_1                           enum GeoWorksNotificationType
        ;
        ;Spreadsheet control notification types
        ;
        GWNT_SPREADSHEET_ACTIVE_CELL_CHANGE     enum GeoWorksNotificationType
        GWNT_SPREADSHEET_EDIT_BAR_CHANGE        enum GeoWorksNotificationType
        GWNT_SPREADSHEET_SELECTION_CHANGE       enum GeoWorksNotificationType
        GWNT_SPREADSHEET_CELL_WIDTH_HEIGHT_CHANGE enum GeoWorksNotificationType
        GWNT_SPREADSHEET_DOC_ATTR_CHANGE        enum GeoWorksNotificationType
        GWNT_SPREADSHEET_CELL_ATTR_CHANGE       enum GeoWorksNotificationType
        GWNT_SPREADSHEET_CELL_NOTES_CHANGE      enum GeoWorksNotificationType
        GWNT_SPREADSHEET_DATA_RANGE_CHANGE      enum GeoWorksNotificationType
        ;
        ;Float library notification types
        ;
        GWNT_FLOAT_FORMAT_CHANGE                enum GeoWorksNotificationType
        ;
        ;Impex mapping control notification types
        ;
        GWNT_MAP_APP_CHANGE                     enum GeoWorksNotificationType
        GWNT_MAP_LIBRARY_CHANGE                 enum GeoWorksNotificationType
        ;
        ;Transfer notification types
        ;
        GWNT_TEXT_NAME_CHANGE                   enum GeoWorksNotificationType
        ;
        ;Card library notification types
        ;
        GWNT_CARD_BACK_CHANGE                   enum GeoWorksNotificationType
        ;
        ;
        GWNT_TEXT_OBJECT_HAS_FOCUS              enum GeoWorksNotificationType
        GWNT_TEXT_CONTEXT                       enum GeoWorksNotificationType
        GWNT_TEXT_REPLACE_WITH_HWR              enum GeoWorksNotificationType
        ;
        ;Help notification types
        ;
        GWNT_HELP_CONTEXT_CHANGE                enum GeoWorksNotificationType
        ;
        GWNT_FLOAT_FORMAT_INIT                  enum GeoWorksNotificationType
        ;
        ;Hard Icon Bar notification types
        ;
        GWNT_HARD_ICON_BAR_FUNCTION             enum GeoWorksNotificationType
        GWNT_STARTUP_INDEXED_APP                enum GeoWorksNotificationType
        ;
        GWNT_SPOOL_PRINTING_COMPLETE            enum GeoWorksNotificationType
        GWNT_MODAL_WIN_CHANGE                   enum GeoWorksNotificationType
        GWNT_SPREADSHEET_NAME_CHANGE            enum GeoWorksNotificationType
        GWNT_DOCUMENT_OPEN_COMPLETE             enum GeoWorksNotificationType
        GWNT_EMAIL_SCAN_INBOX                   enum GeoWorksNotificationType
        GWNT_FOCUS_WINDOW_KBD_STATUS            enum GeoWorksNotificationType
        GWNT_TAB_DOUBLE_CLICK                   enum GeoWorksNotificationType
        GWNT_PAGE_INFO_STATE_CHANGE             enum GeoWorksNotificationType
        GWNT_CURSOR_POSITION_CHANGE             enum GeoWorksNotificationType
        GWNT_FAX_NEW_JOB_CREATED                enum GeoWorksNotificationType
        GWNT_FAX_NEW_JOB_COMPLETED              enum GeoWorksNotificationType
        GWNT_EMAIL_DATABASE_CHANGE              enum GeoWorksNotificationType
        GWNT_EMAIL_STATUS_CHANGE                enum GeoWorksNotificationType
        GWNT_EMAIL_PAGE_PANEL_UPDATE            enum GeoWorksNotificationType
        GWNT_PCCOM_DISPLAY_CHAR                 enum GeoWorksNotificationType
        GWNT_PCCOM_DISPLAY_STRING               enum GeoWorksNotificationType
        GWNT_PCCOM_EXIT                         enum GeoWorksNotificationType

GWNT_INK  
Objects on this list receive notification of data collected as ink. 
This notification passes the handle of a data block holding an 
**InkHeader** structure (containing a series of ink points) in bp. 
If the handle is null, the system could not allocate memory to 
hold all the points, or was intercepted by an Input Monitor.

Note: If a monitor intercepts 
MSG_META_NOTIFY_WITH_DATA_BLOCK with GWNT_INK, it 
must still pass it on, but may pass on **bp**=0 if it wants to 
consume the ink data itself.

Format of data:  
**InkHeader** <>  
**Point**<>  
**Point**<>  
**...**  
**...**  
The high bit of the *x* coord is set to denote the end of a line 
segment.

The points are all in the screen coordinates ; objects may want 
to convert them into their own window coordinates using 
**WinUntransform()**.

GWNT_GEN_CONTROL_NOTIFY_STATUS_CHANGE  
Objects on this list receive notification of a status change in 
GenControl objects. This notification passes the data type 
**NotifyGenControlStatusChange**.

GWNT_SELECT_STATE_CHANGE  
Objects on this list are notified when a selection state has 
changed within a GenEditControl (cut/copy/paste). This type 
uses MSG_META_NOTIFY_WITH_DATA_BLOCK. This 
notification passes the data type **NotifySelectStateChange**.

GWNT_UNDO_STATE_CHANGE  
This notification passes the data type 
**NotifyUndoStateChange**.

GWNT_STYLE_CHANGE  
Objects on this list (a style sheet control) are notified when a 
style change occurs . This notification passes the data type 
**NotifyStyleChange**.

GWNT_STYLE_SHEET_CHANGE  
This notification passes the data type 
**NotifyStyleSheetChange**.

GWNT_TEXT_CHAR_ATTR_CHANGE  
This notification passes the data type 
**VisTextNotifyCharAttrChange**.

GWNT_TEXT_PARA_ATTR_CHANGE  
This notification passes the data type 
**VisTextNotifyParaAttrChange**.

GWNT_TEXT_TYPE_CHANGE  
This notification passes the data type 
**VisTextNotifyTypeChange**.

GWNT_TEXT_SELECTION_CHANGE  
This notification passes the data type 
**VisTextNotifySelectionChange**.

GWNT_TEXT_COUNT_CHANGE  
This notification passes the data type 
**VisTextNotifyCountChange**.

GWNT_TEXT_STYLE_CHANGE  
This notification passes the data type 
**NotifyTextStyleChange**.

GWNT_FONT_CHANGE  
This notification passes the data type **NotifyFontChange**.

GWNT_POINT_SIZE_CHANGE  
This notification passes the data type 
**NotifyPointSizeChange**.

GWNT_FONT_ATTR_CHANGE  
This notification passes the data type 
**NotifyFontAttrChange**.

GWNT_JUSTIFICATION_CHANGE  
This notification passes the data type 
**NotifyJustificationChange**.

GWNT_TEXT_FG_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GWNT_TEXT_BG_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GWNT_TEXT_PARA_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GWNT_TEXT_BORDER_COLOR_CHANGE  
This notification passes the data type **NotifyColorChange**.

GWNT_SEARCH_REPLACE_ENABLE_CHANGE  
This notification passes the data type 
**NotifySearchReplaceEnableChange**.

GWNT_SPELL_ENABLE_CHANGE  
This notification passes the data type 
**NotifySpellEnableChange**.

GWNT_CHART_TYPE_CHANGE  
Objects on this list receive notification when the chart type 
changes.

GWNT_SPOOL_DOC_OR_PAPER_SIZE  
This notification passes the data type **PageSizeReport**.

GWNT_VIEW_STATE_CHANGE  
This notification passes the data type 
**NotifyViewStateChange**.

GWNT_INK_HAS_TARGET  
Send with MSG_META_NOTIFY (**bp** = non-zero if we have the 
target).

GWNT_PAGE_STATE_CHANGE  
This notification passes the data type 
**NotifyPageStateChange**.

GWNT_DOCUMENT_CHANGE  
This notification passes the data type 
**NotifyDocumentChange**.

GWNT_DISPLAY_CHANGE  
This notification passes the data type **NotifyDisplayChange**.

GWNT_DISPLAY_LIST_CHANGE  
This notification passes the data type 
**NotifyDisplayListChange**.

GWNT_SPREADSHEET_ACTIVE_CELL_CHANGE  
This notification passes the data type 
**NotifySSheetActiveCellChange**.

GWNT_SPREADSHEET_EDIT_BAR_CHANGE  
This notification passes the data type 
**NotifySSheetEditBarChange**.

GWNT_SPREADSHEET_SELECTION_CHANGE  
This notification passes the data type 
**NotifySSheetSelectionChange**.

GWNT_SPREADSHEET_CELL_WIDTH_HEIGHT_CHANGE  
This notification passes the data type 
**NotifySSheetCellWidthHeightChange**.

GWNT_SPREADSHEET_DOC_ATTR_CHANGE  
This notification passes the data type 
**NotifySSheetDocAttrChange**.

GWNT_SPREADSHEET_CELL_ATTR_CHANGE  
This notification passes the data type 
**NotifySSheetCellAttrChange**.

GWNT_SPREADSHEET_DATA_RANGE_CHANGE  
This notification passes the data type 
**NotifySSheetDataRangeChange**.

GWNT_TEXT_NAME_CHANGE  
This notification passes the data type 
**VisTextNotifyNameChange**.

GWNT_TEXT_REPLACE_WITH_HWR  
This notification passes the data type **InkHeader** (and ink 
data), followed by a **ReplaceWithHWRData** structure.

GWNT_HELP_CONTEXT_CHANGE  
This notification passes the data type **NotifyHelpChange**.

GWNT_HARD_ICON_BAR_FUNCTION  
GenApplication objects on this list will perform the indicated 
function when receiving this notification.

GWNT_STARTUP_INDEXED_APP  
Objects on this list start up the passed application when 
receiving this notification.

GWNT_SPOOL_PRINTING_COMPLETE  
Objects on this list receive notification that printing has been 
completed. The spooler does not send this out, but instead 
delays MSG_META_SEND_CLASSED_EVENTs that are sent to it 
having this as the encapsulated message, until printing is 
completed. This list is used in remote (IACP) printing.

GWNT_MODAL_WIN_CHANGE   
Objects on this list receive notification that the modal status of 
the application has changed in some way, by becoming modal, 
non-modal, or simply changing which window is modal. 

GWNT_SPREADSHEET_NAME_CHANGE  
Notification that a name has been added, deleted or changed.

GWNT_DOCUMENT_OPEN_COMPLETE  
The GenDocument does not send this out, but rather delays 
MSG_META_SEND_CLASSED_EVENT messages that are sent to 
it having this as the encapsulated message, until the document 
has either been opened (and is ready to be printed), or has had 
the open operation aborted somehow. 
NOTE: used for remote (IACP) printing - as of yet, there is no 
need for the document to delay this message, and so code to do 
that is not present.

GWNT_EMAIL_SCAN_INBOX  
Notify an email geode that it should check for new mail.

GWNT_FOCUS_WINDOW_KBD_STATUS  
On pen systems, this GCN Notification is sent from focus 
windows to the GAGCNLT_FOCUS_WINDOW_KBD_STATUS 
GCN List with the **NotifyFocusWindowKbdStatus** structure 
to tell the system what the floating keyboard should do.

GWNT_TAB_DOUBLE_CLICK  
Sent when a tab is double clicked on. This is an inelegant 
solution to allow backwards compatibility

GWNT_PAGE_INFO_STATE_CHANGE  
Sent when page info (size, margin) changes.
Data type: **NotifyPageInfoChange**

GWNT_CURSOR_POSITION_CHANGE  
Data type: **VisTextCursorPositionChange**

GWNT_FAX_NEW_JOB_CREATED  
Notify the fax spooler that there is a new fax file created.

GWNT_FAX_NEW_JOB_COMPLETED  
Notify the fax spooler that the new fax file is completely 
generated.

GWNT_EMAIL_DATABASE_CHANGE  
Notify email app that database has changed.

GWNT_EMAIL_STATUS_CHANGE  
Notify email app of current communications state.

GWNT_EMAIL_PAGE_PANEL_UPDATE  
Ask the page panel to enable or disable features.

GWNT_PCCOM_DISPLAY_CHAR  
Sent when there is a character ready to be displayed
Data type: char

GWNT_PCCOM_DISPLAY_STRING  
Sent when there is a string ready to be displayed
Data type: **MemHandle** of block containing the 
null-terminated string.

GWNT_PCCOM_EXIT  
Sent when the remote machine has sent the exit command and 
pccom has exited.
Data type: **PCComReturnType** from the call to PCCOMEXIT.

**Library:** geoworks.def

----------
#### GeoWorksPrefDialogGCNListType
    GeoWorksPrefDialogGCNListType   etype word, first PrefDialogMessages, 2
        PDGCNLT_DIALOG_CHANGE           enum GeoWorksPrefDialogGCNListType
        ; MSG_PREF_NOTIFY_DIALOG_CHANGE sent out.

**Library:** config.def

----------
#### GeoWorksVisContentGCNListType
    GeoWorksVisContentGCNListType               etype word , 
        FIRST_VIS_CONTENT_GCN_LIST_TYPE, 2
        VCGCNLT_TARGET_NOTIFY_TEXT_PARA_ATTR_CHANGE enum 
                                            GeoWorksVisContentGCNListType
        ;Data type: VisTextNotifyParaAttrChange

**Library:** geoworks.def

----------
#### GestureType
    GestureType     etype word
        GT_NO_GESTURE               enum GestureType
        GT_DELETE_CHARS             enum GestureType
        GT_SELECT_CHARS             enum GestureType
        GT_V_CROSSOUT               enum GestureType
        GT_H_CROSSOUT               enum GestureType
        GT_BACKSPACE                enum GestureType

**Library:** hwr.def

----------
#### GetContextParams
    GetContextParams        struct
        GCP_replyObj        optr    ;Output to reply to via MSG_META_CONTEXT
        GCP_numCharsToGet   word    ;Maximum number of characters to return
        GCP_location        ContextLocation
        GCP_position        dword
    GetContextParams        ends

**Library:** Objects/vTextC.def

----------
#### GetItemMonikerParams
    GetItemMonikerParams        struct
        GIMP_identifier             word
        GIMP_bufferSize             word
        GIMP_buffer                 fptr.char
    GetItemMonikerParams        ends

**Library:** config.def

----------
#### GetMaskType
    GetMaskType     etype byte
        GMT_ENUM        enum GetMaskType
        GMT_BUFFER      enum GetMaskType

**Library:** graphics.def

----------
#### GetPalType
    GetPalType      etype byte
        GPT_ACTIVE          enum GetPalType
        GPT_DEFAULT         enum GetPalType

**Library:** color.def

----------
#### GetPathType
    GetPathType     etype   word
        GPT_CURRENT     enum GetPathType    ; current path
        GPT_CLIP        enum GetPathType    ; clip path
        GPT_WIN_CLIP    enum GetPathType    ; win clip path

Use this type with **GrGetPath** to determine which sort of path to get.

**Library:** 

----------
#### GetSearchSpellObjectParam
    GetSearchSpellObjectParam               record
        GSSOP_RELAYED_FLAG      :1
                                :11
        GSSOP_TYPE              GetSearchSpellObjectType:4
    GetSearchSpellObjectParam               end

**Library:** Objects/vTextC.def

----------
#### GetSearchSpellObjectType
    GetSearchSpellObjectType            etype word
        GSSOT_FIRST_OBJECT              enum GetSearchSpellObjectType
        GSSOT_LAST_OBJECT               enum GetSearchSpellObjectType
        GSSOT_NEXT_OBJECT               enum GetSearchSpellObjectType
        GSSOT_PREV_OBJECT               enum GetSearchSpellObjectType

GSSOT_FIRST_OBJECT  
This type indicates to the spell checker to start spell checking 
from the first object encountered in the document when the 
user clicks on "Check Entire Document." It is also used by the 
search code to wrap a search to the beginning after it has 
reached the end.

GSSOT_LAST_OBJECT  
This type indicates to the spell checker to wrap a backwards 
search around to the end.

GSSOT_NEXT_OBJECT  
This type indicates to the spell checker to go to the next object 
in which to continue spell checking. At the end of the chain of 
objects, it should return 0:0.

GSSOT_PREV_OBJECT  
This type indicates to the spell checker to go to the previous 
object when continuing spell checking. After reaching the start 
of the chain, it should return 0:0.

**Library:** Objects/vTextC.def

----------
#### GetVarDataParams
    GetVarDataParams        struct
        GVDP_buffer             fptr
        GVDP_bufferSize         word
        GVDP_dataType           word
    GetVarDataParams        ends

*GVDP_buffer* stores the pointer to the buffer to fill with data from the 
VarData entry. This must be passed unless *GVDP_bufferSize* is 0.

*GVDP_bufferSize* stores the size of the above buffer (to allow us to prevent 
overflow). This must be set to zero if no buffer is passed.

*GVDP_dataType* stores the VarData type whose data should be returned.

**Library:** Objects/metaC.def

----------
#### GFM_info
    GFM_info        etype word, 0, 2
        GFMI_HEIGHT                 enum GFM_info   ;height of font box
        GFMI_MEAN                   enum GFM_info   ;top of lowers
        GFMI_DESCENT                enum GFM_info   ;descent of lowers
        GFMI_BASELINE               enum GFM_info   ;baseline offset
        GFMI_LEADING                enum GFM_info   ;external leading
        GFMI_AVERAGE_WIDTH          enum GFM_info   ;average char width
        GFMI_ASCENT                 enum GFM_info   ;ascent line to baseline
        GFMI_MAX_WIDTH              enum GFM_info   ;widest char width
        GFMI_MAX_ADJUSTED_HEIGHT    enum GFM_info   ;height, adjusted, with 
                                                    ;above/below
        GFMI_UNDER_POS              enum GFM_info   ;offset to underline
        GFMI_UNDER_THICKNESS        enum GFM_info   ;thickness of underline
        GFMI_ABOVE_BOX              enum GFM_info   ;height of above box
        GFMI_ACCENT                 enum GFM_info   ;height of accent
        GFMI_DRIVER                 enum GFM_info   ;driver ID
        GFMI_KERN_COUNT             enum GFM_info   ;# of kerning pairs
        GFMI_FIRST_CHAR             enum GFM_info   ;first character in font
        GFMI_LAST_CHAR              enum GFM_info   ;last character in font
        GFMI_DEFAULT_CHAR           enum GFM_info   ;default character for font
        GFMI_STRIKE_POS             enum GFM_info   ;strike-through position
        GFMI_BELOW_BOX              enum GFM_info   ;height of below box

**Library:** font.def

----------
#### GFSTempDataEntry
    GFSTempDataEntry        struct
        GFSTDE_selectionNumber      word
        GFSTDE_selectionFlags       GenFileSelectorEntryFlags
    GFSTempDataEntry        ends

**Library:** Objects/gFSelC.def

----------
#### GOAACFeatures
    GOAACFeatures       record
        GOAACF_MM_CLEAR             :1
        GOAACF_MM_COPY              :1
        GOAACF_MM_NOP               :1
        GOAACF_MM_AND               :1
        GOAACF_MM_INVERT            :1
        GOAACF_MM_XOR               :1
        GOAACF_MM_SET               :1
        GOAACF_MM_OR                :1
        GOAACF_TRANSPARENCY         :1
        GOAACFeatures       end

**Library:** grobj.def

----------
#### GOArcCFeatures
    GOArcCFeatures      record
        GOACF_START_ANGLE           :1
        GOACF_END_ANGLE             :1
        GOACF_PIE_TYPE              :1
        GOACF_CHORD_TYPE            :1
    GOArcCFeatures      end

**Library:** grobj.def

----------
#### GOATGCFeatures
    GOATGCFeatures      record
        GOATGCF_ALIGN_TO_GRID       :1
    GOATGCFeatures      end

**Library:** grobj.def

----------
#### GOCCFeatures
    GOCCFeatures        record
        GOCCF_CONVERT_TO_BITMAP         :1
        GOCCF_CONVERT_TO_GRAPHIC        :1
        GOCCF_CONVERT_FROM_GRAPHIC      :1
    GOCCFeatures        end

**Library:** grobj.def

----------
#### GOCDCFeatures
    GOCDCFeatures       record
        GOCDCF_REPITITIONS      :1
        GOCDCF_MOVE             :1
        GOCDCF_SCALE            :1
        GOCDCF_ROTATE           :1
        GOCDCF_SKEW             :1
    GOCDCFeatures       end

**Library:** grobj.def

----------
#### GOCSCFeatures
    GOCSCFeatures       record
        GOCSCF_NUM_POLYGON_SIDES        :1
        GOCSCF_POLYGON_RADIUS           :1
        GOCSCF_NUM_STAR_POINTS          :1
        GOCSCF_STAR_RADII               :1
    GOCSCFeatures       end

**Library:** grobj.def

----------
#### GODACFeatures
    GODACFeatures       record
        GODACF_SET_DEFAULT_ATTRIBUTES       :1
    GODACFeatures       end

**Library:** grobj.def

----------
#### GODepthCFeatures
    GODepthCFeatures        record
        GODepthCF_BRING_TO_FRONT            :1
        GoDepthCF_SEND_TO_BACK              :1
        GoDepthCF_SHUFFLE_UP                :1
        GoDepthCF_SHUFFLE_DOWN              :1
    GODepthCFeatures        end

**Library:** grobj.def

----------
#### GODMCFeatures
    GODMCFeatures       record
        GODMCF_DRAFT_MODE               :1
    GODMCFeatures       end

**Library:** grobj.def

----------
#### GOFCFeatures
    GOFCFeatures        record
        GOFCF_FLIP_HORIZONTALLY             :1
        GOFCF_FLIP_VERTICALLY               :1
    GOFCFeatures        end

**Library:** grobj.def

----------
#### GOGCFeatures
    GOGCFeatures        record
        GOGCF_GROUP             :1
        GOGCF_UNGROUP           :1
    GOGCFeatures        end

**Library:** grobj.def

----------
#### GOHCFeatures
    GOHCFeatures        record
        GOHCF_SMALL_HANDLES             :1
        GOHCF_MEDIUM_HANDLES            :1
        GOHCF_LARGE_HANDLES             :1
        GOHCF_INVISIBLE_HANDLES         :1
    GOHCFeatures        end

**Library:** grobj.def

----------
#### GOHSCFeatures
    GOHSCFeatures       record
        GOHSCF_HIDE         :1
        GOHSCF_SHOW         :1
    GOHSCFeatures       end

**Library:** grobj.def

----------
#### GOLACFeatures
    GOLACFeatures       record
        GOLACF_WIDTH_INDEX          :1
        GOLACF_WIDTH_VALUE          :1
        GOLACF_STYLE                :1
        GOLACF_ARROWHEAD_TYPE       :1
        GOLACF_ARROWHEAD_WHICH_END  :1
    GOLACFeatures       end

**Library:** grobj.def

----------
#### GOLACToolboxFeatures
    GOLACToolboxFeatures            record
        GOLACTF_WIDTH_INDEX             :1
        GOLACTF_STYLE                   :1
    GOLACToolboxFeatures            end

**Library:** grobj.def

----------
#### GOPICFeatures
    GOPICFeatures   record
        GOPICF_PASTE_INSIDE             :1
        GOPICF_BREAKOUT_PASTE_INSIDE    :1
    GOPICFeatures   end

**Library:** grobj.def

----------
#### GOPICToolboxFeatures
    GOPICToolboxFeatures    record
        GOPICTF_PASTE_INSIDE            :1
        GOPICTF_BREAKOUT_PASTE_INSIDE   :1
    GOPICToolboxFeatures    end

**Library:** grobj.def

----------
#### GOTCFeatures
    GOTCFeatures        record
        GOTCF_PTR               :1
        GOTCF_ROTATE_PTR        :1
        GOTCF_ZOOM              :1
        GOTCF_TEXT              :1
        GOTCF_LINE              :1
        GOTCF_RECT              :1
        GOTCF_ROUNDED_RECT      :1
        GOTCF_ELLIPSE           :1
        GOTCF_ARC               :1
        GOTCF_POLYLINE          :1
        GOTCF_POLYCURVE         :1
        GOTCF_SPLINE            :1
    GOTCFeatures        end

**Library:** grobj.def

----------
#### GOTransformCFeatures
    GOTransformCFeatures            record
        GOTCF_UNTRANSFORM               :1
    GOTransformCFeatures            end

**Library:** grobj.def

----------
#### GPCFeatures
    GPCFeatures         record
        GPCF_GOTO_PAGE              :1
        GPCF_NEXT_PAGE              :1
        GPCF_PREVIOUS_PAGE          :1
    GPCFeatures         end

**Library:** gPageCC.def

----------
#### GPCToolboxFeatures
    GPCToolboxFeatures      record
        GPCTF_PREVIOUS_PAGE         :1
        GPCTF_GOTO_PAGE             :1
        GPCTF_NEXT_PAGE             :1
    GPCToolboxFeatures      end

**Library:** gPageCC.def

----------
#### GPICFeatures
    GPICFeatures            record
        GPICF_KEYBOARD                      :1
        GPICF_CHAR_TABLE                    :1
        GPICF_CHAR_TABLE_SYMBOLS            :1
        GPICF_CHAR_TABLE_INTERNATIONAL      :1
        GPICF_CHAR_TABLE_MATH               :1
        GPICF_CHAR_TABLE_CUSTOM             :1
        GPICF_HWR_ENTRY_AREA                :1
    GPICFeatures            end

**Library:** gPenICC.def

----------
#### GPICToolboxFeatures
    GPICToolboxFeatures         record
        GPICTF_INITIATE     :1
    GPICToolboxFeatures         end

**Library:** grobj.def

----------
#### GraphicPattern
    GraphicPattern      struct
        GP_type     PatternType
        GP_data     byte
    GraphicPattern      ends

This structure stores a system hatch pattern.

**Library:** graphics.def

----------
#### Grid
    Grid    struct
        G_x WWFixed         ;pixels between horiz gridlines
        G_y WWFixed         ;pixels between vert gridlines
    Grid    ends

**Library:** ruler.def

----------
#### GridOptions
    GridOptions     record
        GO_SHOW_GRID            :1
        GO_SNAP_TO_GRID         :1
                                :6
    GridOptions     end

**Library:** ruler.def

----------
#### GrInfoType
    GrInfoType      etype word, 0, 2
        GIT_PRIVATE_DATA        enum GrInfoType
        GIT_WINDOW              enum GrInfoType

**Library:** graphics.def

----------
#### GrObjActionModes
    GrObjActionModes        record
        GOAM_RESIZE                 :1
        GOAM_MOVE                   :1
        GOAM_ROTATE                 :1
        GOAM_CHOOSE                 :1
        GOAM_ACTION_ACTIVATED       :1
        GOAM_ACTION_PENDING         :1
        GOAM_ACTION_HAPPENING       :1
        GOAM_CREATE                 :1
    GrObjActionModes        end

**Library:** grobj.def

----------
#### GrObjActionNotificationStruct
    GrObjActionNotificationStruct               struct
        GOANS_suspendCount      word    ; If non-zero, then defer sending out 
                                        ; action notification.
        GOANS_optr              optr    ; OD to send message to.
    GrObjActionNotificationStruct               ends

**Library:** grobj.def

----------
#### GrObjActionNotificationType
    GrObjActionNotificationType             etype word
        GOANT_NULL              enum GrObjActionNotificationType
                ; Reserve zero as a special value
        GOANT_SELECTED          enum GrObjActionNotificationType
        GOANT_UNSELECTED        enum GrObjActionNotificationType
        GOANT_CREATED           enum GrObjActionNotificationType
        GOANT_MOVED             enum GrObjActionNotificationType
        GOANT_RESIZED           enum GrObjActionNotificationType
        GOANT_ROTATED           enum GrObjActionNotificationType
        GOANT_SKEWED            enum GrObjActionNotificationType
        GOANT_TRANSFORMED       enum GrObjActionNotificationType
        GOANT_ATTRED            enum GrObjActionNotificationType
        GOANT_SPEC_MODIFIED     enum GrObjActionNotificationType
        GOANT_PASTED            enum GrObjActionNotificationType
        GOANT_DELETED           enum GrObjActionNotificationType
        GOANT_WRAP_CHANGED      enum GrObjActionNotificationType
        GOANT_UNDO_GEOMETRY     enum GrObjActionNotificationType
        GOANT_UNDO_DELETE       enum GrObjActionNotificationType
        GOANT_REDO_DELETE       enum GrObjActionNotificationType
        GOANT_PRE_MOVE          enum GrObjActionNotificationType
        GOANT_PRE_RESIZE        enum GrObjActionNotificationType
        GOANT_PRE_ROTATE        enum GrObjActionNotificationType
        GOANT_PRE_SKEW          enum GrObjActionNotificationType
        GOANT_PRE_TRANSFORM     enum GrObjActionNotificationType
        GOANT_PRE_SPEC_MODIFY   enum GrObjActionNotificationType
        GOANT_QUERY_DELETE      enum GrObjActionNotificationType
        GOANT_PRE_WRAP_CHANGE   enum GrObjActionNotificationType

**Library:** grobj.def

----------
#### GrObjAlignDistributeControlFeatures
    GrObjAlignDistributeControlFeatures record
        GOADCF_ALIGN_LEFT                               :1
        GOADCF_ALIGN_CENTER_HORIZONTALLY                :1
        GOADCF_ALIGN_RIGHT                              :1
        GOADCF_ALIGN_WIDTH                              :1
        GOADCF_ALIGN_TOP                                :1
        GOADCF_ALIGN_CENTER_VERTICALLY                  :1
        GOADCF_ALIGN_BOTTOM                             :1
        GOADCF_ALIGN_HEIGHT                             :1
        GOADCF_DISTRIBUTE_LEFT                          :1
        GOADCF_DISTRIBUTE_CENTER_HORIZONTALLY           :1
        GOADCF_DISTRIBUTE_RIGHT                         :1
        GOADCF_DISTRIBUTE_WIDTH                         :1
        GOADCF_DISTRIBUTE_TOP                           :1
        GOADCF_DISTRIBUTE_CENTER_VERTICALLY             :1
        GOADCF_DISTRIBUTE_BOTTOM                        :1
        GOADCF_DISTRIBUTE_HEIGHT                        :1
    GrObjAlignDistributionControlFeatures end

**Library:** grobj.def

----------
#### GrObjAnchoredScaleData
    GrObjAnchoredScaleData          struct
        GOASD_scale             GrObjScaleData
        GOASD_scaleAnchor       GrObjHandleSpecification
        align                   word
    GrObjAnchoredScaleData          ends

**Library:** grobj.def

----------
#### GrObjAnchoredSkewData
    GrObjAnchoredSkewData           struct
        GOASD_degrees           GrObjSkewData
        GOASD_skewAnchor        GrObjHandleSpecification
        align                   word
    GrObjAnchoredSkewData           ends

**Library:** grobj.def

----------
#### GrObjAreaAttrElementType
    GrObjAreaAttrElementType            etype byte
        GOAAET_BASE             enum GrObjAreaAttrElementType
        ;GrObjBaseAreaAttrElement
        GOAAET_GRADIENT         enum GrObjAreaAttrElementType
        ;GrObjGradientAreaAttrElement

**Library:** grobj.def

----------
#### GrObjAreaAttrInfoRecord
    GrObjAreaAttrInfoRecord         record
        GOAAIR_TRANSPARENT              :1
    GrObjAreaAttrInfoRecord         end

GOAAIR_TRANSPARENT  
This flag indicates a GrObj area is transparent. If this flag is set, then there 
is no need to redraw the background behind the object.

**Library:** grobj.def

----------
#### GrObjAttrFlags
    GrObjAttrFlags      record
                                                :6
        GOAF_DONT_COPY_LOCKS                    :1
        GOAF_HAS_PASTE_INSIDE_CHILDREN          :1
        GOAF_PASTE_INSIDE                       :1
        GOAF_INSERT_DELETE_MOVE_ALLOWED         :1
        GOAF_INSERT_DELETE_RESIZE_ALLOWED       :1
        GOAF_INSERT_DELETE_DELETE_ALLOWED       :1
        GOAF_INSTRUCTION                        :1
        GOAF_MULTIPLICATIVE_RESIZE              :1
        GOAF_WRAP                               GrObjWrapTextType:2
    GrObjAttrFlags      end

GOAF_DONT_COPY_LOCKS  
This flag indicates that when the object is written out to the 
transfer format, any locks will not be copied.

GOAF_HAS_PASTE_INSIDE_CHILDREN  
This flag indicates that the object contains paste inside 
children. (This is only relevant for objects in groups.)

GOAF_PASTE_INSIDE  
Meaningless unless object is in a group. This flag indicates that 
the object was pasted inside a group and will be drawn clipped 
to the group's normal children. (This is only relevant for objects 
in groups.)

GOAF_INSERT_DELETE_MOVE_ALLOWED  
This flag indicates that this GrObj can be moved as a result of 
a MSG_GO_INSERT_OR_DELETE_SPACE.

GOAF_INSERT_DELETE_RESIZE_ALLOWED  
This flag indicates that this GrObj can be resized as a result of 
MSG_GO_INSERT_OR_DELETE_SPACE.

GOAF_INSERT_DELETE_DELETE_ALLOWED  
This flag indicates that this GrObj can be deleted as a result of 
a MSG_GO_INSERT_OR_DELETE_SPACE.

GOAF_INSTRUCTION  
This flag indicates that this object is used for instructions in a 
template.

GOAF_MULTIPLICATIVE_RESIZE  
This flag indicates whether resize deltas are added to object 
coordinates. If true, a scale factor is calculated and applied to 
objects transform. If false, then resize deltas are not added to 
object coordinates.

GOAF_WRAP  
This type indicates how to wrap text with respect to the object.

**Library:** grobj.def

----------
#### GrObjAttributeManagerArrayDesc
    GrObjAttributeManagerArrayDesc                  struct
        GOAMAD_areaAttrArrayHandle          word
        GOAMAD_areaDefaultElement           word
        GOAMAD_lineAttrArrayHandle          word
        GOAMAD_lineDefaultElement           word
        GOAMAD_grObjStyleArrayHandle        word
        GOAMAD_charAttrArrayHandle          word
        GOAMAD_charDefaultElement           word
        GOAMAD_paraAttrArrayHandle          word
        GOAMAD_paraDefaultElement           word
        GOAMAD_typeArrayHandle              word
        GOAMAD_typeDefaultElementword       word
        GOAMAD_graphicArrayHandle           word
        GOAMAD_nameArrayHandle              word
        GOAMAD_textStyleArrayHandle         word
    GrObjAttributeManagerArrayDesc                  ends

*GOAMAD_areaAttrArrayHandle* stores the VM block handle of the element 
array for area attributes. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, an area attribute array 
will be created. 

*GOAMAD_areaDefaultElement* stores the element number of the default area 
attributes in the area attribute array. This value is ignored if 0 is passed in 
*GOAMAD_areaAttrArrayHandle*.

*GOAMAD_lineAttrArrayHandle* stores the VM block handle of the element 
array for line attributes. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a line attribute array will 
be created.

*GOAMAD_lineDefaultElement* stores the element number of the default line 
attributes in the line attribute array. This value is ignored if 0 is passed in 
*GOAMAD_inlineAttrArrayHandle*.

*GOAMAD_grObjStyleArrayHandle* stores the VM block handle of the element 
array for grobj styles. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a style array will be 
created.

*GOAMAD_charAttrArrayHandle* stores the VM block handle of the element 
array for character attributes. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a character attribute 
array will be created.

*GOAMAD_charDefaultElement* stores the element number of the default 
attributes in the character array. This value is ignored if 0 is passed in 
*GOAMAD_charAttrArrayHandle*.

*GOAMAD_paraAttrArrayHandle* stores the VM block handle of the element 
array for paragraph attributes. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a paragraph attribute 
array will be created.

*GOAMAD_paraDefaultElement* stores the element number of the default 
attributes in the paragraph array. This value is ignored if 0 is passed in 
*GOAMAD_paraAttrArrayHandle*.

*GOAMAD_typeArrayHandle* stores the VM block handle of the element array 
for types. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a type array will be 
created.

*GOAMAD_typeDefaultElement* stores the element number of the default 
attributes in a type array. This value is ignored if 0 is passed in 
*GOAMAD_paraAttrArrayHandle*.

*GOAMAD_graphicArrayHandle* stores the VM block handle of the element 
array for graphics. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a graphic array will be 
created.

*GOAMAD_nameArrayHandle* stores the VM block handle of the element array 
for names. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a name array will be 
created.

*GOAMAD_textStyleArrayHandle* stores the VM block handle of the element 
array for text styles. The associated chunk must be at the 
GROBJ_VM_ELEMENT_ARRAY_CHUNK offset. If 0, a style array will be 
created.

**Library:** grobj.def

----------
#### GrObjBaseAreaAttrDiffs
    GrObjBaseAreaAttrDiffs          record
        GOBAAD_MULTIPLE_ELEMENT_TYPES           :1
        GOBAAD_MULTIPLE_STYLE_ELEMENTS          :1
        GOBAAD_MULTIPLE_COLORS                  :1
        GOBAAD_MULTIPLE_BACKGROUND_COLORS       :1
        GOBAAD_MULTIPLE_MASKS                   :1
        GOBAAD_MULTIPLE_PATTERNS                :1
        GOBAAD_MULTIPLE_DRAW_MODES              :1
        GOBAAD_MULTIPLE_INFOS                   :1
        GOBAAD_MULTIPLE_GRADIENT_END_COLORS     :1
        GOBAAD_MULTIPLE_GRADIENT_TYPES          :1
        GOBAAD_MULTIPLE_GRADIENT_INTERVALS      :1
                                                :4
        GOBAAD_FIRST_RECIPIENT                  :1
    GrObjBaseAreaAttrDiffs          end

GOBAAD_FIRST_RECIPIENT  
This flag indicates that this GrObj is the first one to receive this data buffer 
(and should thereafter clear it).

**Library:** grobj.def

----------
#### GrObjBaseAreaAttrElement
    GrObjBaseAreaAttrElement            struct
        GOBAAE_styleElement     StyleSheetElementHeader
        GOBAAE_r                byte
        GOBAAE_g                byte
        GOBAAE_b                byte
        GOBAAE_mask             SystemDrawMask
        GOBAAE_drawMode         MixMode
        GOBAAE_pattern          GraphicPattern
        GOBAAE_backR            byte
        GOBAAE_backG            byte
        GOBAAE_backB            byte
        GOBAAE_aaeType          GrObjAreaAttrElementType
        GOBAAE_areaInfo         GrObjAreaAttrInfoRecord
        GOBAAE_reservedByte     byte        ; Currently unused. Must be 0.
        GOBAAE_reserved         word        ; Currently unused. Must be 0.
    GrObjBaseAreaAttrElement            ends

**Library:** grobj.def

----------
#### GrObjBaseLineAttrDiffs
    GrObjBaseLineAttrDiffs          record
        GOBLAD_MULTIPLE_STYLE_ELEMENTS                  :1
        GOBLAD_MULTIPLE_ELEMENT_TYPES                   :1
        GOBLAD_MULTIPLE_COLORS                          :1
        GOBLAD_MULTIPLE_ENDS                            :1
        GOBLAD_MULTIPLE_JOINS                           :1
        GOBLAD_MULTIPLE_WIDTHS                          :1
        GOBLAD_MULTIPLE_MASKS                           :1
        GOBLAD_MULTIPLE_STYLES                          :1
        GOBLAD_ARROWHEAD_ON_START                       :1
        GOBLAD_ARROWHEAD_ON_END                         :1
        GOBLAD_ARROWHEAD_FILLED                         :1
        GOBLAD_ARROWHEAD_FILL_WITH_AREA_ATTRIBUTES      :1
        GOBLAD_MULTIPLE_MITER_LIMITS                    :1
        GOBLAD_MULTIPLE_ARROWHEAD_ANGLES                :1
        GOBLAD_MULTIPLE_ARROWHEAD_LENGTHS               :1
        GOBLAD_FIRST_RECIPIENT                          :1
    GrObjBaseLineAttrDiffs          end

**Library:** grobj.def

----------
#### GrObjBaseLineAttrElement
    GrObjBaseLineAttrElement            struct
        GOBLAE_styleElement         StyleSheetElementHeader
        GOBLAE_r                    byte
        GOBLAE_g                    byte
        GOBLAE_b                    byte
        GOBLAE_end                  LineEnd
        GOBLAE_join                 LineJoin
        GOBLAE_width                WWFixed
        GOBLAE_mask                 SystemDrawMask
        GOBLAE_style                LineStyle
        GOBLAE_miterLimit           WWFixed
        GOBLAE_laeType              GrObjLineAttrElementType
        GOBLAE_lineInfo             GrObjLineAttrInfoRecord
        GOBLAE_arrowheadAngle       byte
        GOBLAE_arrowheadLength      byte
        GOBLAE_reserved             word    ; Currently unused. Must be 0.
    GrObjBaseLineAttrElement            ends

**Library:** grobj.def

----------
#### GrObjBlockHandleElement
    GrObjBlockHandleElement         struct
        GOBHE_blockHandle               hptr
        GOBHE_potentialSize             word
    GrObjBlockHandleElement         ends

**Library:** grobj.def

----------
#### GrObjBodyAddGrObjFlags
    GrObjBodyAddGrObjFlags          record
        GOBAGOF_DRAW_LIST_POSITION          :1
        GOBAGOF_REFERENCE                   :15
    GrObjBodyAddGrObjFlags          end

GOBAGOF_DRAW_LIST_POSITION  
If this flag is set, GOBAGOF_REFERENCE refers to the GrObj's position in the 
draw list; if the flag is clear, GOBAGOF_REFERENCE refers to its position in 
the reverse draw list.

**Library:** grobj.def

----------
#### GrObjBodyCreateGrObjParams
    GrObjBodyCreateGrObjParams              struct
        GBCGP_class     fptr.ClassStruct
        padding1        word        ; This padding ensures that width and 
                                    ; height align with GrObjInitializeData.
        padding2        word
        padding3        word
        padding4        word
        GBCGP_width     WWFixed
        GBCGP_height    WWFixed
    GrObjBodyCreateGrObjParams              ends

**Library:** grobj.def

----------
#### GrObjBodyCustomDuplicateParams
    GrObjBodyCustomDuplicateParams                  struct
        GBCDP_repetitions       word
        GBCDP_move              PointDWFixed
        GBCDP_rotation          WWFixed
        GBCDP_rotateAnchor      GrObjHandleSpecification
        GBCDP_skew              GrObjAnchoredSkewData
        GBCDP_scale             GrObjAnchoredScaleData
            align                   word
    GrObjBodyCustomDuplicateParams                  ends

**Library:** grobj.def

----------
#### GrObjBodyFlags
    GrObjBodyFlags          record
        GBF_HAS_ACTION_NOTIFICATION     :1
        GBF_DEFAULT_TARGET              :1
        GBF_DEFAULT_FOCUS               :1
    GrObjBodyFlags          end

GBF_HAS_ACTION_NOTIFICATION  
This flag is set if the G rObj body contains an action notification 
optr within its vardata.

GBF_DEFAULT_TARGET  
This flag is set if the GrObjBody will grab the default target 
upon a MSG_VIS_OPEN.

GBF_DEFAULT_FOCUS  
This flag is set if the GrObjBody will grab the default focus 
upon a MSG_VIS_OPEN.

**Library:** grobj.def

----------
#### GrObjBodyNotifyInstructionFlags
    GrObjBodyNotifyInstructionFlags             struct
        GBNIF_flags             GrObjDrawFlags
        GBNIF_handleSize        byte    ;this field added post-Zoomer
        align                   word
    GrObjBodyNotifyInstructionFlags             ends

**Library:** grobj.def

----------
#### GrObjBodyPasteCallBackStruct
    GrObjBodyPasteCallBackStruct            struct
        GOBPCBS_message             word
        GOBPCBS_optr                optr
    GrObjBodyPasteCallBackStruct            ends

**Library:** grobj.def

----------
#### GrObjBodyUnsuspendOps
    GrObjBodyUnsuspendOps           record
        GBUO_UI_NOTIFY  GrObjUINotificationTypes: width GrObjUINotificationTypes
    GrObjBodyUnsuspendOps           end

GBUO_UI_NOTIFY  
This field stores notifications that should be sent when the GrObjBody 
suspend counts returns to zero.

**Library:** grobj.def

----------
#### GrObjCreateControlFeatures
    GrObjCreateControlFeatures              record
        GOCCF_RECTANGLE             :1
        GOCCF_ELLIPSE               :1
        GOCCF_LINE                  :1
        GOCCF_ROUNDED_RECTANGLE     :1
        GOCCF_ARC                   :1
        GOCCF_TRIANGLE              :1
        GOCCF_HEXAGON               :1
        GOCCF_OCTOGON               :1
        GOCCF_FIVE_POINTED_STAR     :1
        GOCCF_EIGHT_POINTED_STAR    :1
    GrObjCreateControlFeatures              end

**Library:** grobj.def

----------
#### GrObjCreateGStateType
    GrObjCreateGStateType           etype word
        BODY_GSTATE     enum GrObjCreateGStateType,0    
                                ;Has only translation to body upper left 
                                ;in it. Useful for un-transforming device 
                                ;coordinates into document coordinates.
        PARENT_GSTATE   enum GrObjCreateGStateType  
                                ;Has body translation and group 
                                ;transformations in it.
        OBJECT_GSTATE   enum GrObjCreateGStateType

**Library:** grobj.def

----------
#### GrObjDefiningData
    GrObjDefiningData       struct
        GODD_attrFlags          GrObjAttrFlags
        GODD_locks              GrObjLocks
        GODD_areaToken          word
        GODD_lineToken          word
        GODD_normalTransform    ObjectTransform
    GrObjDefiningData       ends

This structure represents the subset of GrObj instance data required to 
recover the object from a transfer item.

**Library:** grobj.def

----------
#### GrObjDrawFlags
    GrObjDrawFlags              record
                                                :7
        GODF_DRAW_QUICK_VIEW                    :1
        GODF_DRAW_CLIP_ONLY                     :1
        GODF_DRAW_WRAP_TEXT_INSIDE_ONLY         :1
        GODF_DRAW_WRAP_TEXT_AROUND_ONLY         :1
        GODF_DRAW_WITH_INCREASED_RESOLUTION     :1
        GODF_DRAW_INSTRUCTIONS                  :1
        GODF_DRAW_SELECTED_OBJECTS_ONLY         :1
        GODF_DRAW_OBJECTS_ONLY                  :1
        GODF_PRINT_INSTRUCTIONS                 :1
    GrObjDrawFlags              end

GODF_DRAW_QUICK_VIEW  
If this flag is set, GrObjs will draw themselves with 
MSG_GO_DRAW_QUICK_VIEW. This message results in much 
faster drawing but is not WYSIWYG.

GODF_DRAW_CLIP_ONLY  
If this flag is set, GrObjs will only draw their clip area.

GODF_DRAW_WRAP_TEXT_INSIDE_ONLY  
If this flag is set, only GrObjs with GOAF_WRAP set to 
GOWTT_WRAP_INSIDE will draw.

GODF_DRAW_WRAP_TEXT_AROUND_ONLY  
If this flag is set, only GrObjs with GOAF_WRAP set to 
GOWTT_WRAP_AROUND_RECT or 
GOWTT_WRAP_AROUND_TIGHTLY will draw.

GODF_DRAW_WITH_INCREASED_RESOLUTION  
If this flag is set, the object should draw with more resolution 
(if possible). This flag is used for printing and when the view is 
scaled.

GODF_DRAW_INSTRUCTIONS  
If this flag is set, GrObjs with the GOAF_INSTRUCTION bit set 
will draw.

GODF_DRAW_SELECTED_OBJECTS_ONLY  
If this flag is set, only selected objects will draw.

GODF_DRAW_OBJECTS_ONLY  
If this flag is set, then only the GrObjs themselves should be 
drawn. (I.e. don't draw grid lines, sprites, handles, etc.)

GODF_PRINT_INSTRUCTIONS  
If this flag is set, instructions should be printed.

**Library:** grobj.def

----------
#### GrObjDuplicateControlFeatures
    GrObjDuplicateControlFeatures       record
        GODCF_DUPLICATE             :1
        GODCF_DUPLICATE_IN_PLACE    :1
    GrObjDuplicateControlFeatures       end

**Library:** grobj.def

----------
#### GrObjDuplicateControlToolboxFeatures
    GrObjDuplicateControlToolboxFeatures    record
        GODCTF_DUPLICATE                :1
        GODCTF_DUPLICATE_IN_PLACE       :1
    GrObjDuplicateControlToolboxFeatures    end

**Library:** 

----------
#### GrObjEntryPointRelocation
    GrObjEntryPointRelocation               struct
        GOEPR_fullRelocation            EntryPointRelocation
        GOEPR_grObjEntryPoint           word
    GrObjEntryPointRelocation               ends

**Library:** grobj.def

----------
#### GrObjFileStatus
    GrObjFileStatus     record
        GOFS_MOUSE_GRAB     :1      ;True if body has mouse grab
        GOFS_SYS_TARGETED   :1      ;Body has the system target excl
        GOFS_TARGETED       :1      ;Body or one of its children has target
        GOFS_OPEN           :1      ;True if file is open.
    GrObjFileStatus     end

**Library:** grobj.def

----------
#### GrObjFullAreaAttrElement
    GrObjFullAreaAttrElement                struct
        GOFAAE_base     GrObjBaseAreaAttrElement
        GOFAAE_future   byte FUTURE_AREA_ATTR_ELEMENT_DATA_SIZE dup (?)
    GrObjFullAreaAttrElement                ends

This structure is used to allow future routines to access larger 
GrObjBaseAreaAttrElement structures.

**Library:** grobj.def

----------
#### GrObjFullLineAttrElement
    GrObjFullLineAttrElement                struct
        GOFLAE_base     GrObjBaseLineAttrElement
        GOFLAE_future   byte FUTURE_LINE_ATTR_ELEMENT_DATA_SIZE dup (?)
    GrObjFullLineAttrElement                ends

This structure is used to allow future routines to access larger 
GrObjBaseLineAttrElement structures.

**Library:** grobj.def

----------
#### GrObjFunctionsActive
    GrObjFunctionsActive                record
        GOFA_RULER_HAS_SEEN_EVENT   :1
        GOFA_VIEW_ZOOMED            :1
        GOFA_SNAP_TO                :1
        GOFA_FROM_CENTER            :1
        GOFA_ABOUT_OPPOSITE         :1
        GOFA_CONSTRAIN              :1
        GOFA_ADJUST                 :1
        GOFA_EXTEND                 :1
                                    :2
    GrObjFunctionsActive                end

GOFA_RULER_HAS_SEEN_EVENT  
If set, the mouse event has been sent to the ruler already. This 
is used to prevent snapping the mouse more often than is 
needed. For example, when moving multiple GrObjs, you only 
want to snap the mouse once and not for each GrObj.

GOFA_VIEW_ZOOMED  
If set, all drawing operations should be done in high resolution 
mode.

GOFA_SNAP_TO  
If set, operations should be snapped to the grid.

GOFA_FROM_CENTER  
If set, any resize or create operations should be performed from 
the center. 

GOFA_ABOUT_OPPOSITE  
If set, rotations should be performed about the opposite corner.

GOFA_CONSTRAIN  
If set, constrain resize, rotate, etc.

GOFA_ADJUST  
Same as UIFA_ADJUST.

GOFA_EXTEND  
Same as UIFA_EXTEND.

**Library:** grobj.def

----------
#### GrObjGradientAreaAttrElement
    GrObjGradientAreaAttrElement            struct
        GOGAAE_base             GrObjBaseAreaAttrElement
        GOGAAE_type             GrObjGradientType
        GOGAAE_endR             byte            ;ending color red   byte
        GOGAAE_endG             byte            ;ending color green byte
        GOGAAE_endB             byte            ;ending color blue byte
        GOGAAE_numIntervals     word
        GOGAAE_reserved         word
    GrObjGradientAreaAttrElement            ends

**Library:** grobj.def

----------
#### GrObjGradientAttrDiffs
    GrObjGradientAttrDiffs              record
        GGAD_MULTIPLE_END_COLORS    :1
        GGAD_MULTIPLE_TYPES         :1
        GGAD_MULTIPLE_INTERVALS     :1
                                    :4
        GGAD_FIRST_RECIPIENT        :1
    GrObjGradientAttrDiffs              end

GGAD_FIRST_RECIPIENT  
If set, the GrObj knows that it's the first one to receive this data buffer (and 
should clear it).

**Library:** grobj.def

----------
#### GrObjGradientFillControlFeatures
    GrObjGradientFillControlFeatures                record
        GOGFCF_HORIZONTAL_GRADIENT              :1
        GOGFCF_VERTICAL_GRADIENT                :1
        GOGFCF_RADIAL_RECT_GRADIENT             :1
        GOGFCF_RADIAL_ELLIPSE_GRADIENT          :1
        GOGFCF_NUM_INTERVALS                    :1
    GrObjGradientFillControlFeatures                end

**Library:** grobj.def

----------
#### GrObjGradientType
    GrObjGradientType       etype byte
        GOGT_NONE                   enum GrObjGradientType
        GOGT_LEFT_TO_RIGHT          enum GrObjGradientType
        GOGT_TOP_TO_BOTTOM          enum GrObjGradientType
        GOGT_RADIAL_RECT            enum GrObjGradientType
        GOGT_RADIAL_ELLIPSE         enum GrObjGradientType

**Library:** grobj.def

----------
#### GrObjHandleAnchorData
    GrObjHandleAnchorData           struct
        GOHAD_anchor            PointDWFixed
        GOHAD_handle            GrObjHandleSpecification
        align                   word
    GrObjHandleAnchorData           ends

**Library:** grobj.def

----------
#### GrObjHandleSpecification
    GrObjHandleSpecification            record
        GOHS_HANDLE_LEFT            :1
        GOHS_HANDLE_TOP             :1
        GOHS_HANDLE_RIGHT           :1
        GOHS_HANDLE_BOTTOM          :1
    GrObjHandleSpecification            end

**Library:** grobj.def

----------
#### GrObjInitializeData
    GrObjInitializeData         struct
        GOID_position       PointDWFixed
        GOID_width          WWFixed
        GOID_height         WWFixed
        GrObjInitializeData         ends

*GOID_position* stores the position of the upper left corner of the object in 
parent coordinates.

*GOID_width* stores the width of the object in points.

*GOID_height* stores the height of the object in points.

**Library:** grobj.def

----------
#### GrObjInstructionControlFeatures
    GrObjInstructionControlFeatures         record
        GOICF_DRAW                  :1
        GOICF_PRINT                 :1
        GOICF_MAKE_EDITABLE         :1
        GOICF_MAKE_UNEDITABLE       :1
        GOICF_DELETE                :1
    GrObjInstructionControlFeatures         end

**Library:** grobj.def

----------
#### GrObjLineAttrElementType
    GrObjLineAttrElementType            etype byte
        GOLAET_BASE             enum GrObjLineAttrElementType
        ;GrObjBaseLineAttrElement

**Library:** grobj.def

----------
#### GrObjLineAttrInfoRecord
    GrObjLineAttrInfoRecord         record
        GOLAIR_ARROWHEAD_ON_START                       :1
        GOLAIR_ARROWHEAD_ON_END                         :1
        GOLAIR_ARROWHEAD_FILLED                         :1
        GOLAIR_ARROWHEAD_FILL_WITH_AREA_ATTRIBUTES      :1
                                                        :4
    GrObjLineAttrInfoRecord         end

**Library:** grobj.def

----------
#### GrObjLocks
    GrObjLocks      record
        GOL_COPY        :1  ;True if object may not be transferred to the 
                            ;clipboard
        GOL_LOCK        :1  ;True if object cannot have its locks changed
        GOL_SHOW        :1  ;True if object can't be drawn/selected/edited
                            ;Used with MSG_GB_HIDE_UNSELECTED_OBJECTS
                            ;and MSG_GB_SHOW_ALL_OBJECTS
        GOL_WRAP        :1  ;True if can't change wrap type
        GOL_MOVE        :1  ;True if object cannot be moved
        GOL_RESIZE      :1  ;True if object cannot be resized
        GOL_ROTATE      :1  ;True if object cannot be rotated
        GOL_SKEW        :1  ;True if object cannot be skewed
        GOL_EDIT        :1  ;True if object cannot be edit
        GOL_DELETE      :1  ;True if object cannot be deleted
        GOL_SELECT      :1  ;True if object cannot be selected
        GOL_ATTRIBUTE   :1  ;True if object cannot changeattributes
        GOL_GROUP       :1  ;True if object cannot be grouped
        GOL_UNGROUP     :1  ;True if group cannot be ungrouped
        GOL_DRAW        :1  ;True if object cannot be drawn
        GOL_PRINT       :1  ;True if object cannot be printed
    GrObjLocks      end

**Library:** grobj.def

----------
#### GrObjMessageOptimizationFlags
    GrObjMessageOptimizationFlags               record
        GOMOF_GET_DWF_SELECTION_HANDLE_BOUNDS_FOR_TRIVIAL_REJECT :1
        GOMOF_SPECIAL_RESIZE_CONSTRAIN                          :1
        GOMOF_INVALIDATE_LINE                                   :1
        GOMOF_INVALIDATE_AREA                                   :1
        GOMOF_INVALIDATE                                        :1
        GOMOF_NOTIFY_ACTION                                     :1
        GOMOF_SEND_UI_NOTIFICATION                              :1
        GOMOF_DRAW_FG_AREA                                      :1
        GOMOF_DRAW_FG_LINE                                      :1
        GOMOF_DRAW_BG                                           :1
    GrObjMessageOptimizationFlags               end

Each bit below corresponds to an often used, but seldom subclassed message. 
Instead of sending the message to itself, objects will call a utility routine 
(such as **GrObjOptInvalidate**). The routine will check the objects GOMOF 
flags and if the corresponding bit is set it will send the message to itself. 
Otherwise the routine will perform the default functionality.

These bits should only be set in a MSG_META_INITIALIZE handler as they are 
class - and not object-specific. They are not copied to the clipboard.

GOMOF_GET_DWF_SELECTION_HANDLE_BOUNDS_FOR_TRIVIAL_REJECT  
If set, sends 
MSG_GO_GET_DWF_SELECTION_HANDLE_BOUNDS_FOR_TRIVIAL_REJECT.

GOMOF_SPECIAL_RESIZE_CONSTRAIN  
If set, sends MSG_GO_SPECIAL_RESIZE_CONSTRAIN.

GOMOF_INVALIDATE_LINE  
If set, sends MSG_GO_INVALIDATE_LINE.

GOMOF_INVALIDATE_AREA  
If set, sends MSG_GO_INVALIDATE_AREA.

GOMOF_INVALIDATE  
If set, sends MSG_GO_INVALIDATE.

GOMOF_NOTIFY_ACTION  
If set, sends MSG_GO_NOTIFY_ACTION.

GOMOF_SEND_UI_NOTIFICATION  
If set, sends MSG_GO_SEND_UI_NOTIFICATION.

The following bits correspond to several drawing messages that are almost 
always sent during the handling of MSG_GO_DRAW. Under certain conditions 
these message are not sent. If the line or area mask is 0 then the line or area 
drawing messages are not sent. If the area mask is solid, then the 
background message is not sent. If you wish to force one of these messages to 
be sent anyway then set its corresponding bit. For example, the text object 
sets the GOMOF_DRAW_BG bit because the foreground is text which doesn't 
completely cover the background rectangle, so the background should always 
be drawn. 

Note: if the background is transparent then the background message will 
never be sent, regardless of the presence of the GOMOF_DRAW_BG bit.

GOMOF_DRAW_FG_AREA  
If set, sends MSG_GO_DRAW_FG_AREA(_HI_RES).

GOMOF_DRAW_FG_LINE  
If set, sends MSG_GO_DRAW_FG_LINE(_HI_RES).

GOMOF_DRAW_BG  
If set, sends MSG_GO_DRAW_BG.

**Library:** grobj.def

----------
#### GrObjMouseData
    GrObjMouseData      struct
        GOMD_point          PointDWFixed        ; This field must be first.
        GOMD_buttonInfo     ButtonInfo          ; Copy of ButtonInfo
        GOMD_uiFA           UIFunctionsActive   ; Copy of UIFunctionsActive
        GOMD_goFA           GrObjFunctionsActive
        GOMD_gstate         hptr.GState
    GrObjMouseData      ends
    
**Library:** grobj.def

----------
#### GrObjMouseReturnType
    GrObjMouseReturnType            etype byte
        GOMRF_HANDLE    enum GrObjMouseReturnType   ;Mouse position is over a handle 
                                                    ;of a selected object.
        GOMRF_BOUNDS    enum GrObjMouseReturnType   ;Mouse position is over the
                                                    ;bounds of a grobject
        GOMRF_NOTHING   enum GrObjMouseReturnType   ;Mouse position isn't over
                                                    ;anything interesting.

These types are defined in the order in which they will be checked. As soon 
as one of the conditions is met the message returns without checking the 
remaining types.

**Library:** grobj.def

----------
#### GrObjNotifyAreaAttrChange
    GrObjNotifyAreaAttrChange           struct
        GNAAC_areaAttr              GrObjBaseAreaAttrElement
        GNAAC_areaAttrDiffs         GrObjBaseAreaAttrDiffs
    GrObjNotifyAreaAttrChange           ends

**Library:** grobj.def

----------
#### GrObjNotifyCurrentTool
    GrObjNotifyCurrentTool          struct
        GONCT_toolClass         fptr.ClassStruct
        GONCT_specInitData      word
    GrObjNotifyCurrentTool          ends

**Library:** grobj.def

----------
#### GrObjNotifyGradientAttrChange
    GrObjNotifyGradientAttrChange                   struct
        GONGAC_type             GrObjGradientType
        GONGAC_endR             byte                ;ending color red byte
        GONGAC_endG             byte                ;ending color green byte
        GONGAC_endB             byte                ;ending color blue byte
        GONGAC_numIntervals     word
        GONGAC_diffs            GrObjGradientAttrDiffs
        align                   word
    GrObjNotifyGradientAttrChange                   ends

**Library:** grobj.def

----------
#### GrObjNotifyLineAttrChange
    GrObjNotifyLineAttrChange           struct
        GNLAC_lineAttr              GrObjBaseLineAttrElement
        GNLAC_lineAttrDiffs         GrObjBaseLineAttrDiffs
    GrObjNotifyLineAttrChange           ends

**Library:** grobj.def

----------
#### GrObjNotifySelectionStateChange
    GrObjNotifySelectionStateChange             struct
        GONSSC_selectionState           GrObjSelectionState
        GONSSC_selectionStateDiffs      GrObjSelectionStateDiffs
        GONSSC_grObjFlagsDiffs          GrObjAttrFlags
        GONSSC_locksDiffs               GrObjLocks
        GONSSC_arcCloseType             ArcCloseType
        GONSSC_arcStartAngle            WWFixed
        GONSSC_arcEndAngle              WWFixed
    GrObjNotifySelectionStateChange             ends

**Library:** grobj.def

----------
#### GrObjNudgeControlFeatures
    GrObjNudgeControlFeatures               record
        GONCF_NUDGE_LEFT                :1
        GONCF_NUDGE_RIGHT               :1
        GONCF_NUDGE_UP                  :1
        GONCF_NUDGE_DOWN                :1
        GONCF_CUSTOM_MOVE               :1
    GrObjNudgeControlFeatures               end

**Library:** grobj.def

----------
#### GrObjObjManipData
    GrObjObjManipData           struct
        GOOMD_actionGrObj               optr
        GOOMD_origMousePt               PointDWFixed
        GOOMD_oppositeHandle            GrObjHandleSpecification
        GOOMD_grabbedHandle             GrObjHandleSpecification
        GOOMD_initialAngle              WWFixed
        GOOMD_oppositeAnchor            PointDWFixed
        GOOMD_oppositeInitialAngle      WWFixed
    GrObjObjManipData           ends

**Library:** grobj.def

----------
#### GrObjObscureAttrControlFeatures
    GrObjObscureAttrControlFeatures             record
        GOOACF_INSTRUCTIONS                 :1
        GOOACF_INSERT_OR_DELETE_MOVE        :1
        GOOACF_INSERT_OR_DELETE_RESIZE      :1
        GOOACF_INSERT_OR_DELETE_DELETE      :1
        GOOACF_DONT_WRAP                    :1
        GOOACF_WRAP_INSIDE                  :1
        GOOACF_WRAP_AROUND_RECT             :1
        GOOACF_WRAP_TIGHTLY                 :1
    GrObjObscureAttrControlFeatures             end

**Library:** grobj.def

----------
#### GrObjOptimizationFlags
    GrObjOptimizationFlags          record
        GOOF_ADDED_TO_BODY                      :1
        GOOF_IN_GROUP                           :1
        GOOF_GROBJ_INVALID                      :1
        GOOF_ATTRIBUTE_MANAGER                  :1
        GOOF_FLOATER                            :1
        GOOF_HAS_ACTION_NOTIFICATION            :1
        GOOF_HAS_UNBALANCED_PARENT_DIMENSIONS   :1
                                                :1
    GrObjOptimizationFlags          end

GOOF_ADDED_TO_BODY  
If set, the Grobj has been added to a body, or the group it is in 
has been added to a body.

GOOF_IN_GROUP  
If set, the GrObj is within a group.

GOOF_GROBJ_INVALID  
If set, the object is incomplete and cannot be drawn, or it is 
invalidated. It may be missing its normal transform or have no 
attributes, etc.

GOOF_ATTRIBUTE_MANAGER  
If set, the object is an attribute manager.

GOOF_FLOATER  
If set, GrObj is a floater. If this flag is set, we don't need to dirty 
the object because floater objects are not actually in the 
document.

GOOF_HAS_ACTION_NOTIFICATION  
If set, object has an action notification OD in it's vardata.

GOOF_HAS_UNBALANCED_PARENT_DIMENSIONS  
If set, the object contains the vardata entry 
ATTR_GO_PARENT_DIMENSIONS_OFFSET. This ATTR holds 
the offset from the object's center to the center of the parent 
dimensions.

**Library:** grobj.def

----------
#### GrObjPointerImageSituation
    GrObjPointerImageSituation          etype byte
        GOPIS_NORMAL            enum GrObjPointerImageSituation
        GOPIS_EDIT              enum GrObjPointerImageSituation
        GOPIS_CREATE            enum GrObjPointerImageSituation
        GOPIS_MOVE              enum GrObjPointerImageSituation
        GOPIS_RESIZE_ROTATE     enum GrObjPointerImageSituation

**Library:** grobj.def

----------
#### GrObjResizeMouseData
    GrObjResizeMouseData            struct
        GORSMD_point        PointDWFixed            ; Must be first.
        GORSMD_anchor       GrObjHandleSpecification
        GORSMD_grabbed      GrObjHandleSpecification
        GORSMD_goFA         GrObjFunctionsActive
        GORSMD_gstate       hptr.GState
        align               word
    GrObjResizeMouseData            ends

**Library:** grobj.def

----------
#### GrObjRotateControlFeatures
    GrObjRotateControlFeatures              record
        GORCF_45_DEGREES_CW             :1
        GORCF_90_DEGREES_CW             :1
        GORCF_135_DEGREES_CW            :1
        GORCF_180_DEGREES               :1
        GORCF_135_DEGREES_CCW           :1
        GORCF_90_DEGREES_CCW            :1
        GORCF_45_DEGREES_CCW            :1
        GORCF_CUSTOM_ROTATION           :1
    GrObjRotateControlFeatures              end

**Library:** grobj.def

----------
#### GrObjRotateMouseData
    GrObjRotateMouseData            struct
        GORMD_degrees           WWFixed
        GORMD_anchor            GrObjHandleSpecification
        GORMD_goFA              GrObjFunctionsActive
        GORMD_gstate            hptr.GState
        align                   word
    GrObjRotateMouseData            ends

This structure is the stack frame passed with rotate message.

**Library:** grobj.def

----------
#### GrObjScaleControlFeatures
    GrObjScaleControlFeatures               record
        GOSCF_HALF_WIDTH                :1
        GOSCF_HALF_HEIGHT               :1
        GOSCF_DOUBLE_WIDTH              :1
        GOSCF_DOUBLE_HEIGHT             :1
        GOSCF_CUSTOM_SCALE              :1
    GrObjScaleControlFeatures               end

**Library:** grobj.def

----------
#### GrObjScaleData
    GrObjScaleData      struct
        GOSD_xScale         WWFixed
        GOSD_yScale         WWFixed
    GrObjScaleData      ends

**Library:** grobj.def

----------
#### GrObjSelectionState
    GrObjSelectionState     struct
        GSS_numSelected         word
        GSS_classSelected       fptr.ClassStruct
        GSS_flags               GrObjSelectionStateFlags
        GSS_grObjFlags          GrObjAttrFlags
        GSS_locks               GrObjLocks
        align                   word
    GrObjSelectionState     ends

**Library:** grobj.def

----------
#### GrObjSelectionStateDiffs
    GrObjSelectionStateDiffs            record
        GSSD_MULTIPLE_CLASSES               :1
        GSSD_MULTIPLE_ARC_CLOSE_TYPES       :1
        GSSD_MULTIPLE_ARC_START_ANGLES      :1
        GSSD_MULTIPLE_ARC_END_ANGLES        :1
                                            :4
    GrObjSelectionStateDiffs            end

**Library:** grobj.def

----------
#### GrObjSelectionStateFlags
    GrObjSelectionStateFlags            record
        GSSF_EDITING            :1  ;True if an object is being edited.
        GSSF_UNGROUPABLE        :1  ;True if at least one of the objects
                                    ;selected can be ungrouped.
        GSSF_TEXT_SELECTED      :1  ;True if at least one of the objects
                                    ;selected is some sort of text object
        GSSF_BITMAP_SELECTED    :1  ;True if at least one of the objects
                                    ;selected is some sort of bitmap object
        GSSF_SPLINE_SELECTED    :1  ;True if at least one of the objects
                                    ;selected is some sort of spline object
        GSSF_ARC_SELECTED       :1  ;True if at least one of the objects
                                    ;selected is some sort of arc object
    GrObjSelectionStateFlags            end

**Library:** grobj.def

----------
#### GrObjsInRectData
    GrObjsInRectData        struct
        GOIRD_tempMessage           word
        GOIRD_tempMessageDX         word
        GOIRD_inRectMessage         word
        GOIRD_inRectMessageDX       word
        GOIRD_rect                  RectDWord
        GOIRD_special               GrObjsInRectSpecial
        align                       word
    GrObjsInRectData        ends

*GOIRD_tempMessage* stores the message to send to an object that has its 
GOTM_TEMP_HANDLES bit set.

*GOIRD_tempMessageDX* stores the word of data that can be passed with the 
above temporary message in **dx**.

*GOIRD_inRectMessage* stores the message to send to an object if it is found to 
reside within the **Rectangle** specified by *GOIRD_rect*.

*GOIRD_inRectMessageDX* stores the word of data that can be passed with the 
above message in **dx**.

*GOIRD_rect* stores the **Rectangle** that we are checking whether the object 
resides within.

*GOIRD_special* stores special instructions for processing children.

**Library:** grobj.def

----------
#### GrObjsInRectSpecial
    GrObjsInRectSpecial         record
        GOIRS_IGNORE_TEMP           :1
        GOIRS_IGNORE_RECT           :1
        GOIRS_XOR_CHECK             :1
    GrObjsInRectSpecial         end

GOIRS_IGNORE_TEMP  
If set, do not send the GrObj's Temp Message to objects with 
GOTM_TEMP_HANDLES set.

GOIRS_IGNORE_RECT  
If set, do not send the GrObj's InRect Message to objects within 
the rectangle.

GOIRS_XOR_CHECK  
If set and both the Temp and InRect conditions apply, then send 
neither message. Otherwise send both messages. (The Temp 
Message will always be sent first.)

**Library:** grobj.def

----------
#### GrObjSkewControlFeatures
    GrObjSkewControlFeatures        record
        GOSCF_LEFT              :1
        GOSCF_RIGHT             :1
        GOSCF_UP                :1
        GOSCF_DOWN              :1
        GOSCF_CUSTOM_SKEW       :1
    GrObjSkewControlFeatures        end

**Library:** grobj.def

----------
#### GrObjSkewData
    GrObjSkewData       struct
        GOSD_xDegrees           WWFixed
        GOSD_yDegress           WWFixed
    GrObjSkewData       ends

**Library:** grobj.def

----------
#### GrObjStyleElement
    GrObjStyleElement       struct
        GSE_meta                NameArrayElement
        GSE_baseStyle           word
        GSE_flags               StyleElementFlags
        GSE_reserved            byte 6 dup (?)
        GSE_privateData         GrObjStylePrivateData
        GSE_areaAttrToken       word
        GSE_lineAttrToken       word
        GSE_name                label char
    GrObjStyleElement       ends

**Library:** grobj.def

----------
#### GrObjStyleFlags
    GrObjStyleFlags             record
        GSF_AREA_COLOR_RELATIVE         :1
        GSF_AREA_MASK_RELATIVE          :1
        GSF_LINE_COLOR_RELATIVE         :1
        GSF_LINE_MASK_RELATIVE          :1
        GSF_LINE_WIDTH_RELATIVE         :1
                                        :11
    GrObjStyleFlags             end

**Library:** grobj.def

----------
#### GrObjStylePrivateData
    GrObjStylePrivateData           struct
        GSPD_flags          GrObjStyleFlags
        GSPD_unused         byte 2 dup (0)
    GrObjStylePrivateData           ends

**Library:** grobj.def

----------
#### GrObjTempModes
    GrObjTempModes      record
        GOTM_SELECTED               :1
        GOTM_EDITED                 :1
        GOTM_EDIT_INDICATOR_DRAWN   :1
        GOTM_HANDLES_DRAWN          :1
        GOTM_TEMP_HANDLES           :1
        GOTM_SPRITE_DRAWN           :1
        GOTM_SPRITE_DRAWN_HI_RES    :1
        GOTM_SYS_TARGET             :1
    GrObjTempModes      end

GOTM_SELECTED  
If set, the GrObj is in the selection list.

GOTM_EDITED  
If set, the GrObj is currently being edited. This is equivalent to 
the GrObj having the application target.

GOTM_EDIT_INDICATOR_DRAWN  
If set, the object has drawn some indicator to show the user 
that it is being edited.

GOTM_HANDLES_DRAWN  
If set, the GrObj's selection handles have been drawn.

GOTM_TEMP_HANDLES  
If set, the GrObj's handles are drawn in a temporary state. This 
flag is set by MSG_GO_DRAW_HANDLES_FORCE, 
MSG_GO_DRAW_HANDLES_OPPOSITE and cleared by 
MSG_GO_DRAW_HANDLES_MATCH, 
MSG_GO_DRAW_HANDLES and MSG_GO_UNDRAW_HANDLES. 
This functionality is mainly used when drag selecting to cut 
down the number of objects that methods must be sent to.

GOTM_SPRITE_DRAWN  
If set, the object's sprite has been drawn.

GOTM_SPRITE_DRAWN_HI_RES  
If set , the object's sprite was drawn at a higher resolution. This 
bit is meaningless if GOTM_SPRITE_DRAWN is not set.

GOTM_SYS_TARGET  
If set, the GrObjBody has the system target. This bit is 
meaningless unless GOTM_SELECTED or GOTM_EDITED is set. 
(The GrObjBody only updates objects in selection list and with 
the application target.)

**Library:** grobj.def

----------
#### GrObjTextArrays
    GrObjTextArrays         struct
        GOTA_charAttrArray          word
        GOTA_paraAttrArray          word
        GOTA_typeArray              word
        GOTA_graphicArray           word
        GOTA_nameArray              word
        GOTA_textStyleArray         word
    GrObjTextArrays         ends

**Library:** grobj.def

----------
#### GrObjTiledDataFlags
    GrObjTiledDataFlags     record
                                :7
        GOTDF_VAR_DATA          :1
    GrObjTiledDataFlags     end

**Library:** grobj.def

----------
#### GrObjTransferBlockHeader
    GrObjTransferBlockHeader                struct
        GOTBH_meta              VMChainTree
        GOTBH_size              PointDWord  ; Width and height of the cut.
        GOTBH_firstLMem         label word
        GOTBH_areaAttrArray     dword
        GOTBH_lineAttrArray     dword
        GOTBH_styleArray        dword
        GOTBH_charAttrRuns      dword
        GOTBH_paraAttrRuns      dword
        GOTBH_textStyleArray    dword
        GOTBH_lastLMem          label word
        GOTBH_textGraphicsTree  dword
    GrObjTransferBlockHeader                ends

This structure heads a "transfer" item, which is used by the Clipboard in 
Cut/Copy/Paste operations.

**Library:** grobj.def

----------
#### GrObjTransferDataDirectory
    GrObjTransferDataDirectory              struct
        GOTDD_tiledDataFlags        GrObjTiledDataFlags
        GOTDD_protocol              byte
    GrObjTransferDataDirectory              ends

**Library:** grobj.def

----------
#### GrObjTransferParams
    GrObjTransferParams         struct
        GTP_ssp                         StyleSheetParams
        GTP_textSSP                     VisTextSaveStyleSheetParams
        GTP_selectionCenterDOCUMENT     PointDWFixed
        GTP_optBlock                    hptr
        GTP_treeBlock                   hptr
        GTP_curSlot                     word
        GTP_id                          dword
        GTP_curSize                     word
        GTP_curPos                      word
    GrObjTransferParams         ends

**Library:** grobj.def

----------
#### GrObjTransMatrix
    GrObjTransMatrix        struct
        GTM_e11         WWFixed
        GTM_e12         WWFixed
        GTM_e21         WWFixed
        GTM_e22         WWFixed
    GrObjTransMatrix        ends

**Library:** grobj.def

----------
#### GrObjUINotificationTypes
    GrObjUINotificationTypes            record
        GOUINT_STYLE        :1  ;True if style notification needs to be 
                                ;sent
        GOUINT_AREA         :1  ;True if area notification needs to be sent
        GOUINT_LINE         :1  ;True if line notification needs to be sent
        GOUINT_GROBJ_SELECT :1  ;True if grobj specific selection state
                                ;notification needs to be sent
        GOUINT_STYLE_SHEET  :1  ;True if style notification needs to be 
                                ;sent
        GOUINT_SELECT       :1  ;True if edit menu notification need be 
                                ;sent
                            :10 ;unused
    GrObjUINotificationTypes            end

**Library:** grobj.def

----------
#### GrObjUndoAppType
    GrObjUndoAppType        struct
        GOUAT_freeMessage           word
        GOUAT_undoMessage           word
    GrObjUndoAppType        ends

**Library:** grobj.def

----------
#### GrObjVisGuardianCreateMode
    GrObjVisGuardianCreateMode              etype byte, 0
        GOVGCM_NO_CREATE            enum GrObjVisGuardianCreateMode
        GOVGCM_GUARDIAN_CREATE      enum GrObjVisGuardianCreateMode
        GOVGCM_VIS_WARD_CREATE      enum GrObjVisGuardianCreateMode

GOVGCM_NO_CREATE  
This type indicates that new objects cannot be created with the 
guardian.

GOVGCM_GUARDIAN_CREATE  
This type indicates that creating a new object is handled by the 
guardian and consists of dragging open a rectangular area.

GOVGCM_VIS_WARD_CREATE  
This type indicates that creating a new object is handled by the 
ward and mouse events during the create operation should be 
sent to the ward. 

**Library:** grobj.def

----------
#### GrObjVisGuardianFlags
    GrObjVisGuardianFlags           record
        GOVGF_VIS_BOUNDS_HAVE_CHANGED       :1
        GOVGF_LARGE                         :1
        GOVGF_UNUSED                        :1
        GOVGF_ALSO_UNUSED                   :1
        GOVGF_APPLY_OBJECT_TO_VIS_TRANSFORM :1
        GOVGF_CAN_EDIT_EXISTING_OBJECTS     :1
        GOVGF_CREATE_MODE                   GrObjGuardianCreateMode:2
    GrObjVisGuardianFlags           end

GOVGF_VIS_BOUNDS_HAVE_CHANGED  
If set, the visible bounds of the ward have changed since the 
last time the bit was cleared. Each guardian uses this bit 
differently to help it determine when to send out 
GOANT_RESIZE action notifications.

GOVGF_LARGE  
If set, send large mouse events to the object. If clear, send small 
mouse events instead.

GOVGF_APPLY_OBJECT_TO_VIS_TRANSFORM  
If set, you must send 
MSG_GOVG_APPLY_OBJECT_TO_VIS_TRANSFORM to the 
object; otherwise a utility routine can be used.

GOVGF_CAN_EDIT_EXISTING_OBJECTS  
If set, the floater can edit existing objects in the document.

**Library:** grobj.def

----------
#### GrObjWrapTextType
    GrObjWrapTextType       etype byte, 0
        GOWTT_DONT_WRAP             enum GrObjWrapTextType
        GOWTT_WRAP_AROUND_RECT      enum GrObjWrapTextType
        GOWTT_WRAP_AROUND_TIGHTLY   enum GrObjWrapTextType
        GOWTT_WRAP_INSIDE           enum GrObjWrapTextType

**Library:** grobj.def

----------
#### GroupAddGrObjFlags
    GroupAddGrObjFlags      record
        GAGOF_RELATIVE          :1
        GAGOF_REFERENCE         :15
    GroupAddGrObjFlags      end

GAGOF_RELATIVE  
This flag indicates that the position of the center of the object is already 
relative to the center of the group. Otherwise, the object center is absolute 
and must be adjusted when it is added.

**Library:** grobj.def

----------
#### GroupUnsuspendOps
    GroupUnsuspendOps       record
        GUO_EXPAND      :1
    GroupUnsuspendOps       end

GUO_EXPAND  
If set, the group should send MSG_GROUP_EXPAND to itself when its suspend 
count reaches zero.

**Library:** grobj.def

----------
#### GSControl
    GSControl       record
                        :6,
        GSC_PARTIAL     :1  ; Just do one element. If element is a complex
                            ; bit map, do just one piece. (This flag works 
                            ; only with GrCopyGString).)
        GSC_ONE         :1, ; just do one element
        GSC_MISC        :1, ; return on MISC opcode
        GSC_LABEL       :1, ; return on GR_LABEL opcode
        GSC_ESCAPE      :1, ; return on GR_ESCAPE opcode
        GSC_NEW_PAG     :1, ; return when we get to a NEW_PAGE
        GSC_XFORM       :1, ; return on TRANSFORMATIONopcode
        GSC_OUTPUT      :1, ; return on OUTPUT opcode
        GSC_ATTR        :1, ; return on ATTRIBUTE opcode
        GSC_PATH        :1  ; return on PATH opcode
    GSControl       end

**Library:** gstring.def

----------
#### GSElemInfo
    GSElemInfo      struct
        GSEI_size           word
        GSEI_play           nptr
        GSEI_kern           fptr
    GSElemInfo      ends

**Library:** gstring.def

----------
#### GSRefCountAndFlags
    GSRefCountAndFlags          record
        GSRCAF_USE_DOC_CLIP_REGION  :1  ;If set, then use GrSetDocClipRect
                                        ;instead of GrSetClipRect
        GSRCAF_REF_COUNT            :7
    GSRefCountAndFlags          end

**Library:** Objects/vTextC.def

----------
#### GSRetType
    GSRetType       etype word
        GSRT_COMPLETE   enum GSRetType
        GSRT_ONE        enum GSRetType
        GSRT_MISC       enum GSRetType
        GSRT_LABEL      enum GSRetType
        GSRT_ESCAPE     enum GSRetType
        GSRT_NEW_PAGE   enum GSRetType
        GSRT_XFORM      enum GSRetType
        GSRT_OUTPUT     enum GSRetType
        GSRT_ATTR       enum GSRetType
        GSRT_PATH       enum GSRetType
        GSRT_FAULT      enum GSRetType, 0xffff

**Library:** gstring.def

----------
#### GStringElement
    GStringElement      etype byte, 0, 1

    DefGSElement    macro   \
            rootName, gseEnum, playRout, varType, varOff, altKern
    gseEnum enum GStringElement

        DefGSElement EndGString, GR_END_GSTRING, PENull
        DefGSElement Comment,   GR_COMMENT,     PEComment, bytes, OC_size
        DefGSElement NullOp,    GR_NULL_OP,     PENoArgs
        DefGSElement SetGStringBounds, GR_SET_GSTRING_BOUNDS, PETwoCoords
        DefGSElement    ,       GR_MISC_4,      PENull
        DefGSElement    ,       GR_MISC_5,      PENull
        DefGSElement    ,       GR_MISC_6,      PENull
        DefGSElement    ,       GR_MISC_7,      PENull
        DefGSElement    ,       GR_MISC_8,      PENull
        DefGSElement    ,       GR_MISC_9,      PENull
        DefGSElement    ,       GR_MISC_A,      PENull
        DefGSElement    ,       GR_MISC_B,      PENull
        DefGSElement    ,       GR_MISC_C,      PENull
        DefGSElement Label,     GR_LABEL,       PEWordAttr
        DefGSElement Escape,    GR_ESCAPE,      PEComment, bytes, OE_escSize
        DefGSElement NewPage,   GR_NEW_PAGE,    PEByteAttr
        DefGSElement ApplyRotation, GR_APPLY_ROTATION, PERotate
        DefGSElement ApplyScale, GR_APPLY_SCALE,        PETransScale
        DefGSElement ApplyTranslation, GR_APPLY_TRANSLATION, PETransScale
        DefGSElement ApplyTransform, GR_APPLY_TRANSFORM, PETMatrix
        DefGSElement ApplyTranslationDWord, GR_APPLY_TRANSLATION_DWORD, PETransScale
        DefGSElement SetTransform, GR_SET_TRANSFORM,    PETMatrix
        DefGSElement SetNullTransform, GR_SET_NULL_TRANSFORM, PENoArgs
        DefGSElement SetDefaultTransform, GR_SET_DEFAULT_TRANSFORM, PENoArgs
        DefGSElement InitDefaultTransform, GR_INIT_DEFAULT_TRANSFORM, PENoArgs
        DefGSElement SaveTransform, GR_SAVE_TRANSFORM, PENoArgs
        DefGSElement RestoreTransform, GR_RESTORE_TRANSFORM, PENoArgs
        DefGSElement    ,       GR_XFORM_1B, PENull
        DefGSElement    ,       GR_XFORM_1C, PENull
        DefGSElement    ,       GR_XFORM_1D, PENull
        DefGSElement    ,       GR_XFORM_1E, PENull
        DefGSElement    ,       GR_XFORM_1F, PENull
        DefGSElement DrawLine, GR_DRAW_LINE,    PETwoCoords
        DefGSElement DrawLineTo, GR_DRAW_LINE_TO, PEOneCoord
        DefGSElement DrawRelLineTo, GR_DRAW_REL_LINE_TO, PERelCoord
        DefGSElement DrawHLine, GR_DRAW_HLINE, PEDrawHalfLine
        DefGSElement DrawHLineTo, GR_DRAW_HLINE_TO, PEDrawHalfLine
        DefGSElement DrawVLine, GR_DRAW_VLINE, PEDrawHalfLine
        DefGSElement DrawVLineTo, GR_DRAW_VLINE_TO, PEDrawHalfLine
        DefGSElement DrawPolyline, GR_DRAW_POLYLINE, PEPolyCoord, coords, ODPL_count
        DefGSElement DrawArc,   GR_DRAW_ARC, PEDrawArcs
        DefGSElement DrawArc3Point, GR_DRAW_ARC_3POINT, PEDrawArcs
        DefGSElement DrawArc3PointTo, GR_DRAW_ARC_3POINT_TO, PEDrawArcs
        DefGSElement DrawRelArc3PointTo, GR_DRAW_REL_ARC_3POINT_TO, PEDrawArcs
        DefGSElement DrawRect, GR_DRAW_RECT, PETwoCoords
        DefGSElement DrawRectTo, GR_DRAW_RECT_TO, PEOneCoord
        DefGSElement DrawRoundRect, GR_DRAW_ROUND_RECT, PEDrawRoundRects
        DefGSElement DrawRoundRectTo, GR_DRAW_ROUND_RECT_TO, PEDrawRoundRects
        DefGSElement DrawSpline, GR_DRAW_SPLINE, PEPolyCoord, coords, ODS_count
        DefGSElement DrawSplineTo, GR_DRAW_SPLINE_TO, PEPolyCoord, coords, ODST_count
        DefGSElement DrawCurve, GR_DRAW_CURVE, PECurve
        DefGSElement DrawCurveTo, GR_DRAW_CURVE_TO, PECurve
        DefGSElement DrawRelCurveTo, GR_DRAW_REL_CURVE_TO, PECurve
        DefGSElement DrawEllipse, GR_DRAW_ELLIPSE, PETwoCoords
        DefGSElement DrawPolygon, GR_DRAW_POLYGON, PEPolyCoord, coords, ODPG_count
        DefGSElement DrawPoint, GR_DRAW_POINT, PEOneCoord
        DefGSElement DrawPointAtCP, GR_DRAW_POINT_CP, PENoArgs
        DefGSElement BrushPolyline, GR_BRUSH_POLYLINE, PEPolyCoord, coords, OBPL_count
        DefGSElement DrawChar, GR_DRAW_CHAR, PEDrawChar
        DefGSElement DrawCharAtCP, GR_DRAW_CHAR_CP, PEDrawChar
        DefGSElement DrawText, GR_DRAW_TEXT, PEDrawText, bytes, ODT_len
        DefGSElement DrawTextAtCP, GR_DRAW_TEXT_CP, PEDrawText, bytes, ODTCP_len
        DefGSElement DrawTextField, GR_DRAW_TEXT_FIELD, PETextField
        DefGSElement DrawTextPtr,GR_DRAW_TEXT_PTR, PETextPtr,,,GrDrawText

        DefGSElement DrawTextOptr,GR_DRAW_TEXT_OPTR, PETextOptr,,,GrDrawText
        DefGSElement DrawPath, GR_DRAW_PATH, PENoArgs
        DefGSElement FillRect, GR_FILL_RECT, PETwoCoords
        DefGSElement FillRectTo, GR_FILL_RECT_TO, PEOneCoord
        DefGSElement FillRoundRect, GR_FILL_ROUND_RECT, PEDrawRoundRects
        DefGSElement FillRoundRectTo, GR_FILL_ROUND_RECT_TO, PEDrawRoundRects
        DefGSElement FillArc, GR_FILL_ARC, PEDrawArcs
        DefGSElement FillPolygon, GR_FILL_POLYGON, PEPolyCoord, coords, OFP_count
        DefGSElement FillEllipse, GR_FILL_ELLIPSE, PETwoCoords
        DefGSElement FillPath, GR_FILL_PATH, PEByteAttr
        DefGSElement FillArc3Point, GR_FILL_ARC_3POINT, PEDrawArcs
        DefGSElement FillArc3PointTo, GR_FILL_ARC_3POINT_TO, PEDrawArcs
        DefGSElement FillBitmap, GR_FILL_BITMAP, PEBitmap, bytes, OFB_size
        DefGSElement FillBitmapAtCP, GR_FILL_BITMAP_CP, PEBitmap, bytes, OFBCP_size
        DefGSElement FillBitmapOptr, GR_FILL_BITMAP_OPTR, PEBitmapOptr,,,GrFillBitmap
        DefGSElement FillBitmapPtr, GR_FILL_BITMAP_PTR, PEBitmapPtr,,,GrFillBitmap
        DefGSElement DrawBitmap, GR_DRAW_BITMAP, PEBitmap, bytes, ODB_size
        DefGSElement DrawBitmapAtCP, GR_DRAW_BITMAP_CP, PEBitmap, bytes, ODBCP_size
        DefGSElement DrawBitmapOptr, GR_DRAW_BITMAP_OPTR, PEBitmapOptr,,,GrDrawBitmap
        DefGSElement DrawBitmapPtr, GR_DRAW_BITMAP_PTR, PEBitmapPtr,,,GrDrawBitmap
        DefGSElement BitmapSlice, GSE_BITMAP_SLICE, PESlice,bytes,OBS_size,GrDrawBitmap
        DefGSElement    ,       GR_OUTPUT_55,   PENull
        DefGSElement    ,       GR_OUTPUT_56,   PENull
        DefGSElement    ,       GR_OUTPUT_57,   PENull
        DefGSElement    ,       GR_OUTPUT_58,   PENull
        DefGSElement    ,       GR_OUTPUT_59,   PENull
        DefGSElement    ,       GR_OUTPUT_5A,   PENull
        DefGSElement    ,       GR_OUTPUT_5B,   PENull
        DefGSElement    ,       GR_OUTPUT_5C,   PENull
        DefGSElement    ,       GR_OUTPUT_5D,   PENull
        DefGSElement    ,       GR_OUTPUT_5E,   PENull
        DefGSElement    ,       GR_OUTPUT_5F,   PENull
        DefGSElement SaveState, GR_SAVE_STATE, PENoArgs
        DefGSElement RestoreState, GR_RESTORE_STATE, PENoArgs
        DefGSElement SetMixMode, GR_SET_MIX_MODE, PEByteAttr
        DefGSElement MoveTo, GR_MOVE_TO, PEOneCoord
        DefGSElement RelMoveTo, GR_REL_MOVE_TO, PERelCoord
        DefGSElement CreatePalette, GR_CREATE_PALETTE, PENoArgs
        DefGSElement DestroyPalette, GR_DESTROY_PALETTE, PENoArgs
        DefGSElement SetPaletteEntry, GR_SET_PALETTE_ENTRY, PEOneCoord
        DefGSElement SetPalette, GR_SET_PALETTE, PEPalette, bytes, OSP_num
        DefGSElement SetLineColor, GR_SET_LINE_COLOR, PE3ByteAttr
        DefGSElement SetLineMask, GR_SET_LINE_MASK, PEByteAttr
        DefGSElement SetLineColorMap, GR_SET_LINE_COLOR_MAP, PEByteAttr
        DefGSElement SetLineWidth, GR_SET_LINE_WIDTH, PEOneCoord
        DefGSElement SetLineJoin, GR_SET_LINE_JOIN, PEByteAttr
        DefGSElement SetLineEnd, GR_SET_LINE_END, PEByteAttr
        DefGSElement SetLineAttr, GR_SET_LINE_ATTR, PEAttr
        DefGSElement SetMiterLimit, GR_SET_MITER_LIMIT, PEOneCoord
        DefGSElement SetLineStyle, GR_SET_LINE_STYLE, PELineStyle
        DefGSElement SetLineColorIndex,GR_SET_LINE_COLOR_INDEX,PEByteAttr,,,GrSetLineColor
        DefGSElement SetCustomLineMask,GR_SET_CUSTOM_LINE_MASK,PECustomMask,,,GrSetLineMask
        DefGSElement SetCustomLineStyle,GR_SET_CUSTOM_LINE_STYLE,PECustomStyle,words,OSCLS_count, GrSetLineStyle
        DefGSElement SetAreaColor, GR_SET_AREA_COLOR, PE3ByteAttr
        DefGSElement SetAreaMask, GR_SET_AREA_MASK, PEByteAttr
        DefGSElement SetAreaColorMap, GR_SET_AREA_COLOR_MAP, PEByteAttr
        DefGSElement SetAreaAttr, GR_SET_AREA_ATTR, PEAttr
        DefGSElement SetAreaColorIndex,GR_SET_AREA_COLOR_INDEX, PEByteAttr,,,GrSetAreaColor
        DefGSElement SetCustomAreaMask,GR_SET_CUSTOM_AREA_MASK,PECustomMask,,,GrSetAreaMask
        DefGSElement SetAreaPattern, GR_SET_AREA_PATTERN, PESetPattern
        DefGSElement SetCustomAreaPattern,GR_SET_CUSTOM_AREA_PATTERN,PESetCustPattern,bytes,OSCAP_size, GrSetAreaPattern
        DefGSElement SetTextColor, GR_SET_TEXT_COLOR, PE3ByteAttr
        DefGSElement SetTextMask, GR_SET_TEXT_MASK, PEByteAttr
        DefGSElement SetTextColorMap, GR_SET_TEXT_COLOR_MAP, PEByteAttr
        DefGSElement SetTextStyle, GR_SET_TEXT_STYLE, PEWordAttr
        DefGSElement SetTextMode, GR_SET_TEXT_MODE, PEWordAttr
        DefGSElement SetTextSpacePad, GR_SET_TEXT_SPACE_PAD, PESpacePad
        DefGSElement SetTextAttr, GR_SET_TEXT_ATTR, PEAttr
        DefGSElement SetFont, GR_SET_FONT, PESetFont
        DefGSElement SetTextColorIndex,GR_SET_TEXT_COLOR_INDEX,PEByteAttr,,,GrSetTextColor
        DefGSElement SetCustomTextMask,GR_SET_CUSTOM_TEXT_MASK,PECustomMask,,,GrSetTextMask
        DefGSElement SetTrackKern, GR_SET_TRACK_KERN, PEWordAttr
        DefGSElement SetFontWeight, GR_SET_FONT_WEIGHT, PEByteAttr
        DefGSElement SetFontWidth, GR_SET_FONT_WIDTH, PEByteAttr
        DefGSElement SetSuperscriptAttr, GR_SET_SUPERSCRIPT_ATTR, PEWordAttr
        DefGSElement SetSubscriptAttr, GR_SET_SUBSCRIPT_ATTR, PEWordAttr
        DefGSElement SetTextPattern, GR_SET_TEXT_PATTERN, PESetPattern
        DefGSElement SetCustomTextPattern,GR_SET_CUSTOM_TEXT_PATTERN,PESetCustPattern,bytes,OSCTP_size, GrSetTextPattern
        DefGSElement    ,       GR_ATTR_8E,     PENull
        DefGSElement    ,       GR_ATTR_8F,     PENull
        DefGSElement    ,       GR_ATTR_90,     PENull
        DefGSElement    ,       GR_ATTR_91,     PENull
        DefGSElement    ,       GR_ATTR_92,     PENull
        DefGSElement    ,       GR_ATTR_93,     PENull
        DefGSElement    ,       GR_ATTR_94,     PENull
        DefGSElement    ,       GR_ATTR_95,     PENull
        DefGSElement    ,       GR_ATTR_96,     PENull
        DefGSElement    ,       GR_ATTR_97,     PENull
        DefGSElement    ,       GR_ATTR_98,     PENull
        DefGSElement    ,       GR_ATTR_99,     PENull
        DefGSElement    ,       GR_ATTR_9A,     PENull
        DefGSElement    ,       GR_ATTR_9B,     PENull
        DefGSElement    ,       GR_ATTR_9C,     PENull
        DefGSElement    ,       GR_ATTR_9D,     PENull
        DefGSElement    ,       GR_ATTR_9E,     PENull
        DefGSElement    ,       GR_ATTR_9F,     PENull
        DefGSElement BeginPath, GR_BEGIN_PATH, PEOneCoord
        DefGSElement EndPath, GR_END_PATH, PENoArgs
        DefGSElement SetClipRect, GR_SET_CLIP_RECT, PEClipRect
        DefGSElement SetWinClipRect, GR_SET_WIN_CLIP_RECT, PEClipRect
        DefGSElement CloseSubPath, GR_CLOSE_SUB_PATH, PENoArgs
        DefGSElement SetClipPath, GR_SET_CLIP_PATH, PEPathArgs
        DefGSElement SetWinClipPath, GR_SET_WIN_CLIP_PATH, PEPathArgs
        DefGSElement SetStrokePath, GR_SET_STROKE_PATH, PENoArgs
        DefGSElement    ,       GR_PATH_A8,     PENull
        DefGSElement    ,       GR_PATH_A9,     PENull
        DefGSElement    ,       GR_PATH_AA,     PENull
        DefGSElement    ,       GR_PATH_AB,     PENull
        DefGSElement    ,       GR_PATH_AC,     PENull
        DefGSElement    ,       GR_PATH_AD,     PENull
        DefGSElement    ,       GR_PATH_AE,     PENull
        DefGSElement    ,       GR_PATH_AF,     PENull

**Library:** gstring.def

----------
#### GStringErrorType
    GStringErrorType        etype word
        GSET_NO_ERROR   enum GStringErrorType   ; there was no error
        GSET_DISK_FULL  enum GStringErrorType   ; disk became full, file truncated

**Library:** gstring.def

----------
#### GStringKillType
    GStringKillType     etype byte
        GSKT_KILL_DATA      enum GStringKillType    ; delete the data too
        GSKT_LEAVE_DATA     enum GStringKillType    ; leave the data alone

**Library:** gstring.def

----------
#### GStringSetPosType
    GStringSetPosType       etype byte
        GSSPT_SKIP_1    enum GStringSetPosType  ; advance 1 element
        GSSPT_RELATIVE  enum GStringSetPosType  ; advance N elements
        GSSPT_BEGINNING enum GStringSetPosType  ; set to start of gstring
        GSSPT_END       enum GStringSetPosType  ; set to end of gstring

**Library:** gstring.def

----------
#### GStringType
    GStringType     etype byte
        GST_CHUNK   enum GStringType    ; write to a memory chunk
        GST_STREAM  enum GStringType    ; write to a stream
        GST_VMEM    enum GStringType    ; write to a vmem block
        GST_PTR     num GStringType     ; static memory (read only)
        GST_PATH    enum GStringType    ; write to a path (&store in an lmem 
                                        ; chunk). INTERNAL ONLY!

**Library:** gstring.def

----------
#### GTCFeatures
    GTCFeatures     record
        GTCF_TOOL_DIALOG        :1
    GTCFeatures     end

**Library:** Objects/gToolCC.def

----------
#### GTP_vars
    GTP_vars        struct
        GTPL_style          TextMetricStyles    ; Must be first
        GTPL_object         dword               ; Object segment address
        GTPL_startPosition  word                ; Passed position into gstring
        GTPL_charCount      word                ; Start offset in string
        align               word
    GTP_vars        ends

This structure is passed to **GrTextPosition**. 

**Library:** text.def

----------
#### Guide
    Guide   struct
        Guide_location      DWFixed
    Guide   ends

**Library:** ruler.def

----------
#### GVCFeatures
    GVCFeatures     record
        GVCF_MAIN_100               :1
        GVCF_MAIN_SCALE_TO_FIT      :1
        GVCF_ZOOM_IN                :1
        GVCF_ZOOM_OUT               :1
        GVCF_REDUCE                 :1
        GVCF_100                    :1
        GVCF_ENLARGE                :1
        GVCF_BIG_ENLARGE            :1
        GVCF_SCALE_TO_FIT           :1
        GVCF_ADJUST_ASPECT_RATIO    :1
        GVCF_APPLY_TO_ALL           :1
        GVCF_SHOW_HORIZONTAL        :1
        GVCF_SHOW_VERTICAL          :1
        GVCF_CUSTOM_SCALE           :1
        GVCF_REDRAW                 :1
    GVCFeatures     end

**Library:** Objects/gViewCC.def

----------
#### GVCToolboxFeatures
    GVCToolboxFeatures      record
        GVCTF_100                   :1
        GVCTF_SCALE_TO_FIT          :1
        GVCTF_ZOOM_IN               :1
        GVCTF_ZOOM_OUT              :1
        GVCTF_REDRAW                :1
        GVCTF_PAGE_LEFT             :1
        GVCTF_PAGE_RIGHT            :1
        GVCTF_PAGE_UP               :1
        GVCTF_PAGE_DOWN             :1
        GVCTF_ADJUST_ASPECT_RATIO   :1
        GVCTF_APPLY_TO_ALL          :1
        GVCTF_SHOW_HORIZONTAL       :1
        GVCTF_SHOW_VERTICAL         :1
    GVCToolboxFeatures      end

**Library:** Objects/gViewCC.def

[Structures D-F](asmstrdf.md) <-- [Table of Contents](../asmref.md) &nbsp;&nbsp; --> [Structures H-M](asmstrhm.md)

