
#include "class.h"
#include <utility/pack.h>
#include <datatypes/textclass.h>
#include <dos/dostags.h>
#include <libraries/gadtools.h>
#include <graphics/rpattr.h>

/****************************************************************************/

struct data
{
    char                        url[256];
    char                        text[256];
    int                         tLen;
    struct MUI_RenderInfo       *mri;
    struct TextExtent           te;
    Object                      *menu;
    struct TextFont             *tf;
    ULONG                       flags;
    LONG                        mouseOutPen;
    LONG                        mouseOverPen;
    LONG                        visitedPen;
    LONG                        textpen;
    struct MUI_EventHandlerNode he;
};

enum
{
    UTFLG_Show              = 1<<0,
    UTFLG_MouseOver         = 1<<1,
    UTFLG_Active            = 1<<2,
    UTFLG_Visited           = 1<<3,
    UTFLG_DoVisitedPen      = 1<<4,
    UTFLG_Fallback          = 1<<5,
    UTFLG_Underline         = 1<<6,
    UTFLG_SetMax            = 1<<7,
    UTFLG_DoOpenURL         = 1<<8,
    UTFLG_UserDoVisitedPen  = 1<<9,
    UTFLG_UserFallback      = 1<<10,
    UTFLG_UserUnderline     = 1<<11,
    UTFLG_UserSetMax        = 1<<12,
    UTFLG_UserDoOpenURL     = 1<<13,
    UTFLG_FreeMouseOutPen   = 1<<14,
    UTFLG_FreeMouseOverPen  = 1<<15,
    UTFLG_FreeVisitedPen    = 1<<16,
    UTFLG_Input             = 1<<17,
    UTFLG_Disabled          = 1<<18,
    UTFLG_NoMenu            = 1<<19,
    UTFLG_UseFont           = 1<<20,
    UTFLG_CloseFont         = 1<<21,
    UTFLG_InVirtualGroup    = 1<<22,
    UTFLG_ActiveObject      = 1<<23,
};

#define UTFLG_Use               (UTFLG_Active|UTFLG_Fallback)
#define UTFLG_IsToShow(f)       (((ULONG)(f)) & UTFLG_Show)
#define UTFLG_IsDisabled(f)     (((ULONG)(f)) & UTFLG_Disabled)
#define UTFLG_IsToUse(f)        (((ULONG)(f)) & UTFLG_Use)
#define UTFLG_IsMouseOver(f)    (((ULONG)(f)) & UTFLG_MouseOver)
#define UTFLG_IsVisited(f)      ((((ULONG)(f)) & UTFLG_DoVisitedPen) && (((ULONG)(f)) & UTFLG_Visited))
#define UTFLG_IsUnderline(f)    (((ULONG)(f)) & UTFLG_Underline)
#define UTFLG_IsSetMax(f)       (((ULONG)(f)) & UTFLG_SetMax)
#define UTFLG_IsDoOpenURL(f)    (((ULONG)(f)) & UTFLG_DoOpenURL)

/***********************************************************************/

struct urltextPackTags
{
    STRPTR  url;
    STRPTR  text;
    BOOL    doVisitedPen;
    BOOL    fallBack;
    BOOL    underline;
    ULONG   flags;
};

ULONG utTable[] =
{
    PACK_STARTTABLE(MUIA_Urltext_Base),

    PACK_ENTRY(MUIA_Urltext_Base,MUIA_Urltext_Url,urltextPackTags,url,PKCTRL_LONG|PKCTRL_PACKONLY),
    PACK_ENTRY(MUIA_Urltext_Base,MUIA_Urltext_Text,urltextPackTags,text,PKCTRL_LONG|PKCTRL_PACKONLY),

    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_DoVisitedPen,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_DoVisitedPen),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_FallBack,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_Fallback),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_Underline,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_Underline),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_SetMax,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_SetMax),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_DoOpenURL,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_DoOpenURL),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_NoMenu,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY,UTFLG_NoMenu),

    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_DoVisitedPen,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_UserDoVisitedPen),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_FallBack,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_UserFallback),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_Underline,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_UserUnderline),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_SetMax,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_UserSetMax),
    PACK_LONGBIT(MUIA_Urltext_Base,MUIA_Urltext_DoOpenURL,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_UserDoOpenURL),

    PACK_NEWOFFSET(MUIA_ContextMenu),
    PACK_LONGBIT(MUIA_ContextMenu,MUIA_ContextMenu,urltextPackTags,flags,PKCTRL_BIT|PKCTRL_PACKONLY|PSTF_EXISTS,UTFLG_NoMenu),

    PACK_ENDTABLE
};

static ULONG ASM
mNew(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct opSet *msg)
{
    struct urltextPackTags  ut = {0};
    register struct TagItem *attrs = msg->ops_AttrList,
                            tags[] = {MUIA_InnerBottom,0,
                                      MUIA_InnerLeft,0,
                                      MUIA_InnerRight,0,
                                      MUIA_InnerTop,0,
                                      MUIA_Weight,0,
                                      MUIA_InputMode,MUIV_InputMode_Immediate,
                                      MUIA_ShowSelState,FALSE,
                                      TAG_MORE,0};

    PackStructureTags(&ut,utTable,attrs);

    if (!ut.url) return 0;

    tags[7].ti_Data = (ULONG)attrs;
    msg->ops_AttrList = tags;

    if (obj = (Object *)DoSuperMethodA(cl,obj,(APTR)msg))
    {
        register struct data    *data = INST_DATA(cl,obj);
        register ULONG          flags = ut.flags;

        stccpy(data->url,ut.url,sizeof(data->url));

        stccpy(data->text,ut.text ? ut.text : ut.url,sizeof(data->text));
        data->tLen = strlen(data->text);

        data->mri          = NULL;
        data->mouseOutPen  = -1;
        data->mouseOverPen = -1;
        data->visitedPen   = -1;
        data->textpen      = -1;
        data->menu         = NULL;
        data->tf           = NULL;

        if (!(flags & UTFLG_UserDoVisitedPen))
            if (DEFAULT_DOVISITEDPEN) flags |= UTFLG_DoVisitedPen;
            else flags &= ~UTFLG_DoVisitedPen;

        if (!(flags & UTFLG_UserFallback))
            if (DEFAULT_FALLBACK) flags |= UTFLG_Fallback;
            else flags &= ~UTFLG_Fallback;

        if (!(flags & UTFLG_UserUnderline))
            if (DEFAULT_UNDERLINE) flags |= UTFLG_Underline;
            else flags &= ~UTFLG_Underline;

        if (!(flags & UTFLG_UserSetMax))
            if (DEFAULT_SETMAX) flags |= UTFLG_SetMax;
            else flags &= ~UTFLG_SetMax;

        if (!(flags & UTFLG_UserDoOpenURL))
            if (DEFAULT_SETMAX) flags |= UTFLG_DoOpenURL;
            else flags &= ~UTFLG_DoOpenURL;

        if (OpenURLBase)
        {
            flags |= UTFLG_Active;
            if (UTFLG_IsDoOpenURL(flags))
                DoMethod(obj,MUIM_Notify,MUIA_Pressed,1,obj,2,MUIM_Urltext_OpenURL,MUIV_Urltext_OpenURL_CheckOver);
        }

        if (!(flags & UTFLG_NoMenu))
        {
            register Object *menu;
            register BPTR   lock;

            if (lock = Lock("SYS:Prefs/OpenURL",SHARED_LOCK)) UnLock(lock);

            if (menu = MenustripObject,
                MUIA_Family_Child, MenuObjectT("Urltext"),
                    MUIA_Family_Child, MenuitemObject,
                        MUIA_Menuitem_Title, getString(Msg_Send),
                        MUIA_Menuitem_Enabled, OpenURLBase,
                        MUIA_UserData, Msg_Send,
                    End,
                    MUIA_Family_Child, MenuitemObject,
                        MUIA_Menuitem_Title, getString(Msg_Copy),
                        MUIA_UserData, Msg_Copy,
                    End,
                    MUIA_Family_Child, MenuitemObject,
                        MUIA_Menuitem_Title, NM_BARLABEL,
                    End,
                    MUIA_Family_Child, MenuitemObject,
                        MUIA_Menuitem_Title, getString(Msg_OpenURLPrefs),
                        MUIA_Menuitem_Enabled, lock,
                        MUIA_UserData, Msg_OpenURLPrefs,
                    End,
                End,
            End) set(obj,MUIA_ContextMenu,data->menu = menu);
        }

        data->flags = flags;
    }

    msg->ops_AttrList = attrs;

    return (ULONG)obj;
}

/***********************************************************************/

static ASM ULONG
mDispose(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    register Object         *menu = data->menu;
    register ULONG          res;

    res = DoSuperMethodA(cl,obj,msg);

    if (menu) MUI_DisposeObject(menu);

    return res;
}

/***********************************************************************/

static ASM ULONG
mGet(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct opGet *msg)
{
    register struct data *data = INST_DATA(cl,obj);

    switch(msg->opg_AttrID)
    {
        case MUIA_Urltext_Url:      *msg->opg_Storage = (ULONG)data->url; return TRUE;
        case MUIA_Urltext_Text:     *msg->opg_Storage = (ULONG)data->text; return TRUE;
        case MUIA_Urltext_Active:   *msg->opg_Storage = (data->flags & UTFLG_Active) ? TRUE : FALSE; return TRUE;
        case MUIA_Urltext_Visited:  *msg->opg_Storage = (data->flags & UTFLG_Visited) ? TRUE : FALSE; return TRUE;
        case MUIA_Version:          *msg->opg_Storage = VERSION; return TRUE;
        case MUIA_Revision:         *msg->opg_Storage = REVISION; return TRUE;
        case MUIA_Urltext_Version:  *msg->opg_Storage = (ULONG)VERSTAG+7;
        default: return DoSuperMethodA(cl,obj,(APTR)msg);
    }
}

/***********************************************************************/

#define BOOLSAME(a,b) (((BOOL)(a) && (BOOL)(b)) || ((!(BOOL)(a)) && (!(BOOL)(b))))

static ULONG ASM
mSets(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct opSet *msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    register struct TagItem *tag;
    struct TagItem          *tstate;
    register ULONG          flags, redraw, notifyUrl, over;

    flags = data->flags;
    redraw = notifyUrl = over = 0;

    for (tstate = msg->ops_AttrList; tag = NextTagItem(&tstate); )
    {
        register ULONG tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case MUIA_Urltext_MouseOver:
                if (!UTFLG_IsToUse(flags) || BOOLSAME(tidata,UTFLG_IsMouseOver(flags)))
                    tag->ti_Tag = TAG_IGNORE;
                else
                {
                    if (tidata)
                    {
                        flags |= UTFLG_MouseOver;
                        over = 1;
                    }
                    else flags &= ~UTFLG_MouseOver;
                    redraw = notifyUrl = 1;
                }
                break;

            case MUIA_Urltext_Visited:
                if (BOOLSAME(tidata,flags & UTFLG_Visited))
                    tag->ti_Tag = TAG_IGNORE;
                else
                {
                    if (tidata) flags |= UTFLG_Visited;
                    else flags &= ~UTFLG_Visited;
                    redraw = 1;
                }
                break;

            case MUIA_Disabled:
                if (BOOLSAME(tidata,UTFLG_IsDisabled(flags)))
                    tag->ti_Tag = TAG_IGNORE;
                else
                {
                    if (tidata) flags |= UTFLG_Disabled;
                    else flags &= ~UTFLG_Disabled;
                    redraw = 1;
                }
                break;

            case MUIA_Urltext_MouseOutPen:
                if (data->mri)
                {
                    register LONG pen;

                    if ((pen = MUI_ObtainPen(data->mri,(struct MUI_PenSpec *)tidata,0))!=-1)
                    {
                        if (data->flags & UTFLG_FreeMouseOutPen) MUI_ReleasePen(data->mri,data->mouseOutPen);
                        data->mouseOutPen = pen;
                        data->flags |= UTFLG_FreeMouseOutPen;
                        redraw = 1;
                    }
                }
                break;

            case MUIA_Urltext_MouseOverPen:
                if (data->mri)
                {
                    register LONG pen;

                    if ((pen = MUI_ObtainPen(data->mri,(struct MUI_PenSpec *)tidata,0))!=-1)
                    {
                        if (data->flags & UTFLG_FreeMouseOverPen) MUI_ReleasePen(data->mri,data->mouseOverPen);
                        data->mouseOverPen = pen;
                        data->flags |= UTFLG_FreeMouseOverPen;
                        redraw = 1;
                    }
                }
                break;

            case MUIA_Urltext_VisitedPen:
                if (data->mri)
                {
                    register LONG pen;

                    if ((pen = MUI_ObtainPen(data->mri,(struct MUI_PenSpec *)tidata,0))!=-1)
                    {
                        if (data->flags & UTFLG_FreeVisitedPen) MUI_ReleasePen(data->mri,data->visitedPen);
                        data->flags |= UTFLG_FreeVisitedPen;
                        data->visitedPen = pen;
                        redraw = 1;
                    }
                }
                break;

            case MUIA_Urltext_PUnderline:
                if (tidata) flags |= UTFLG_Underline;
                else flags &= ~UTFLG_Underline;
                redraw = 1;
                break;

            case MUIA_Urltext_PDoVisitedPen:
                if (tidata) flags |= UTFLG_DoVisitedPen;
                else flags &= ~UTFLG_DoVisitedPen;
                redraw = 1;
                break;

            case MUIA_Urltext_PFallBack:
                if (tidata) flags |= UTFLG_Fallback;
                else flags &= ~UTFLG_Fallback;
                redraw = 1;
                break;
        }
    }

    data->flags = flags;

    if (redraw)
    {
        MUI_Redraw(obj,MADF_DRAWOBJECT);
        if (notifyUrl) SetSuperAttrs(cl,obj,MUIA_Urltext_Url,over ? data->url : NULL,TAG_DONE);
    }

    return DoSuperMethodA(cl,obj,(APTR)msg);
}


/***********************************************************************/

static ASM ULONG
mSetup(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_Setup *msg)
{
    register struct data        *data = INST_DATA(cl,obj);
    register struct TextFont    *df;
    Object                      *parent;
    register ULONG              flags = data->flags, v;
    ULONG                       p;

    if (!DoSuperMethodA(cl,obj,(APTR)msg)) return FALSE;

    data->mri          = msg->RenderInfo;
    data->mouseOutPen  = _pens(obj)[DEFAULT_MOUSEOUT_PEN];
    data->mouseOverPen = _pens(obj)[DEFAULT_MOUSEOVER_PEN];
    data->visitedPen   = _pens(obj)[DEFAULT_VISITED_PEN];
    data->textpen      = _pens(obj)[MPEN_TEXT];

    if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_MouseOutPen,&p))
        set(obj,MUIA_Urltext_MouseOutPen,p);

    if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_MouseOverPen,&p))
        set(obj,MUIA_Urltext_MouseOverPen,p);

    if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_VisitedPen,&p))
        set(obj,MUIA_Urltext_VisitedPen,p);

    if (!(flags & UTFLG_UserDoVisitedPen))
    {
        if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_DoVisitedPen,&p)) v = *(ULONG *)p;
        else v = DEFAULT_DOVISITEDPEN;

        if (v) data->flags |= UTFLG_DoVisitedPen;
        else data->flags &= ~UTFLG_DoVisitedPen;
    }

    if (!(flags & UTFLG_UserFallback))
    {
        if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_FallBack,&p)) v = *(ULONG *)p;
        else v = DEFAULT_FALLBACK;

        if (v) data->flags |= UTFLG_Fallback;
        else data->flags &= ~UTFLG_Fallback;
    }

    if (!(flags & UTFLG_UserUnderline))
    {
        if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_Underline,&p)) v = *(ULONG *)p;
        else v = DEFAULT_UNDERLINE;

        if (v) data->flags |= UTFLG_Underline;
        else data->flags &= ~UTFLG_Underline;
    }

    df = _font(obj);
    if (DoMethod(obj,MUIM_GetConfigItem,MUIA_Urltext_Font,&p))
    {
        register char   buf[256], *t, *s;
        struct TextAttr ta;
        long            ys;

        strcpy(buf,(STRPTR)p);

        if (t = strchr(buf,'/'))
        {
            *t++ = 0;
            if (!stcd_l(t,&ys) || ys<=0) ys = 8;
        }
        else ys = 8;

        for (s = NULL, t = buf; *t; t++) if (*t=='.') s = t;
        if (!s || stricmp(++s,"font")) strcat(buf,".font");

        ta.ta_Name  = buf;
        ta.ta_YSize = ys;
        ta.ta_Style = 0;
        ta.ta_Flags = 0;

        if (data->tf = OpenDiskFont(&ta)) data->flags |= UTFLG_CloseFont;
        else data->tf = df;
    }
    else data->tf = df;

    if (UTFLG_IsToUse(data->flags))
    {
        data->he.ehn_Class  = cl;
        data->he.ehn_Object = obj;
        data->he.ehn_Events = IDCMP_MOUSEMOVE;

        DoMethod(_win(obj),MUIM_Window_AddEventHandler,&data->he);
        data->flags |= UTFLG_Input;
    }

    parent = obj;
    v = FALSE;
    while (1)
    {
        get(parent,MUIA_Parent,&parent);
        if (!parent) break;

        if (get(parent,MUIA_Virtgroup_Top,&p))
        {
            v = TRUE;
            break;
        }
    }
    if (v) data->flags |= UTFLG_InVirtualGroup;

    return TRUE;
}

/***********************************************************************/

static ASM ULONG
mCleanup(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_Setup *msg)
{
    register struct data *data = INST_DATA(cl,obj);

    if (data->flags & UTFLG_Input) DoMethod(_win(obj),MUIM_Window_RemEventHandler,&data->he);
    if (data->flags & UTFLG_CloseFont) CloseFont(data->tf);
    if (data->flags & UTFLG_FreeMouseOutPen) MUI_ReleasePen(data->mri,data->mouseOutPen);
    if (data->flags & UTFLG_FreeMouseOverPen) MUI_ReleasePen(data->mri,data->mouseOverPen);
    if (data->flags & UTFLG_FreeVisitedPen) MUI_ReleasePen(data->mri,data->visitedPen);

    data->mri = NULL;
    data->flags &= ~(UTFLG_Input | UTFLG_CloseFont | UTFLG_FreeMouseOutPen | UTFLG_FreeMouseOverPen | UTFLG_FreeVisitedPen);

    return DoSuperMethodA(cl,obj,(APTR)msg);
}

/***********************************************************************/

static ASM ULONG
mAskMinMax(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_AskMinMax *msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    struct RastPort         rp;
    register WORD           w, h;

    DoSuperMethodA(cl,obj,(APTR)msg);

    InitRastPort(&rp);
    SetFont(&rp,data->tf);
    TextExtent(&rp,data->text,data->tLen,&data->te);

    w = data->te.te_Width;
    h = data->te.te_Height;

    msg->MinMaxInfo->MinWidth  += w;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefWidth  += w;
    msg->MinMaxInfo->DefHeight += h;

    if (UTFLG_IsSetMax(data->flags))
    {
        msg->MinMaxInfo->MaxWidth  += w;
        msg->MinMaxInfo->MaxHeight += h;
    }
    else
    {
        msg->MinMaxInfo->MaxWidth  += MBQ_MUI_MAXMAX;
        msg->MinMaxInfo->MaxHeight += MBQ_MUI_MAXMAX;
    }

    return 0;
}

/***********************************************************************/

static ASM ULONG
mShow(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    register ULONG          res;

    if (res = DoSuperMethodA(cl,obj,msg)) data->flags |= UTFLG_Show;

    return res;
}

/***********************************************************************/

static ASM ULONG
mHide(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register struct data *data = INST_DATA(cl,obj);

    data->flags &= ~UTFLG_Show;

    return DoSuperMethodA(cl,obj,msg);
}

/***********************************************************************/

static ASM ULONG
mDraw(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_Draw *msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    register ULONG          flags = data->flags;

    DoSuperMethodA(cl,obj,(APTR)msg);

    if (UTFLG_IsToShow(flags) && (msg->flags & MADF_DRAWOBJECT))
    {
        register struct RastPort    *rp = _rp(obj);
        struct TextFont             *tf;
        register SHORT              l = _mtop(obj)-data->te.te_Extent.MinY;

        SetAPen(rp,
            (UTFLG_IsToUse(flags) && !UTFLG_IsDisabled(flags)) ?
                (UTFLG_IsMouseOver(flags) ?
                    MUIPEN(data->mouseOverPen) :
                    (UTFLG_IsVisited(flags) ?
                        MUIPEN(data->visitedPen) :
                        MUIPEN(data->mouseOutPen))) :
                data->textpen);

        GetRPAttrs(rp,RPTAG_Font,&tf,TAG_DONE);
        SetRPAttrs(rp,RPTAG_Font,data->tf,TAG_DONE);

        Move(rp,_mleft(obj),l);
        Text(rp,data->text,data->tLen);
        SetRPAttrs(rp,RPTAG_Font,tf,TAG_DONE);

        if (UTFLG_IsUnderline(flags))
        {
            Move(rp,_mleft(obj),++l);
            Draw(rp,_mleft(obj)+data->te.te_Extent.MaxX,l);
        }
    }

    return 0;
}

/***********************************************************************/

#define _between(a,x,b)             ((x)>=(a) && (x)<=(b))
#define _isinobject(obj,x,y)        (_between(_mleft(obj),(x),_mright(obj)) && _between(_mtop(obj),(y),_mbottom(obj)))
#define _isinobjectr(obj,r,x,y)     (_between(_mleft(obj),(x),(r)) && _between(_mtop(obj),(y),_mbottom(obj)))

static ASM ULONG
mHandleEvent(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_HandleEvent *msg)
{
    if (msg->imsg && msg->imsg->Class==IDCMP_MOUSEMOVE)
    {
        register struct data    *data = INST_DATA(cl,obj);
        register ULONG          over;
        register WORD           r, x = msg->imsg->MouseX, y = msg->imsg->MouseY;

        r = UTFLG_IsSetMax(data->flags) ? _mright(obj) : _mleft(obj)+data->te.te_Width;

        if ((over = _isinobjectr(obj,r,x,y)) && (data->flags & UTFLG_InVirtualGroup))
        {
            Object *p = obj;

            while (1)
            {
                get(p,MUIA_Parent,&p);
                if (!p) break;

                if (!_isinobject(p,x,y))
                {
                    over = FALSE;
                    break;
                }
            }
        }

        set(obj,MUIA_Urltext_MouseOver,over ? TRUE : FALSE);
    }

    return 0;
}

/***********************************************************************/

static ASM ULONG
mOpenURL(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_Urltext_OpenURL *msg)
{
    register struct data    *data = INST_DATA(cl,obj);
    register STRPTR         url = data->url;
    register ULONG          flags = data->flags, res = FALSE;

    if (url && *url && OpenURLBase && !UTFLG_IsDisabled(flags) &&
        ((msg->flags & MUIV_Urltext_OpenURL_CheckOver) ? (data->flags & (UTFLG_MouseOver|UTFLG_ActiveObject)) : 1))
    {
        if (URL_Open(url,URL_NewWindow,TRUE,TAG_DONE))
            set(obj,MUIA_Urltext_Visited,res = TRUE);
    }

    return res;
}

/***********************************************************************/

static ASM ULONG
mCopy(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_Urltext_Copy *msg)
{
    register struct data        *data = INST_DATA(cl,obj);
    register struct IFFHandle   *iff;
    register STRPTR             url = data->url;
    register ULONG              unit = msg->unit, res = 0, len;

    if (iff = AllocIFF())
    {
        if (iff->iff_Stream = (LONG)OpenClipboard(unit))
        {
            InitIFFasClip(iff);

            if (!OpenIFF(iff,IFFF_WRITE))
            {
                res = !PushChunk(iff,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN) &&
                      !PushChunk(iff,ID_FTXT,ID_CHRS,len = strlen(url)) &&
                      (WriteChunkBytes(iff,url,len)>=0) &&
                      !PopChunk(iff);

                CloseIFF(iff);
            }

            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }

        FreeIFF(iff);
    }

    return res;
}

/***********************************************************************/

static ASM ULONG
mOpenURLPrefs(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register BPTR in, out;

    if ((in  = Open("NIL:",MODE_OLDFILE)) &&
        (out = Open("NIL:",MODE_OLDFILE)))
    {
        SystemTags("SYS:Prefs/OpenURL",
                    SYS_Asynch, TRUE,
                    SYS_Input, in,
                    SYS_Output, out,
                    NP_StackSize, 8192,
                    TAG_DONE);
        return TRUE;
    }

    if (in) Close(in);

    return FALSE;
}

/***********************************************************************/

static ULONG SAVEDS ASM
mContextMenuChoice(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) struct MUIP_ContextMenuChoice *msg)
{
    switch (muiUserData(msg->item))
    {
        case Msg_Send:
            DoMethod(obj,MUIM_Urltext_OpenURL,0);
            break;

        case Msg_Copy:
            DoMethod(obj,MUIM_Urltext_Copy,0);
            break;

        case Msg_OpenURLPrefs:
            DoMethod(obj,MUIM_Urltext_OpenURLPrefs);
            break;
    }

    return 0;
}

/***********************************************************************/

static ULONG SAVEDS ASM
mActive(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register struct data *data = INST_DATA(cl,obj);

    data->flags |= UTFLG_ActiveObject;

    return DoSuperMethodA(cl,obj,msg);
}

/***********************************************************************/

static ULONG SAVEDS ASM
mInactive(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    register struct data *data = INST_DATA(cl,obj);

    data->flags &= ~UTFLG_ActiveObject;

    return DoSuperMethodA(cl,obj,msg);
}

/***********************************************************************/

static SAVEDS ASM ULONG
dispatcher(REG(a0) struct IClass *cl,REG(a2) Object *obj,REG(a1) Msg msg)
{
    switch(msg->MethodID)
    {
        case MUIM_HandleEvent:          return mHandleEvent(cl,obj,(APTR)msg);
        case MUIM_Draw:                 return mDraw(cl,obj,(APTR)msg);
        case OM_SET:                    return mSets(cl,obj,(APTR)msg);
        case MUIM_Setup:                return mSetup(cl,obj,(APTR)msg);
        case MUIM_Cleanup:              return mCleanup(cl,obj,(APTR)msg);
        case MUIM_AskMinMax:            return mAskMinMax(cl,obj,(APTR)msg);
        case MUIM_Show:                 return mShow(cl,obj,(APTR)msg);
        case MUIM_Hide:                 return mHide(cl,obj,(APTR)msg);
        case OM_NEW:                    return mNew(cl,obj,(APTR)msg);
        case OM_DISPOSE:                return mDispose(cl,obj,(APTR)msg);
        case OM_GET:                    return mGet(cl,obj,(APTR)msg);
        case MUIM_ContextMenuChoice:    return mContextMenuChoice(cl,obj,(APTR)msg);
        case MUIM_Urltext_OpenURL:      return mOpenURL(cl,obj,(APTR)msg);
        case MUIM_Urltext_Copy:         return mCopy(cl,obj,(APTR)msg);
        case MUIM_Urltext_OpenURLPrefs: return mOpenURLPrefs(cl,obj,(APTR)msg);
        case MUIM_GoActive:             return mActive(cl,obj,(APTR)msg);
        case MUIM_GoInactive:           return mInactive(cl,obj,(APTR)msg);
        default:                        return DoSuperMethodA(cl,obj,msg);
    }
}

/***********************************************************************/

BOOL ASM
initMCC(REG(a0) struct UrltextBase *base)
{
    if (base->mcc = MUI_CreateCustomClass((struct Library *)base,MUIC_Area,NULL,sizeof(struct data),dispatcher))
    {
        if (MUIMasterBase->lib_Version>=20)
            base->mcc->mcc_Class->cl_ID = PRG;

        return TRUE;
    }

    return FALSE;
}

/***********************************************************************/