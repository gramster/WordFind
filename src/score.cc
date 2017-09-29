#include "mypilot.h"
#include "fw.h"

class ScoreDatabase : public Database
{
  private:
  
    struct ScoreEntry
    {
        UInt32 score;
        char name[1];
    };
    
  public:
  
    enum SortOrders { byName, byScore };

  private:
  
    void SetEntry(UInt16 recnum, MemHandle h, Char* name, UInt32 score) const;

  public:
  
    ScoreDatabase();
    void SortRecords(enum SortOrders order);
    Boolean GetEntry(UInt16 recnum, UInt32 &score, Char* buf, Int16 bufsiz) const;
    void AddEntry(Char* name, UInt32 score);
    void EditEntry(UInt16 recnum, Char* name, UInt32 score) const;
    void Delete(UInt16 recnum);
    virtual ~ScoreDatabase() {}
};

ScoreDatabase *db;


ScoreDatabase::ScoreDatabase()
  : Database()
{
    OpenByType('scor', dmModeReadWrite, "Scores", sizeof(enum SortOrders));
}

Int16 sortcmp(void *rec1, void *rec2, Int16 other, SortRecordInfoPtr srip1, SortRecordInfoPtr srip2, MemHandle aib)
{
    (void)other;
    (void)srip1;
    (void)srip2;
    
    ScoreDatabase::ScoreEntry *e1 = (ScoreDatabase::ScoreEntry*)rec1;
    ScoreDatabase::ScoreEntry *e2 = (ScoreDatabase::ScoreEntry*)rec2;
    // get the sort order from the app info block; inefficient, but
    // pedagogical
    MemPtr aibp = MemHandleLock(aib);
    Int16 rtn = 0;
    if (aibp)
    {
        switch(*(enum ScoreDatabase::SortOrders *)aibp)
        {
        case ScoreDatabase::byName:
	    rtn = strcmp(e1->name, e2->name);
	    break;
        case ScoreDatabase::byScore:
            rtn = (short)(e1->score - e2->score);
            break;
        default:
            break;
        }
        MemPtrUnlock(aibp);
    }
    return rtn;
}

void ScoreDatabase::SortRecords(enum SortOrders order)
{
    // Save the order persistently in the app info block
    MemPtr aibp = GetAppInfoPtr();
    if (aibp)
    {
        Write(aibp, &order, sizeof(order));
        ReleaseAppInfoPtr(aibp);
    }
    QuickSort(sortcmp);
}

Boolean ScoreDatabase::GetEntry(UInt16 recnum, UInt32 &score, Char* buf, Int16 bufsiz) const
{
    MemHandle h = QueryRecord(recnum);
    if (h)
    {
        ScoreEntry *e = (ScoreEntry*)MemHandleLock(h);
        if (e)
        {
            StrNCopy(buf, e->name, bufsiz);
            score = e->score;
            MemHandleUnlock(h);
            return True;
        }
    }
    buf[0] = 0;
    score = 0;
    return False;
}

void ScoreDatabase::SetEntry(UInt16 recnum, MemHandle h, Char* name, UInt32 score) const
{
    if (h)
    {
        MemPtr e = (char *)MemHandleLock(h);
        if (e)
        {
            Write(e, (const void *)&score, sizeof(score), 0);
            Write(e, (const void*)name, StrLen(name)+1, sizeof(score));
            MemHandleUnlock(h);
            InsertionSort(sortcmp);
        }
        // else error
        ReleaseRecord(recnum, True);
    }
    // else error
}

void ScoreDatabase::AddEntry(Char* name, UInt32 score)
{
    if (name == 0 || NumRecords()<0) return; // error
    MemHandle h = NewRecord((UInt16)NumRecords(), ((UInt16)sizeof(score))+StrLen(name)+1u);
    SetEntry((UInt16)(NumRecords()-1), h, name, score);
}

void ScoreDatabase::EditEntry(UInt16 recnum, Char* name, UInt32 score) const
{
    if (numrecords<0 || recnum >= (UInt16)numrecords || name == 0)
	return; // error
    MemHandle h = Resize(recnum, (UInt16)(sizeof(score)+StrLen(name)+1u));
    SetEntry(recnum, h, name, score);
}

void ScoreDatabase::Delete(UInt16 recnum)
{
    PurgeRecord(recnum);
}

//---------------------------------------------------------

class ScoreListSource : public RecordListSource
{
  protected:
    virtual void Format(Char* buf, UInt16 buflen, MemPtr rec) const
    {
        unsigned long s = ((ScoreDatabase::ScoreEntry*)rec)->score;
        StrPrintF(buf, "%-8lu ", s);
        StrNCopy(buf+9, ((ScoreDatabase::ScoreEntry*)rec)->name, (Int16)buflen-11);
        buf[buflen-1] = 0;
    }
  public:
    ScoreListSource();
};

ScoreListSource::ScoreListSource()
      : RecordListSource()
{}

//---------------------------------------------------------------

class ScoreList
    : public List
{
  protected:
    ScoreListSource s;

    LISTHANDLER(ScoreFormScoreList)

  public:
   
    ScoreList(Form *owner_in)
      : List(owner_in, ScoreFormScoreList, &s),
        s()
    {
    }
    void SetSource(Database *db_in)
    {
        s.SetSource(db_in);
    }
};

//---------------------------------------------------------------

class SortOrderList
    : public List
{
  public:
   
    SortOrderList(Form *owner_in)
      : List(owner_in, ScoreFormSortOrderList)
    {
    }
    virtual Boolean HandleSelect(Int16 selection);
};

Boolean SortOrderList::HandleSelect(Int16 selection)
{
    if (db)
    {
        db->SortRecords( (selection == 0) ?
            			ScoreDatabase::byName :
            			ScoreDatabase::byScore);
    }
    return List::HandleSelect(selection);
}

//---------------------------------------------------------------

class EditForm: public Form
{
  protected:
    Field namefield;
    Field scorefield;
    UInt16 recnum;

    virtual Boolean HandleOpen();
    virtual Boolean HandleSelect(UInt16 objID);

  public:
    EditForm();
    void SetRecNum(UInt16 recnum_in)
    {
        recnum = recnum_in;
    }
    virtual ~EditForm()
    { 
    }
};

EditForm::EditForm()
    : Form(EditFormForm, 2),
      namefield(this, EditFormNameField),
      scorefield(this, EditFormScoreField),
      recnum(0)
{}

Boolean EditForm::HandleOpen()
{
    if (db == 0 || recnum >= (UInt16)db->NumRecords()) // new record
    {
        namefield.Clear();
        scorefield.Clear();
    }
    else // editing existing record
    {
        UInt32 score;
        char buf[32], scr[12];
        db->GetEntry(recnum, score, buf, sizeof(buf));
        namefield.Set(buf);
        scorefield.Set(StrIToA(scr, (long)score));
    }
    SetFocus(EditFormNameField);
    return Form::HandleOpen();
}

Boolean EditForm::HandleSelect(UInt16 objID)
{
    switch (objID)
    {
    case EditFormDoneButton:
        if (db)
        {
            char name[32], scorebuf[32];
            namefield.Read(name, sizeof(name));
            scorefield.Read(scorebuf, sizeof(scorebuf));
        
            if (recnum >= (UInt16)db->NumRecords())
                db->AddEntry(name, (unsigned long)StrAToI(scorebuf));
            else
                db->EditEntry(recnum, name, (unsigned long)StrAToI(scorebuf));
        }
        // fall through
    case EditFormCancelButton:
        // switch back to the score form
        Switch(ScoreFormForm);
        break;
    default:
	return Form::HandleSelect(objID);
    }
    return True;
}

//-----------------------------------------------------------------------

class ScoreForm : public Form
{
  protected:
  
    ScoreList scores;
    SortOrderList orders;
    
    virtual Boolean HandleSelect(UInt16 objID);
    virtual Boolean HandleMenu(UInt16 menuID);
    virtual Boolean HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection);
    
  public:
  
    ScoreForm()
        : Form(ScoreFormForm, 2),
          scores(this),
          orders(this)
    { 
    }
    void SetSource(Database *db_in)
    {
        scores.SetSource(db_in);
    }
    void UpdateScores()
    {
        scores.Erase();
        PostUpdateEvent();
    }
};

Boolean ScoreForm::HandleSelect(UInt16 objID)
{
    // Get which record is selected, if any
    Int16 sel = scores.GetSelection();
    switch (objID)
    {
    case ScoreFormNewButton:
        // We are in effect going to edit the (last+1th) record
        sel = db->NumRecords();
        // fall through
    case ScoreFormEditButton:
        if (sel != noListSelection)
        {
            // Get a pointer to the edit form
            EditForm *f = (EditForm*)Application::Instance()->GetForm(EditFormForm);
            // tell the edit form which record we are editing...
            f->SetRecNum((UInt16)sel);
            // and switch to the form
            Switch(EditFormForm);
        }
        return True;
    case ScoreFormDeleteButton:
        if (sel != noListSelection)
        {
            // delete the selected item
            db->Delete((UInt16)sel);
            // update the display
            UpdateScores();
        }
        return True;
    default:
        return Form::HandleSelect(objID);
    }
}
 
Boolean ScoreForm::HandlePopupListSelect(UInt16 triggerID,
					UInt16 listID,
					Int16 selection)
{
    (void)triggerID;
    if (listID == orders.ID())
    {
        // resort the database based on the selection
        orders.HandleSelect(selection);
        // update the display
        UpdateScores();
    }
    return True; 
}


Boolean ScoreForm::HandleMenu(UInt16 menuID)
{
    if (menuID == HelpAbout)
        FrmAlert(AboutAlert);
    else return Form::HandleMenu(menuID);
    return True;
}

//------------------------------------------------------------------------

class ScoreboardApplication : public Application
{
  protected:
  
    ScoreForm scoreform;
    EditForm editform;
    
  public:
    ScoreboardApplication()
      : Application(2, ScoreFormForm),
        scoreform(),
        editform()
    {}
    virtual Boolean OpenDatabases()
    {
        // open the score database and initialise the score list source
        db = new ScoreDatabase();
        if (db)
        {
            scoreform.SetSource(db);
            return True;
        }
        return False;
    }
    virtual void CloseDatabases()
    {
        if (db)
        {
            // close the score database
            delete db;
            db = 0;
        }
    }
    virtual ~ScoreboardApplication()
    {
    }
};

//------------------------------------------------------------------------

UInt32 RunApplication(MemPtr cmdPBP, UInt16 launchflags)
{
    ScoreboardApplication *app = new ScoreboardApplication();
    UInt32 rtn = app->Main(cmdPBP, launchflags);
    delete app;
    return rtn;
}


