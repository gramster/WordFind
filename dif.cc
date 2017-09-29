69c69
< Boolean Database::Open(ULong type, UInt mode, ULong creator)
---
> Boolean Database::Init(DmOpenRef db_in)
71,74c71
<     if (!Application::HideSecretRecords())
<         mode |= dmModeShowSecret;
<     db = ::DmOpenDatabaseByTypeCreator(type, creator, mode);
<     if (db)
---
>     if (db_in)
75a73
>         db = db_in;
84a83,89
> 
> Boolean Database::Open(ULong type, UInt mode, ULong creator)
> {
>     if (!Application::HideSecretRecords())
>         mode |= dmModeShowSecret;
>     return Init(::DmOpenDatabaseByTypeCreator(type, creator, mode));
> }
85a91,97
> Boolean Database::Open(UInt card, LocalID id, UInt mode)
> {
>     if (!Application::HideSecretRecords())
>         mode |= dmModeShowSecret;
>     return Init(::DmOpenDatabase(card, id, mode));
> }
> 
135c147
<     List *list = form ? form->GetList() : 0;
---
>     List *list = form ? form->GetActiveList() : 0;
185c197
< void DatabaseList::Init(class Form *owner_in, 
---
> DatabaseList::DatabaseList(class Form *owner_in, 
190a203,212
>     : List(owner_in, listid_in),
>       creator(creator_in),
>       type(type_in),
>       dbnames(0),
>       cards(0),
>       ids(0),
>       numdbs(0),
>       state(0),
>       name_out(name_out_in),
>       db(db_in)
192,201d213
<     List::Init(owner_in, listid_in);
<     creator = creator_in;
<     type = type_in;
<     dbnames = 0;
<     cards = 0;
<     ids = 0;
<     numdbs = 0;
<     state = 0;
<     name_out = name_out_in;
<     db = db_in;
302d313
<     if (GetList()) GetList()->Init();
438,439c449,450
< 	if (GetList())
< 	    return GetList()->Activate();
---
> 	if (GetActiveList())
> 	    return GetActiveList()->Activate();
451a463,474
> Form *Form::ReturnToForm(UInt resid) // return to calling form
> {
>     Form *f = Application::Instance()->GetForm(resid);
>     if (f)
>     {
>         Application::Instance()->SetActiveForm(f);
>         Application::Instance()->ReleaseForm(id);
>         FrmReturnToForm(resid);
>     }
>     return f;
> }
> 
559,563d581
< List *Form::GetList()
< {
<     return 0;
< }
< 
680a699,704
> void Application::ReleaseForm(UInt formID)
> {
>     (void)formID;
>     return 0;
> }
> 
695d718
< 
