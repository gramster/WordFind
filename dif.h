45a46
>     Boolean Init(DmOpenRef db_in);
47a49
>         : db(0), numrecords(-1)
62a65
>     Boolean Open(UInt card, LocalID id, UInt mode = dmModeReadWrite);
158a162
>     List *activelist;
178a183,191
>     List *GetActiveList() const
>     {
>         return activelist;
>     }
>     void ActivateList(List *l)
>     {
>         activelist = l;
> 	l->Activate();
>     }
207a221
>     Form *ReturnToForm(UInt resid); // return to calling form
211d224
<     virtual List *GetList();
286a300
>     virtual void ReleaseForm(UInt formID);
