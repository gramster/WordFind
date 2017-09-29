#include "mypilot.h"

#include "fw.h"

Application *Application::instance;

#ifdef USE_DATABASES

Database::Database()
    : db(0),
      type(0),
      mode(0),
      creator(0),
      card(0),
      id(0),
      numrecords(-1)
{
    dbname[0]=0;
}

Boolean Database::Init()
{
    dbname[0] = 0;
    if (db)
    {
        numrecords = (int)DmNumRecords(db);
	if (DmOpenDatabaseInfo(db, &id, 0, 0, &card, 0) == 0)
	{
	    if (DmDatabaseInfo(card, id, dbname,0,0,0,0,0,0,0,0,&type,&creator) != 0)
		strcpy(dbname, "Default");
	}
	else
	{
	    card = 0;
	    id = 0;
	}
	return True;
    }
    else
    {
        numrecords = -1;
	card = 0;
	id = 0;
        return False;
    }
}

Boolean Database::Open(UInt32 type_in, LocalID i, UInt16 c, UInt16 mode_in, Char* name_in,
				UInt16 appinfosize_in, UInt32 creator_in)
{
    if (!Application::HideSecretRecords())
        mode_in |= dmModeShowSecret;
    if (name_in == 0) appinfosize_in = 0;
    mode = mode_in;
  retry:
    if (c == 0 && i == 0)
        db = DmOpenDatabaseByTypeCreator(type_in, creator_in, mode);
    else
        db = DmOpenDatabase(c, i, mode);
    if (db == 0 && name_in)
    {
        if (DmCreateDatabase(0, name_in, creator_in, type_in, False) == 0)
        {
            name_in = 0; // to prevent another retry
            goto retry;
        }
        else ErrNonFatalDisplay("Could not create database");
    }
    Boolean rtn = Init();
    if (rtn && name_in == 0 && appinfosize_in > 0) // newly created, must make appinfoblock
    {
        MemHandle h = DmNewHandle(db, appinfosize_in);
        if (h)
        {
            LocalID aibID = MemHandleToLocalID(h);
            DmSetDatabaseInfo(card, id, 0, 0, 0, 0, 0, 0, 0, &aibID, 0, 0, 0);
            // initialise it to zero bytes
            MemPtr p = MemHandleLock(h);
            DmSet(p, 0, appinfosize_in, 0);
            MemPtrUnlock(p);
        }
    }
    return rtn;
}

// Copy data into a record's memory

void Database::Write(void *destptr, const void *srcptr, UInt32 size, UInt32 offset) const
{
    DmWrite(destptr, offset, srcptr, size); 
}

Boolean Database::CanDeleteRecord(UInt16 recnum) const
{
    // XXX we rely on dmModeReadWrite having the same bit set as used 
    // for dmModeWrite
    return (db && InRange(recnum) && (mode&dmModeWrite) != 0) ?  True : False;
}
 
Boolean Database::DeleteRecord(UInt16 recnum) const
{
    // note that this deletes the data but not the header, which simply
    // gets flagged as deleted. For this reason we don't decrement
    // numrecords. It is advisable to sort the database after deletions,
    // which will move the deleted blocks to the end, and then recompute
    // numrecords based on the number of records that are not deleted.
    return (CanDeleteRecord(recnum) && DmDeleteRecord(db, recnum) == 0) ? True : False;
}

Boolean Database::PurgeRecord(UInt16 recnum)
{
    if (CanDeleteRecord(recnum))
    {
        if (DmRemoveRecord(db, recnum) == 0)
        {
            --numrecords;
            return True;
        }
        else ErrNonFatalDisplay("Failed to delete record");
    }
    return False;
}

UInt32 Database::Close(Boolean forget_all)
{
    if (db)
    {
        UInt32 error = ::DmCloseDatabase(db);
	if (error) return error;
	db = 0;
    }
    numrecords = -1;
    if (forget_all)
    {
        card = 0;
        id = 0;
        dbname[0] = 0;
    }
    return 0;
}

MemPtr Database::GetAppInfoPtr()
{
    if (db) 
    {
        LocalID aibID = DmGetAppInfoID(db);
        return MemLocalIDToLockedPtr(aibID, card);
    }
    return 0;
}

Database::~Database()
{
    if (db) Close();
}

#endif USE_DATABASES

//---------------------------------------------------------------------

//-----------------------------------------------------------------

#ifdef USE_WIDGETS

Widget::Widget(Form *owner_in, formObjects type_in, UInt16 resid_in)
   : type(type_in),
     resid(resid_in)
{
    owner_in->AddWidget(this);
}

Widget::~Widget()
{
}

#endif USE_WIDGETS

//-----------------------------------------------------------------

#ifdef USE_LISTS

ListSource::~ListSource()
{
}

#ifdef USE_FIXED_LIST_SOURCES

UInt16 FixedListSource::NumItems()
{
    return numitems;
}

FixedListSource::~FixedListSource()
{
}

#endif USE_FIXED_LIST_SOURCES

//-----------------------------------------------------------------------

#ifdef USE_DATABASE_LIST_SOURCES

DatabaseListSource::DatabaseListSource(UInt32 creator_in, UInt32 type_in)
  : ListSource(),
    creator(creator_in),
    type(type_in),
    dbnames(0),
    cards(0),
    ids(0),
    numdbs(0)
{
    InitChoices();
}

Boolean DatabaseListSource::Delete(Int16 idx)
{
    if (HasItem(idx))
    {
        DmDeleteDatabase(cards[idx], ids[idx]);
	--numdbs;
	delete [] dbnames[idx];
	for (Int16 i = idx; i < (Int16)numdbs; i++)
	{
	    dbnames[i] = dbnames[i+1];
	    cards[i] = cards[i+1];
	    ids[i] = ids[i+1];
	}
	return True;
    }
    return False;
}

UInt16 DatabaseListSource::NumItems()
{
    return numdbs;
}

void DatabaseListSource::FreeChoices()
{
    for (UInt16 i = 0; i < numdbs; i++)
        delete [] dbnames[i];
    delete [] dbnames;
    dbnames = 0;
    delete [] cards;
    cards = 0;
    delete [] ids;
    ids = 0;
    numdbs = 0;
}

void DatabaseListSource::InitChoices()
{
    Err x;
    UInt16 card;
    LocalID id;
    Boolean newsearch = True;
    DmSearchStateType state;
    int cnt = 0;
    FreeChoices();
    while ((x = DmGetNextDatabaseByTypeCreator(newsearch, &state, type,
    						creator, False, &card, &id))==0)
    {
        newsearch = False;
	cnt++;
    }
    if (cnt)
    {
        dbnames = new char*[cnt];
        cards = new UInt16[cnt];
        ids = new LocalID[cnt];
        newsearch = True;
        while ((x = DmGetNextDatabaseByTypeCreator(newsearch, &state,
    					type, creator, False,
					&card, &id)) == 0)
        {
            char name[32];
            newsearch = False;
	    if (DmDatabaseInfo(card, id, name,0,0,0,0,0,0,0,0,0,0) == 0)
	    {
	        dbnames[numdbs] = new char[strlen(name)+1];
	        if (dbnames[numdbs])
	        {
	            cards[numdbs] = card;
	            ids[numdbs] = id;
	            StrCopy(dbnames[numdbs++], name);
	        }
	    }
        }
    }
}

Boolean DatabaseListSource::GetItem(Int16 itemNum, Char* buf, UInt16 buflen)
{
    StrNCopy(buf, Name(itemNum), (Int16)buflen-1);
    buf[buflen-1] = 0;
    return HasItem(itemNum) ? True : False;
}

DatabaseListSource::~DatabaseListSource()
{
    FreeChoices();
}

#endif USE_DATABASE_LIST_SOURCES

//-----------------------------------------------------------------------------

#ifdef USE_RECORD_LIST_SOURCES

RecordListSource::RecordListSource(Database *db_in)
    : ListSource(), db(db_in)
{
}

UInt16 RecordListSource::NumItems()
{
    return db ? db->NumRecords() : (UInt16)0;
}

Boolean RecordListSource::GetItem(Int16 itemNum, Char* buf, UInt16 buflen)
{
    MemHandle h = db ? db->QueryRecord((UInt16)itemNum) : 0;
    if (h)
    {
        MemPtr p = MemHandleLock(h);
        if (p)
        {
            Format(buf, buflen, p);
            MemHandleUnlock(h);
            return True;
        }
    }
    buf[0] = 0;
    return False;
}

RecordListSource::~RecordListSource()
{
}

#endif USE_RECORD_LIST_SOURCES
  
//--------------------------------------------------------------------------
    
List::List(Form *owner_in, UInt16 id_in, ListSource *source_in)
   : Widget(owner_in, frmListObj, id_in),
     lst(0),
     source(source_in),
     select(noListSelection)
{
}

void List::InstallDrawHandler() const
{
}

Boolean List::Activate()
{
    Form *af = Application::Instance()->GetForm();
    lst = af ? ((ListPtr)af->GetTypedObject(resid, frmListObj)) : 0;
    if (lst && source)
    {
    	// XXX the PalmOS SDK docs claim the third param is a UInt16
    	// but the header files declare it as an Int16
	LstSetListChoices(lst, (char**)0, (Int16)NumItems());
	InstallDrawHandler();
	return True;
    }
    return False;
}

Boolean List::HandleSelect(Int16 selection)
{
    select = selection;
    return False;
}

UInt16 List::NumItems()
{
    if (LISTGUARD)
    {
        return source ? source->NumItems() : LstGetNumberOfItems(lst);
    }
    else LISTERR("NumItems");
    return 0;
}

void List::Select(Int16 itemnum)
{
    if (LISTGUARD)
    {
        if (itemnum>= 0 && itemnum < (Int16)NumItems())
        {
            LstSetSelection(lst, select = itemnum);
            LstMakeItemVisible(lst, itemnum);
        }
    }
    else LISTERR("Select");
}

void List::DrawItem(Int16 idx, RectanglePtr bounds) const
{
    // must be careful - this could blow the stack if very little 
    // memory is left!
    char item[32];
    if (source && source->GetItem(idx, item, sizeof(item)))
   	WinDrawChars(item, (Int16)StrLen(item), bounds->topLeft.x, bounds->topLeft.y);
}


List::~List()
{
}

#endif USE_LISTS

//-----------------------------------------------------------------

#ifdef USE_FIELDS

Field::Field(Form *owner_in, UInt16 resid_in)
    : Widget(owner_in, frmFieldObj, resid_in),
      fld(0)
{
}

Boolean Field::Activate()
{
    Form *owner = Application::Instance()->GetForm();
    fld = owner ? owner->GetFieldPtr(resid) : 0;
    return fld ? True : False;
}

void Field::Read(Char* buf, UInt16 maxlen, UInt16 *offset) const
{
    buf[0] = 0;
    if (FIELDGUARD)
    {
        if (offset) *offset = FldGetInsPtPosition(fld);
        Char* p = FldGetTextPtr(fld);
        if (p)
        {
            StrNCopy(buf, p, (Int16)(maxlen-1));
            buf[maxlen-1] = 0;
        }
    }
    else FIELDERR("Read");
}

void Field::SetOffset(UInt16 offset) const
{
    if (FIELDGUARD)
    {
	if (offset<=Length())
	    FldSetInsPtPosition(fld, (UInt16)offset);
    }
    else FIELDERR("SetOffset");
}

void Field::Set(const char *txt, UInt16 offset) const
{
    if (FIELDGUARD)
    {
        FldDelete(fld, 0, FldGetTextLength(fld));
        if (txt)
        {
            UInt16 l = (UInt16)StrLen(txt);
            FldInsert(fld, (Char*)txt, l);
	    if (offset<=l)
	        FldSetInsPtPosition(fld, offset);
	}
    }
    else FIELDERR("Set");
}

Field::~Field()
{}

#endif USE_FIELDS

//-----------------------------------------------------------------

Form::Form(UInt16 id_in, UInt16 numwidgets_in)
    : id(id_in), frm(0)
#ifdef USE_WIDGETS
    , widgets(0), numwidgets(numwidgets_in)
#endif USE_WIDGETS
{
    Application::Instance()->AddForm(this);
#ifdef USE_WIDGETS
    if (numwidgets)
    {
        widgets = new Widget*[numwidgets];
        if (widgets)
        {
            for (UInt16 i = 0; i < numwidgets; i++)
                widgets[i] = 0;
        }
        else ErrFatalDisplay("Could not allocate widget table");
    }
#else
    (void)numwidgets_in;
#endif
}

UInt16 Form::GetObjectIndex(UInt16 resid) const
{
    return frm ? FrmGetObjectIndex(frm, resid) : noFocus;
}

MemPtr Form::GetObject(UInt16 resid) const
{
    UInt16 oi = GetObjectIndex(resid);
    if (oi == noFocus)
    {
        ErrNonFatalDisplay("GetObject: A form object with the specified resource ID does not exist");
        return 0;
    }
    return FrmGetObjectPtr(frm, oi);
}

void Form::SetFocus(UInt16 resid) const
{
    if (FORMGUARD)
        FrmSetFocus(frm, FrmGetObjectIndex(frm, resid));
}

MemPtr Form::GetTypedObject(UInt16 resid, FormObjectKind type) const
{
    if (FORMGUARD)
    {
        UInt16 oi = GetObjectIndex(resid);
        if (oi == noFocus || FrmGetObjectType(frm, oi) != type)
        {
            ErrNonFatalDisplay("GetTypedObject: A form object is not of the expected type");
            return 0;
        }
        return FrmGetObjectPtr(frm, oi);
    }
    else return 0;
}

#if 0    
ControlPtr Form::GetTypedControl(UInt16 resid, ControlStyleType type) const
{
    ControlPtr rtn = GetControl(resid);
    if (rtn && rtn->style == type) return rtn;
    ErrNonFatalDisplay("GetTypedControl: A form object is not of the expected control type");
    return 0;
}
#endif

void Form::DrawControl(UInt16 resid) const
{
    ControlPtr b = GetControl(resid);
    if (b)
    {
        CtlSetUsable(b, 1);
        CtlDrawControl(b);
    }
}

void Form::EnableControl(UInt16 resid, int v) const
{
    ControlPtr b = GetControl(resid);
    if (b) CtlSetEnabled(b, (unsigned char)v);
}

void Form::DisableControl(UInt16 resid) const
{
    ControlPtr b = GetControl(resid);
    if (b) CtlSetEnabled(b, 0);
}

void Form::HideControl(UInt16 resid) const
{
    ControlPtr b = GetControl(resid);
    if (b) CtlHideControl(b);
}

void Form::ShowControl(UInt16 resid) const
{
    ControlPtr b = GetControl(resid);
    if (b) CtlShowControl(b);
}

void Form::SetControlLabel(UInt16 resid, const char *lbl) const
{
    ControlPtr sel = GetControl(resid);
    if (sel) CtlSetLabel(sel, (Char*)lbl);
}

// XXX be very careful with the next routine: never set the label text to be something
// longer than the one used at compile time, or an overrun will occur and cause 
// corruption!

void Form::SetLabelText(UInt16 resid, const Char* lbl) const
{
    UInt16 oi = GetObjectIndex(resid);
    if (oi != noFocus)
    {
        FrmHideObject(frm, oi);
        FrmCopyLabel(frm, resid, lbl);
        FrmShowObject(frm, oi);
    }
    else ErrNonFatalDisplay("SetLabelText: No label with the given resource ID");
}

#ifdef USE_FIELDS

Field *Form::GetFieldWithFocus() const
{
    UInt16 focus = FORMGUARD ? FrmGetFocus(frm) : noFocus;
    return (focus == noFocus) ? 0 : GetField(FrmGetObjectId(frm, focus));
}

#endif USE_FIELDS

FieldPtr Form::GetFieldPtrWithFocus() const
{
    UInt16 focus = FORMGUARD ? FrmGetFocus(frm) : 0;
    if (focus == noFocus || FrmGetObjectType(frm, focus) != frmFieldObj)
        return 0;
    else
        return (FieldPtr)FrmGetObjectPtr(frm, focus);
}

void Form::SetCheckBox(UInt16 resid, UInt16 val) const
{
#if 0
    ControlPtr cm = GetTypedControl(resid, checkboxCtl);
#else
    ControlPtr cm = GetControl(resid);
#endif
    if (cm) CtlSetValue(cm, (Int16)val);
}

UInt16 Form::IsCheckBoxSet(UInt16 resid) const
{
#if 0
    ControlPtr cm = GetTypedControl(resid, checkboxCtl);
#else
    ControlPtr cm = GetControl(resid);
#endif
    return (UInt16)((cm && CtlGetValue(cm)) ? 1 : 0);
}

//------------------------------------------------------------------------
// Event Handling

Boolean Form::EventHandler(EventPtr event) // static
{
    Boolean handled = False;
    Form *form = Application::Instance()->GetForm();
    if (form) switch (event->eType)
    {
    
        // Events affecting the active form
        
    case frmOpenEvent:
        // app must initialise and draw form.
        // form ID is in event->data.frmOpen.formID
	handled = form->HandleOpen();
	break;
#if 0
    case frmGotoEvent:
        // not required yet
        break;
    case frmLoadEvent:
        // what should we do here, if anything?
        // the formID is in event->data.frmLoad.formID
        break;
    case frmSaveEvent:
        // not required yet. No data is passed.
        break;
#endif
    case frmCloseEvent:
        // app must close and free up form.
        // form ID is in event->data.frmOpen.formID
        // if we don't handle it, then the FormObject will be 
        // delete by PalmOS, but not our Form object, so we need
        // to use care here!
		form->HandleClose();
    	break;
    case frmUpdateEvent:
        // redraw the form. The event data contains the form
        // ID and the update reason code
		handled = form->HandleUpdate();
		break;
#if 0
    case frmTitleSelectEvent:
        // not required yet
        break;
	
	// Events affecting controls. Typically we would only want to
	// bother with ctlSelectEvent. ctlRepeatEvent is useful for
	// handling repeating buttons. ctlEnterEvent is usually only
	// of interest if we want to dynically update a control when
	// it is tapped (e.g. filling in the contents of a popup list),
	// in which case we may also want to handle ctlExitEvent if we
	// need to free any resources that were allocated by the handler
	// for ctlEnterEvent. Note that a control that gets a ctlEnterEvent
	// will get either a ctlSelectEvent or a ctlExitEvent, but 
	// not both (ctlExitEvent means the stylus moved out of the
	// control before it was lifted).

    case ctlEnterEvent: // data has control ID and ControlPtr
    case ctlExitEvent:
    case ctlRepeatEvent: // data also has timer ticks
        // not required yet
        break;
#endif

    case ctlSelectEvent:
		handled = form->HandleSelect(event->data.ctlSelect.controlID);
		break;

#if 0	
    case daySelectEvent:
        // not required yet
        break;
    
    case fldChangedEvent:
    case fldEnterEvent:
    case fldHeightChangedEvent:
        // not required yet
        break;
#endif
        
    case keyDownEvent:
        handled = form->HandleKeyDown(event->data.keyDown.chr, 
        			      event->data.keyDown.keyCode,
        			      event->data.keyDown.modifiers);
        break;

	// Events affecting lists. As for controls, we are usually
	// only interested in handling the lstSelectEvent.
#if 0	
    case lstEnterEvent:
    case lstExitEvent:
        // not required yet
        break;            
#endif

    case lstSelectEvent:
#ifdef USE_LISTS
		(void)form->HandleListSelect(event->data.lstSelect.listID,
	      			     event->data.lstSelect.selection);
#endif
		handled = False; // important; PalmOS may need to do more!
		break;
	
    case menuEvent:
		handled = form->HandleMenu(event->data.menu.itemID);
		break;

#if 0
    case nilEvent: // timeout event
        // not required yet
        break;

    case penDownEvent:
    case penMoveEvent:
        // not required yet
        break;
#endif
    case penUpEvent:
        handled = form->HandlePenUp((UInt16)event->screenX, (UInt16)event->screenY);
        break;
               
    case popSelectEvent:
		handled = form->HandlePopupListSelect(event->data.popSelect.controlID,
					      event->data.popSelect.listID,
					      event->data.popSelect.selection);
		break;

#if 0
    case sclEnterEvent:
    case sclExitEvent:
        // not required yet
        break;
#endif

    case sclRepeatEvent:
		handled = form->HandleScroll(event->data.sclRepeat.scrollBarID,
				     event->data.sclRepeat.value,
				     event->data.sclRepeat.newValue);
		break;
	
#if 0
	// Events affecting tables; like controls and lists we typically
	// care only about the tblSelectEvent.
	
    case tblEnterEvent:
    case tblExitEvent:
    case tblSelectEvent:
        // not required yet
        break;
        
    case winEnterEvent:
    case winExitEvent:
        // not required yet
        break;
#endif
        
    default:
        break;
    }
    return handled;
}

Boolean Form::HandleSelect(UInt16 objID)
{
    (void)objID;
    return False;
}

Boolean Form::HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection)
{
    (void)triggerID;
    (void)listID;
    (void)selection;
    return False;
}

#ifdef USE_LISTS

Boolean Form::HandleListSelect(UInt16 listID, Int16 selection)
{
    List *l = GetList(listID);
    return l ? l->HandleSelect(selection) : False;
}

#endif USE_LISTS

Boolean Form::HandleScroll(UInt16 objID, Int16 oldv, Int16 newv)
{
    (void)objID;
    (void)oldv;
    (void)newv;
    return False;
}

Boolean Form::HandleKeyDown(UInt16 chr, UInt16 keyCode, UInt16 &modifiers)
{
    (void)chr;
    (void)keyCode;
    (void)modifiers;
    return False;
}

Boolean Form::HandlePenUp(UInt16 screenX, UInt16 screenY)
{
    (void)screenX;
    (void)screenY;
    return False;
}

Boolean Form::HandleMenu(UInt16 menuID)
{
    if (FORMGUARD)
    {
#ifdef USE_FIELDS
        Field *fld = GetFieldWithFocus();
#endif
        switch (menuID)
        {
#ifdef USE_FIELDS
#ifdef EditCut
	case EditCut:
	    if (fld) fld->CutToClipboard();
	    return True;
#endif
#ifdef EditCopy
	case EditCopy:
	    if (fld) fld->CopyToClipboard();
	    return True;
#endif
#ifdef EditPaste
	case EditPaste:
	    if (fld) fld->PasteFromClipboard();
	    return True;
#endif
#ifdef EditSelectAll
	case EditSelectAll:
	    if (fld) fld->SelectAll();
	    return True;
#endif
#ifdef EditUndo
	case EditUndo:
	    if (fld) fld->Undo();
	    return True;
#endif
#endif USE_FIELDS
#ifdef EditKeyboard
	case EditKeyboard:
            SysKeyboardDialog(kbdDefault);
	    return True;
#endif
#ifdef EditGraffiti
	case EditGraffiti:
            SysGraffitiReferenceDialog(referenceDefault);
	    return True;
#endif
	default:
	    break;
        }
    }
    return False;
}

Boolean Form::HandleOpen() 
{
    // NB Dialogs don't get frmOpenEvents and mustn't require
    // this (see PP pg 103)
    Draw();
    return True;
}

Boolean Form::HandleUpdate()
{
    return False;
}

void Form::HandleClose()
{
    // NB dialogs don't get this, so don't do any dialog
    // cleanup here!
    frm = 0;
}

//--------------------------------------------------------------------

Boolean Form::IsActive() const
{
    return Application::IsActive(this);
}

#ifdef USE_WIDGETS

Boolean Form::AddWidget(Widget *w)
{
    for (UInt16 i = 0; i < numwidgets; i++)
    {
        if (widgets[i] == 0)
        {
            widgets[i] = w;
            return True;
        }
    }
    ErrFatalDisplay("Form::AddWidget: not enough space for widgets");
    return False;
}

#endif

Boolean Form::Activate()
{
    // allocate and initialise the FormObject data structure.
    // the form is not drawn and doesn't get sent any events
    // yet.
    if (frm == 0)
        frm = FrmInitForm(id);
    if (frm)
    {
        FrmSetEventHandler(frm, Form::EventHandler);
        FrmSetActiveForm(frm);
        Application::SetActiveForm(this);
        Boolean rtn = True;
#ifdef USE_WIDGETS
        for (UInt16 i = 0; i < numwidgets; i++)
            if (widgets[i] && !widgets[i]->Activate())
                rtn = False;
#endif USE_WIDGETS
        return rtn;
    }
    return False;
}

#ifdef USE_WIDGETS

Widget *Form::GetWidget(UInt16 id) const
{
    for (UInt16 i = 0; i < numwidgets; i++)
        if (widgets[i] && widgets[i]->ID() == id)
            return widgets[i];
    return 0;
}

#endif USE_WIDGETS

#ifdef USE_FIELDS

Field *Form::GetField(UInt16 id) const
{
    Widget *w = GetWidget(id);
    if (w && w->Type() == frmFieldObj)
        return (Field*)w;
    return 0;
}

#endif USE_FIELDS

#ifdef USE_LISTS

List *Form::GetList(UInt16 id) const
{
    Widget *l = GetWidget(id);
    if (l && l->Type() == frmListObj)
        return (List*)l;
    return 0;
}

#endif USE_LISTS

Form::~Form()
{
#ifdef USE_WIDGETS
    delete [] widgets;
#endif USE_WIDGETS
}

//-----------------------------------------------------------------------

#ifdef USE_DIALOGS

Boolean Dialog::Activate()
{
    caller = Application::Instance()->GetForm();
    return Form::Activate();
}

Boolean Dialog::HandleSelect(UInt16 objID)
{
    return Form::HandleSelect(objID);
}

Boolean Dialog::HandleScroll(UInt16 objID, Int16 oldv, Int16 newv)
{
    return Form::HandleScroll(objID, oldv, newv);
}
     
UInt16 Dialog::Run()
{
    return Activate() ? FrmDoDialog(frm) : 0;
}

Dialog::~Dialog()
{
    if (caller)
	caller->Activate();
    if (frm)
	FrmDeleteForm(frm);
}

#endif USE_DIALOGS

//------------------------------------------------------------

Application::Application(UInt16 numforms_in, UInt16 startformID_in)
    : forms(0), numforms(numforms_in), activeform(0), startformID(startformID_in)
{
    if (instance) ErrNonFatalDisplay("Multiple application instances!");
    instance = this;
    forms = new Form*[numforms];
    if (forms)
    {
        for (UInt16 i = 0; i < numforms; i++)
            forms[i] = 0;
    }
}

Application::~Application()
{
    delete [] forms;
}

Boolean Application::AddForm(Form *f)
{
    for (UInt16 i = 0; i < numforms; i++)
    {
        if (forms[i] == 0)
        {
            forms[i] = f;
            return True;
        }
    }
    return False;
}

Boolean Application::OpenDatabases()
{
    return True;
}

void Application::CloseDatabases()
{
}

Form *Application::GetForm(UInt16 formID) const
{
    if (forms == 0) return 0;
    for (UInt16 i = 0; i < numforms; i++)
    {
        if (forms[i] && forms[i]->ID() == formID)
            return forms[i];
    }
    return 0;
}

Form *Application::GetForm() const
{
    return activeform;
}

Application *Application::Instance()
{
    return instance;
}

void Application::SetActiveForm(Form *f)
{
    if (instance) instance->activeform = f;
}

Boolean Application::IsActive(const Form *f)
{
    return (instance && (instance->GetForm() == f));
}

void Application::SetLetterCapsMode(Boolean newcaps)
{
    Boolean caps, num, aut;
    UInt16 temp;
    GrfGetState(&caps, &num, &temp, &aut);
    GrfSetState(newcaps, num, False);
}

Boolean Application::HideSecretRecords()
{
    if (instance && instance->sysPrefs.hideSecretRecords)
        return true;
    else
        return false;
}

Err Application::RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
    UInt32 romVersion;
	
    // See if we're on in minimum required version of the ROM or later.
    // The system records the version number in a feature.  A feature is a
    // piece of information which can be looked up by a creator and feature
    // number.

    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);

    if (romVersion < requiredVersion)
    {
	// If the user launched the app from the launcher, explain
	// why the app shouldn't run.  If the app was contacted for something
	// else, like it was asked to find a string by the system find, then
	// don't bother the user with a warning dialog.  These flags tell how
	// the app was launched to decided if a warning should be displayed.

	if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
	    	(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
	{
	  //  FrmAlert (RomIncompatibleAlert);
	    // Pilot 1.0 will continuously relaunch this app unless we switch to 
	    // another safe one.  The sysFileCDefaultApp is considered "safe".
	    if (romVersion < 0x02000000)
		AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
	}
	return (sysErrRomIncompatible);
    }
    return 0;
}

int Application::EventWaitTime() const
{
    return evtWaitForever;
}

Boolean Application::HandleLoadEvent(UInt16 formID)
{
    Form *form = GetForm(formID);
    if (form)
    {
        if (form->Activate())
            return True;
        else
            ErrNonFatalDisplay("Form activation failure");
    }
    return False;
}

Boolean Application::HandleEvent(EventType &event)
{
    Boolean handled = False;
    switch (event.eType)
    {
    case frmLoadEvent:
        handled = HandleLoadEvent(event.data.frmLoad.formID);
	break;
    default:
	break;
    }
    return handled;
}

UInt32 Application::Start()
{
    ::PrefGetPreferences(&sysPrefs);
    return OpenDatabases() ? 0 : 1; // should be returning an error code to PilotMain
}

void Application::EventLoop()
{
    EventType event;
    do
    {
        ::EvtGetEvent(&event, EventWaitTime());
	if (!::SysHandleEvent(&event) || event.eType == penUpEvent)
	{
	    UInt16 error;
	    if (!::MenuHandleEvent(0, &event, &error))
	    {
	        if (!HandleEvent(event))
		    ::FrmDispatchEvent(&event);
	    }
	}
    } while (event.eType != appStopEvent);
}

void Application::Stop()
{
    FrmCloseAllForms();
    CloseDatabases();
}

UInt32 Application::Main(MemPtr cmdPBP, UInt16 launchflags)
{
    UInt32 error;
    (void)cmdPBP;
    (void)launchflags;
    if ((error = Start()) == 0)
    {
	Form *f = GetForm(startformID);
	if (f)
	{
	    f->Switch(startformID);
	    EventLoop();
	}
	Stop();
    }
    return error;
}

#ifdef USE_LISTS
void Application::DrawListItem(UInt16 lidx, Int16 idx, RectanglePtr bounds)
{
    if (idx<0) return;
    Form *f = GetForm();
    if (f)
    {
        List *l = f->GetList(lidx);
	if (l) l->DrawItem(idx, bounds);
    }
}
#endif

extern UInt32 RunApplication(MemPtr cmdPBP, UInt16 launchflags);

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchflags)
{
    (void)cmdPBP;
    (void)launchflags;
    UInt32 error = Application::RomVersionCompatible (0x02000000, launchflags);
    if (error == 0) 
    {
        switch (cmd)
        {
        case sysAppLaunchCmdNormalLaunch:
            return RunApplication(cmdPBP, launchflags);
        case sysAppLaunchCmdFind:
        case sysAppLaunchCmdGoTo:
        case sysAppLaunchCmdSyncNotify:
        case sysAppLaunchCmdTimeChange:
        case sysAppLaunchCmdSystemReset:
        case sysAppLaunchCmdAlarmTriggered:
        case sysAppLaunchCmdDisplayAlarm:
        case sysAppLaunchCmdCountryChange:
        case sysAppLaunchCmdSyncRequest:
        case sysAppLaunchCmdSaveData:
        case sysAppLaunchCmdInitDatabase:
            error = 0;
	    break;
        default:
            error = sysErrParamErr;
            break;
        }
    }
    return error;
}

//------------------------------------------------------------------

