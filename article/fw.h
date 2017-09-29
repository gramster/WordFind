// fw.h - A Lightweight C++ Class Framework for PalmOS
//
// Written by Graham Wheeler, gramster@bigfoot.com
//
// This framework adds classes for databases, forms,
// list and field objects, and applications.
// It does not yet have any specific support for
// table or gadget objects, and also has no support
// yet for networking or conduits, system find, or
// saving/restoring application preferences in the
// App preferences database. Nontheless it is
// a good starting point for programming PalmOS 
// applications in C++, and has been used as the basis 
// of several successful shareware PalmOS applications.
// It should be easily extensible to handle additional
// requirements.
//
// Using Metrowerks CodeWarrior R6 this framework 
// compiles to between 5-10kb, depending on the features
// used and the optimisation settings selected.

// TODO: test whether dialogs get a HandleClose call.
// Test the list source that uses string resources.

#ifndef __FW_H
#define __FW_H

#define True			((Boolean)1)
#define False			((Boolean)0)

// Make things easier for C/C++ programmers

#define isspace(c)		((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)		((c)>='A' && (c)<='Z')
#define islower(c)		((c)>='a' && (c)<='z')
#define toupper(c)		((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define tolower(c)		((char)(isupper(c) ? ((c)+'a'-'A') : (c)))
#define isalpha(c)		(isupper(c) || islower(c))
#define isdigit(c)		((c)>='0' && (c)<='9')

#define strcpy(dst, src)	StrCopy(dst, src)
#define strcat(dst, src)	StrCat(dst, src)
#define strlen(s)		StrLen(s)
#define strcmp(s1, s2)		StrCompare(s1, s2)
#define strncmp(s1, s2, n)	StrNCompare(s1, s2, n)
#define strncpy(dst, src, n)	StrNCopy(dst, src, n)
#define memset(dst, v, n)	MemSet(dst, n, v)
#define memcpy(dst, src, n)	MemMove(dst, src, n)


// Do we need forms to manage widgets?

#if defined(USE_FIELDS) || defined(USE_LISTS)
#define USE_WIDGETS	1
#endif

// Database class wrapper. Currently this is just a sequential
// database. although insertions and deletions of arbitrary 
// records are allowed. There is not yet support for searching
// for specific records, or for using categories, although
// these could easily be added.

#ifdef USE_DATABASES

class Database
{
  protected:
  
    DmOpenRef	db;
    UInt32	type;
    UInt16	mode;
    UInt32	creator;
    UInt16	card;
    UInt32	id;
    int		numrecords;
    char	dbname[32];
    
  private:
  
    // After opening a database, Init initialises the number of records,
    // card number, local ID, and database name, type and creator.
    
    Boolean Init();

    Boolean Open(UInt32 type_in, LocalID i, UInt16 c, UInt16 mode_in, Char* name_in,
				UInt16 appinfosize_in, UInt32 creator_in);

  public:
  
    Database();
    
    // Get the pointer to the PalmOS-level db data structure,
    
    DmOpenRef GetOpenRef() const;
    
    // Tell whether the database is open or not
    
    Boolean IsOpen() const;
    
    // Get the number of records in the database
    
    int NumRecords() const;

    // Tell whether a record exists

    Boolean InRange(UInt16 recnum) const;
    
    // Get the card number, local ID, or database name
    
    UInt16 Card() const;
    UInt32 ID() const;
    const char *Name() const;
    
    // Open the database. The database to open can be specified in
    // a high-level way, by giving the type and creator ID, or in
    // a low-level way, by giving the memory card number and local ID
    // (which is the relative memory offset in the card). If the name
    // parameter is not NULL the database will be created if it does
    // not already exist.
    //
    // If you have multiple databases with the same creator and type,
    // then OpenByType will open the database with most recent revision
    // date. If you want to open an earlier revision, you will have to
    // determine the card number and local ID and use OpenByID instead.
    // Use DmGetNextDatabaseByTypeCreator to iterate through the
    // possible choices and determine the card and local IDs for each
    // possibility, or use a DatabaseListSource (see further below).
 
    Boolean OpenByType(UInt32 type_in, UInt16 mode_in = dmModeReadWrite,
    				Char* name_in = 0,
    				UInt16 appinfosize_in = 0,
    				UInt32 creator_in = MY_CREATOR_ID);
    				
    Boolean OpenByID(LocalID id, UInt16 card = 0, UInt16 mode_in = dmModeReadWrite,
    				Char* name_in = 0,
    				UInt32 type_in = 0,
    				UInt16 appinfosize_in = 0,
    				UInt32 creator_in = MY_CREATOR_ID);
    
    // Close the database. If forget_all is True, then clear the
    // database name, card and ID information too.
    
    UInt32 Close(Boolean forget_all = False);

    // To modify or view the contents of a record, first call GetRecord
    // or QueryRecord to get a handle to the contents. QueryRecord returns
    // a handle for read-only access, while GetRecord returns one for
    // read-write access. If using GetRecord, then when the modification or
    // viewing is complete, ReleaseRecord must be called, and passed a Boolean
    // specifying whether or not the record contents were altered.
    
    MemHandle QueryRecord(UInt16 recnum) const;
    MemHandle GetRecord(UInt16 recnum) const;
    void ReleaseRecord(UInt16 recnum, Boolean dirty = True) const;

    // Insert a new record, and return a handle to it. ReleaseRecord must
    // be called after the record has been modified.
    
    MemHandle NewRecord(UInt16 recnum, UInt16 size);

    // Check whether a record can be deleted - database must be open
    // for writes and record number must be in range.
    
    Boolean CanDeleteRecord(UInt16 recnum) const;
    
    // Delete a record. The record header isn't actually deleted, just marked
    // so that on the next sync the corresponding record on the host
    // will be archived.
    
    Boolean DeleteRecord(UInt16 recnum) const;
    
    // Really delete a record; the number of records get decremented.
    
    Boolean PurgeRecord(UInt16 recnum);
    
    // Resize an existing record and return a handle to the new memory block.
    // I think that ReleaseRecord should be called after modifying the
    // record, but this is not clear from the PalmOS dcumentation.
        
    MemHandle Resize(UInt16 recnum, UInt16 size) const;
    
    // The memory that the databases are stored in is read-only, and in order
    // to actually write data into a record's memory, you should use the
    // Write method. destptr is the pointer to the locked memory area,
    // and offset is the offset into this memory that we want to write to.
    // srcptr is the pointer to the data to be copied, and size is the number
    // of bytes of data that must be copied. 
    //
    // So, for example, to create a new record and copy some data into it, we
    // would use NewRecord, MemHandleLock, Write, MemHandleUnlock, and
    // ReleaseRecord, in that order.
    
    void Write(void *destptr, const void *srcptr, UInt32 size, UInt32 offset = 0) const;

    // Sort the database. Use QuickSort if it is unsorted, Insertion sort
    // if it is already mostly sorted.
    
    void QuickSort(DmComparF *cmp, Int16 other = 0) const;
    void InsertionSort(DmComparF *cmp, Int16 other = 0) const;
    
    // Get a (locked) pointer to the AppInfoBlock, and release it when done. To
    // write to the memory pointed to, Write must be used as the memory
    // is write-protected.
    
    MemPtr GetAppInfoPtr();
    void ReleaseAppInfoPtr(MemPtr aib);
    
    ~Database();
};

inline Boolean Database::OpenByType(UInt32 type_in, UInt16 mode_in, Char* name_in, 
				UInt16 appinfosize_in, UInt32 creator_in)
{
    return Open(type_in, 0, 0, mode_in, name_in, appinfosize_in, creator_in);
}

inline Boolean Database::OpenByID(LocalID i, UInt16 c, UInt16 mode_in, Char* name_in, UInt32 type_in,
				UInt16 appinfosize_in, UInt32 creator_in)
{
    return Open(type_in, i, c, mode_in, name_in, appinfosize_in, creator_in);
}

inline DmOpenRef Database::GetOpenRef() const
{
    return db;
}

inline Boolean Database::IsOpen() const
{
    return db ? True : False;
}

inline int Database::NumRecords() const
{
    return numrecords;
}

inline Boolean Database::InRange(UInt16 recnum) const
{
    return (numrecords>0 && recnum<(UInt16)numrecords) ?  True : False;
}

inline UInt16 Database::Card() const
{
    return card;
}

inline UInt32 Database::ID() const
{
    return id;
}

inline const char *Database::Name() const
{
    return dbname;
}

// Create a new record and return a read-write handle to it

inline MemHandle Database::NewRecord(UInt16 recnum, UInt16 size)
{
    ++numrecords;
    return DmNewRecord(db, &recnum, size);
}

// Resize a record, and return the new handle - I think this is
// a read-write handle but I'm not certain!

inline MemHandle Database::Resize(UInt16 recnum, UInt16 size) const
{
    return DmResizeRecord(db, recnum, size);
}

// Get a read-only handle to a record

inline MemHandle Database::QueryRecord(UInt16 recnum) const
{
    return InRange(recnum) ? DmQueryRecord(db, recnum) : 0;
}

// Get a read-write handle to a record

inline MemHandle Database::GetRecord(UInt16 recnum) const
{
    return InRange(recnum) ?  DmGetRecord(db, recnum) : 0;
}

// Release a read-write record handle

inline void Database::ReleaseRecord(UInt16 recnum, Boolean dirty) const
{
    if (InRange(recnum))
	DmReleaseRecord(db, recnum, dirty);
}

inline void Database::QuickSort(DmComparF *cmp, Int16 other) const
{
    if (db) DmQuickSort(db, cmp, other);
}

inline void Database::InsertionSort(DmComparF *cmp, Int16 other) const
{
    if (db) DmQuickSort(db, cmp, other);
}

inline void Database::ReleaseAppInfoPtr(MemPtr aib)
{
    if (aib) MemPtrUnlock(aib);
}   

#endif USE_DATABASES

//-------------------------------------------------------------------------------
// The base class for complex field elements

#ifdef USE_WIDGETS

class Widget
{
  protected:
  
    formObjects type;
    UInt16 resid;

  public:
  
    // Construct the Widget, given the owner form, resource ID and type
    
    Widget(class Form *owner_in, formObjects type_in, UInt16 id_in);
    
    // Return the resource ID
    
    UInt16 ID() const;
    
    // Return the type
    
    formObjects Type() const;

    // Activate the widget. Called after activating the owner form
    
    virtual Boolean Activate() = 0;
    
    virtual ~Widget();
};

inline formObjects Widget::Type() const
{
    return type;
}

inline UInt16 Widget::ID() const
{
    return resid;
}

#endif USE_WIDGETS

//-------------------------------------------------------------------------------
// List form objects are complex enough to merit their own class.
// It is also useful to separate the items in the list from the 
// user interface concerns, a la MVC philosophy - we use ListSources
// for this.

#ifdef USE_LISTS

// A ListSource is a `producer' of lines in a list

class ListSource
{
  public:
    ListSource();
    virtual UInt16 NumItems() = 0;
    virtual Boolean GetItem(Int16 itemnum, Char* buf, UInt16 buflen) = 0;
    virtual ~ListSource();
};

inline ListSource::ListSource()
{
}

// It seems the handler has no way of knowing the list ID, so that makes it tricky to
// have one handler for all lists. Instead, we use the next macro in each leaf class
// in the List hierarchy to supply a handler for that class.
//
// NB we must only do this if the List uses a ListSource! If a list has its items
// statically specified in the resource file it must not use this macro and must
// not pass a ListSource through in the list constructor.

#define LISTHANDLER(lid) static void ListDrawEventHandler(Int16 idx, RectanglePtr bounds, Char **data) \
				{ (void)data; Application::Instance()->DrawListItem(lid, idx, bounds); } \
			 virtual void InstallDrawHandler() const \
			 	{ LstSetDrawFunction(lst, ListDrawEventHandler); }

class List : public Widget
{
  protected:
  
    ListPtr lst;
    ListSource *source;
    Int16 select;
    
    virtual void InstallDrawHandler() const;
    
  public:
  
    // Construct the List, given the resource ID
    
    List(class Form *owner_in, UInt16 id_in, ListSource *source_in = 0);
    
    // Draw handler. idx is the index, and bounds the location and extent
    
    void DrawItem(Int16 idx, RectanglePtr bounds) const;
    
    // Erase the screen area used by the list
    
    void Erase() const;
    
    // Activate the list. Called after activating the owner form
    
    virtual Boolean Activate();
    
    // Get the total number of items in the list
    
    UInt16 NumItems();
    
    // Get the number of visible items in the list
    
    UInt16 NumVisibleItems() const;
    
    // Select a particular item and bring that into view
    
    void Select(Int16 itemnum);
    
    // Handle a select event
    
    virtual Boolean HandleSelect(Int16 selection);
    
    // Get the index of the selected item
    
    Int16 GetSelection() const;
    
    // Get the text of the selected item
    
    Char* GetSelectionText() const;
    
    virtual ~List();
};

// Define PARANOID for extra run-time error checking

#ifdef PARANOID
#define LISTGUARD	(lst)
#define LISTERR(x)	ErrNonFatalDisplay(x ": no such list")
#else
#define LISTGUARD	(1)
#define LISTERR(x)
#endif

inline UInt16 List::NumVisibleItems() const
{
    return LISTGUARD ? LstGetVisibleItems(lst) : ((UInt16)0);
}

inline void List::Erase() const
{
    if (LISTGUARD) LstEraseList(lst);
}

inline Int16 List::GetSelection() const
{
    return (LISTGUARD) ? LstGetSelection(lst) : noListSelection;
}

// The next routine will only work with static lists! It will cause
// a crash if the list is dynamic! Use the list source instead in
// such a case.

inline Char* List::GetSelectionText() const
{
    Int16 s = GetSelection();
    return (LISTGUARD && s!=noListSelection) ? LstGetSelectionText(lst, s) : 0;
}

//-----------------------------------------------------------------------
// Now we have a few different types of list sources
//
// A FixedListSource has a fixed number of items.

#ifdef USE_FIXED_LIST_SOURCES

class FixedListSource : public ListSource
{
    UInt16 numitems;
  public:
    FixedListSource(UInt16 numitems_in);
    virtual UInt16 NumItems();
    virtual Boolean GetItem(Int16 itemnum, Char* buf, UInt16 buflen) = 0;
    virtual ~FixedListSource();
};

inline FixedListSource::FixedListSource(UInt16 numitems_in)
    : ListSource(),
      numitems(numitems_in)
{
}

#endif USE_FIXED_LIST_SOURCES

//--------------------------------------------------------------------
// A DatabaseListSource is a list of databases

#ifdef USE_DATABASE_LIST_SOURCES

class DatabaseListSource : public ListSource
{
  protected:
    UInt32 creator;
    UInt32 type;
    char **dbnames;
    UInt16 *cards;
    LocalID *ids;
    UInt16 numdbs;
    
    void FreeChoices();
    virtual UInt16 NumItems();
  public:
    DatabaseListSource(UInt32 creator_in = 0, UInt32 type_in = 0);
    void InitChoices();
    Boolean HasItem(Int16 idx) const;
    Char* Name(Int16 idx) const;
    UInt16 Card(Int16 idx) const;
    LocalID ID(Int16 idx) const;
    virtual Boolean GetItem(Int16 itemNum, Char* buf, UInt16 buflen);
    Boolean Delete(Int16 idx);
    virtual ~DatabaseListSource();
};

inline Boolean DatabaseListSource::HasItem(Int16 idx) const
{
    return (idx>=0 && idx<numdbs) ? True : False;
}

inline Char* DatabaseListSource::Name(Int16 idx) const
{
    return (HasItem(idx) && dbnames) ? dbnames[idx] : "";
}

inline UInt16 DatabaseListSource::Card(Int16 idx) const
{
    return (HasItem(idx) && cards) ? cards[idx] : 0;
}

inline LocalID DatabaseListSource::ID(Int16 idx) const
{
    return (HasItem(idx) && ids) ? ids[idx] : 0;
}

#endif USE_DATABASE_LIST_SOURCES

//--------------------------------------------------------------------------------
// A RecordListSource is a set of records from a Database. To use this, derive
// a subclass with an appropriate Format method.

#ifdef USE_RECORD_LIST_SOURCES

class RecordListSource : public ListSource
{
  protected:
    Database *db;
    
    virtual UInt16 NumItems();
    virtual void Format(Char* buf, UInt16 buflen, MemPtr rec) const = 0;
  public:
    RecordListSource(Database *db_in = 0);
    void SetSource(Database *db_in);
    virtual Boolean GetItem(Int16 itemNum, Char* buf, UInt16 buflen);
    virtual ~RecordListSource();
};    

inline void RecordListSource::SetSource(Database *db_in)
{
    db = db_in;
}

#endif USE_RECORD_LIST_SOURCES
    
#endif USE_LISTS

//--------------------------------------------------------------------------------
// Text fields. These are arguably not complex enough to be put in their own class,
// but having a TextField class does have the benefit that it reduces the clutter
// of methods in the form class, and possibly also Form subclasses.

#ifdef USE_FIELDS

class Field : public Widget
{
  protected:
    FieldPtr fld;

  public:
  
    // Construct the field given the resource ID
    
    Field(class Form *owner_in, UInt16 resid_in);    
    
    // Activate the field; called after activating the form
    
    virtual Boolean Activate();
    
    // Set the text of the field, given a handle to some text
    
    void SetTextFromHandle(MemPtr h) const;
    
    // Get the length of the text
    
    UInt16 Length() const;
    
    // Delete a range of text in the field
    
    void DeleteRange(UInt16 start, UInt16 end) const;
    
    // Clear the field (delete all text)
    
    void Clear() const;
    
    // Get the text in the field, copy it into a given
    // buffer. If offset is not NULL, then return the insertion
    // point position as well.
    
    void Read(Char* buf, UInt16 maxlen, UInt16 *offset = 0) const;
    
    // Insert a character into the field at the insertion point
    
    void Insert(char c) const;
    
    // Insert a string into the field at the insertion point
    
    void Insert(const char *s) const;
    
    // Set the position of the insertion point. If the parameter is 
    // omitted or 0xFFFF, then position the insertion point at the 
    // end of the text.
    
    void SetOffset(UInt16 offset = 0xFFFF) const;
    
    // Set the text of the field AND the insertion point
    
    void Set(const char *text, UInt16 offset = 0xFFFF) const;
    
    // Get the position of the insertion point
    
    UInt16 GetOffset() const;
    
    // Get the maximum allowed length of the field
    
    UInt16 GetMaxLen() const;
    
    // Tell whether the text has changed since the field was last drawn
    
    Boolean IsDirty() const;
    
    // Draw the field
    
    void Draw() const;
    
    // Edit menu operations
    
    void Undo() const;
    void CopyToClipboard() const;
    void CutToClipboard() const;
    void PasteFromClipboard() const;
    void SelectRange(UInt16 start, UInt16 end) const;
    void SelectAll() const;
    
    virtual ~Field();
};

#ifdef PARANOID
#define FIELDGUARD	(fld)
#define FIELDERR(x)	ErrNonFatalDisplay(x ": no such field")
#else
#define FIELDGUARD	(1)
#define FIELDERR(x)
#endif

inline void Field::SetTextFromHandle(MemPtr h) const
{
    if (FIELDGUARD) FldSetTextHandle(fld, (MemHandle)h);
}

inline UInt16 Field::Length() const
{
 
   return FIELDGUARD ? FldGetTextLength(fld) : 0;
}

inline void Field::DeleteRange(UInt16 start, UInt16 end) const
{
    if (FIELDGUARD && start>=0 && end>start && end<=Length())
        FldDelete(fld, start, end);
}

inline void Field::Clear() const
{
    DeleteRange(0, Length());
}

inline void Field::Insert(char c) const
{
    if (FIELDGUARD) FldInsert(fld, &c, 1);
}

inline void Field::Insert(const char *s) const
{
    if (FIELDGUARD && s) FldInsert(fld, (Char*)s, (UInt16)StrLen((Char*)s));
}

inline UInt16 Field::GetOffset() const
{
    return FIELDGUARD ? FldGetInsPtPosition(fld) : 0;
}

inline UInt16 Field::GetMaxLen() const
{
    return FIELDGUARD ? FldGetMaxChars(fld) : 0;
}

inline Boolean Field::IsDirty() const
{
    return FIELDGUARD ? FldDirty(fld) : False;
}

inline void Field::Draw() const
{
    if (FIELDGUARD) FldDrawField(fld);
}

inline void Field::Undo() const
{
    if (FIELDGUARD) FldUndo(fld);
}

inline void Field::CopyToClipboard() const
{
    if (FIELDGUARD) FldCopy(fld);
}

inline void Field::CutToClipboard() const
{
    if (FIELDGUARD) FldCut(fld);
}

inline void Field::PasteFromClipboard() const
{
    if (FIELDGUARD) FldPaste(fld);
}

inline void Field::SelectRange(UInt16 start, UInt16 end) const
{
    if (FIELDGUARD) FldSetSelection(fld, start, end);
}

inline void Field::SelectAll() const
{
    SelectRange(0, Length());
}

#endif USE_FIELDS

//-----------------------------------------------------------------------------
// Every form in the application should be represented by a subclass
// of Form, with List, Field, and ScrollableField member variables as
// necessary, and redefined event handlers as necessary.

class Form
{
  protected:
    UInt16 id;
    FormPtr frm;
#ifdef USE_WIDGETS
    Widget **widgets;
    UInt16 numwidgets;
#endif USE_WIDGETS

    // Event handling. EventHandler is the static event handler used as a
    // callback by PalmOS. It figures out the appropriate Form class instance
    // that should handle the event, and invokes an appropriate handler method.
    // The latter are virtual so they can be extended or overridden in derived
    // Form classes. Once an event has been handled, True should be returned;
    // a return value of Flase indicates that the event has not been handled 
    // or that handling is incomplete.
    
    static Boolean EventHandler(EventPtr event);
    
    // Redefinable event handlers for specific event types
    
    virtual Boolean HandleScroll(UInt16 objID, Int16 oldv, Int16 newv);
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleMenu(UInt16 menuID);
#ifdef USE_LISTS
    virtual Boolean HandleListSelect(UInt16 listID, Int16 selection);
#endif    
    virtual Boolean HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection);
    virtual Boolean HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers);

    virtual Boolean HandleOpen(); // draw time
    virtual Boolean HandleUpdate();
    virtual void HandleClose();
    
  public:
  
    // Construct a form.
  
    Form(UInt16 id_in, UInt16 numwidgets_in = 0);

    // Return the resource ID of the form
        
    UInt16 ID() const;

    // Return the index (from 0) of a user interface object given
    // its resource ID. If the object doesn't exist, then it returns
    // noFocus (65535)
    
    UInt16 GetObjectIndex(UInt16 resid) const;
    
    // Get a pointer to a resource object in the form given
    // its resource ID. Returns NULL if there is no object.
    
    MemPtr GetObject(UInt16 resid) const;
    
    // GetTypedObject is similar to GetObject but enforces a
    // type restriction on the object; if the object is not of the
    // appropriate type NULL is returned.
    
    MemPtr GetTypedObject(UInt16 resid, FormObjectKind type) const;

    // Save/retrieve widget pointers
    
#ifdef USE_WIDGETS
    Boolean AddWidget(Widget *w); 
    Widget *GetWidget(UInt16 resid) const;
#endif

    // Set the focus
    
    void SetFocus(UInt16 resid) const;
       
    // Get a pointer to a ScrollBarType resource given the resource ID
    
    ScrollBarPtr GetScrollBar(UInt16 resid) const;
    
    // Similar to GetObject, but works only for control resources:
    // buttons, pushbuttons, checkboxes, popup and selector triggers,
    // and repeating buttons. Returns a pointer to a ControlType
    // structure
    
    ControlPtr GetControl(UInt16 resid) const;
    
    // GetTypedControl allows us to restrict to a specific type
    // of control resource
    
    ControlPtr GetTypedControl(UInt16 resid, ControlStyleType type) const;

    // Methods for doing useful things with controls

    void DrawControl(UInt16 id) const;
    void EnableControl(UInt16 id, int v = 1) const;
    void DisableControl(UInt16 id) const;
    void HideControl(UInt16 resid) const;
    void ShowControl(UInt16 resid) const;
    void SetControlLabel(UInt16 resid, const char *lbl) const;

    // Some additional useful operations on checkbox control objects
    
    void SetCheckBox(UInt16 resid, UInt16 val = 1) const;
    UInt16 IsCheckBoxSet(UInt16 resid) const;

    // Change the text associated with a label. The new text must not be
    // longer than the text used at compile time!
    
    void SetLabelText(UInt16 resid, const Char* lbl) const;
    
    // Methods for doing useful things with fields. Subclasses that use
    // Field classes must redefine GetField to return the appropriate
    // class instance.

#ifdef USE_FIELDS
    Field *GetField(UInt16 resid) const;
    Field *GetFieldWithFocus() const;
#endif

    // Low-level PalmOS field structure versions
    
    FieldPtr GetFieldPtr(UInt16 resid) const;
    FieldPtr GetFieldPtrWithFocus() const;

    // Subclasses that use List classes must redefine GetList to return
    // the appropriate List instance

#ifdef USE_LISTS
    List *GetList(UInt16 lidx) const;
#endif
    
    void PostUpdateEvent() const;	// Tell OS we need to redraw the form
    void Draw() const;			// draw the form
    Boolean IsActive() const;		// is this the active form?
    virtual Boolean Activate();		// initialise the form (pre-draw time)
    void Switch(UInt16 resid) const;	// activate a different form
    virtual ~Form();
};

#ifdef PARANOID
#define FORMGUARD	(lst)
#else
#define FORMGUARD	(1)
#endif

inline UInt16 Form::ID() const
{
    return id;
}

inline ControlPtr Form::GetControl(UInt16 resid) const
{
    return (ControlPtr)GetTypedObject(resid, frmControlObj);
}
    
inline FieldPtr Form::GetFieldPtr(UInt16 resid) const
{
    return (FieldPtr)GetTypedObject(resid, frmFieldObj);
}
    
inline ScrollBarPtr Form::GetScrollBar(UInt16 resid) const
{
    return (ScrollBarPtr)GetTypedObject(resid, frmScrollBarObj);
}

inline void Form::PostUpdateEvent() const
{
    FrmUpdateForm(id, frmRedrawUpdateCode);
}

inline void Form::Draw() const
{
    if (FORMGUARD) FrmDrawForm(frm);
}

inline void Form::Switch(UInt16 resid) const // activate a different form
{
    FrmGotoForm(resid);
}

//--------------------------------------------------------------------
// A Dialog is a modal form. 
// The calling form should just construct the dialog, call
// Run(), do any necessary post-processing, and then destroy
// the Dialog class instance. Often this can be done as simply
// as: result = MyDialogSubclass(resid).Run();
// More sophisticated behaviour can be achieved by overriding 
// the event handlers.

#ifdef USE_DIALOGS

class Dialog : public Form
{
  protected:
    Form *caller;
  public:
    Dialog(UInt16 id_in, UInt16 numwidgets_in = 0)
        : Form(id_in, numwidgets_in), caller(0)
    {}
    // We could add a full set of event handlers here; I've only 
    // needed the next two so far.

    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleScroll(UInt16 objid, Int16 oldv, Int16 newv);
    
    // Activate the form
    
    virtual Boolean Activate();
    
    // Execute the modal dialog
    
    virtual UInt16 Run();
    
    virtual ~Dialog();
};

#endif USE_DIALOGS

// The wrapper class for the application itself. An actual
// application should subclass this. The subclass should have
// Form (subclass) member variables for each form.
// Other virtual methods can be redefined as well, but the above
// are the minimal requirements.
// 
// If you want your application to save its state when exiting,
// you can do this by redefining the Stop method.


class Application
{
  protected:
    static Application *instance;
    Form **forms;
    UInt16 numforms;
    Form *activeform;
    UInt16 startformID;
    SystemPreferencesType sysPrefs;

    virtual int EventWaitTime() const;

    // If databases need to be opened/closed at the start/end of the
    // app, the next two methods can be redefined to do so.
    
    virtual Boolean OpenDatabases();
    virtual void CloseDatabases();
    
    // Event handling
    
    Boolean HandleLoadEvent(UInt16 formID);
    Boolean HandleEvent(EventType &event);
    void EventLoop();

    // Methods that can be redefined to do things at the beginning and
    // end of application execution. The Start method should return 
    // zero on success, or an error code suitable for the return value
    // of the PilotMain function.
    
    virtual UInt32 Start();
    virtual void Stop();
    
  public:
    Application(UInt16 numforms, UInt16 startformID_in);
    
    // Add a form to the forms table
    
    Boolean AddForm(Form *form);
    
    // Check for version compatibility
    
    static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags);
    
    // Save a pointer to the currently active form
    
    static void SetActiveForm(Form *f);
    
    // Tell whether a particular form is active
    
    static Boolean IsActive(const Form *f);
    
    // Return a pointer to the application
    
    static Application *Instance();
    
    // Tell whether secret records should be displayed or not
    
    static Boolean HideSecretRecords();
    
    // Useful method to set the letter case mode
    
    static void SetLetterCapsMode(Boolean newcaps);

#ifdef USE_LISTS    
    // DrawListItem is needed for the LISTHANDLER macro
    
    void DrawListItem(UInt16 lidx, Int16 idx, RectanglePtr bounds);
#endif

    // Get a pointer to a specific form
    
    Form *GetForm(UInt16 formID) const;
    
    // Get the pointer to the currently active form
    
    Form *GetForm() const;
    
    // The main entry point of the application class
    
    UInt32 Main(MemPtr cmdPBP, UInt16 launchflags);
    virtual ~Application();
};

void DrawDebugText(int x, int y, const char *txt);

#endif

