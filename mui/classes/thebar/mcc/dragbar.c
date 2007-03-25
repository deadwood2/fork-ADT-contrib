/***************************************************************************

 TheBar.mcc - Next Generation Toolbar MUI Custom Class
 Copyright (C) 2003-2005 Alfonso Ranieri
 Copyright (C) 2005-2007 by TheBar.mcc Open Source Team

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 TheBar class Support Site:  http://www.sf.net/projects/thebar

 $Id: dragbar.c 34 2007-01-20 16:08:29Z damato $

***************************************************************************/

#include "class.h"
#include "private.h"

#include <mui/muiundoc.h>

#include "SDI_hook.h"
#include "Debug.h"

/***********************************************************************/

struct data
{
    Object *bar;
    long   pshine;
    long   pshadow;
    long   pfill;
    ULONG  flags;
};

enum
{
    FLG_DB_Horiz          = 1<<0,
    FLG_DB_ShowMe         = 1<<1,
    FLG_DB_UseDragBarFill = 1<<2,
};

/***********************************************************************/

static ULONG mNew(struct IClass *cl,Object *obj,struct opSet *msg)
{
  ENTER();

  if((obj = (Object *)DoSuperNew(cl,obj,
    MUIA_InputMode,      MUIV_InputMode_Immediate,
    MUIA_ShowSelState,   FALSE,
    MUIA_CustomBackfill, TRUE,
    TAG_DONE)))
  {
    struct data *data = INST_DATA(cl,obj);

    data->bar   = (Object *)GetTagData(MUIA_TheButton_TheBar,FALSE,msg->ops_AttrList);
    data->flags = (GetTagData(MUIA_Group_Horiz,FALSE,msg->ops_AttrList) ? FLG_DB_Horiz : 0) |
                  (GetTagData(MUIA_ShowMe,TRUE,msg->ops_AttrList) ? FLG_DB_ShowMe : 0);
  }

  RETURN((ULONG)obj);
  return (ULONG)obj;
}

/***********************************************************************/

static ULONG
mGet(struct IClass *cl,Object *obj,struct opGet *msg)
{
  struct data *data = INST_DATA(cl,obj);
  BOOL result = FALSE;

  ENTER();

  switch (msg->opg_AttrID)
  {
    case MUIA_TheButton_Spacer: *msg->opg_Storage = MUIV_TheButton_Spacer_DragBar; result=TRUE; break;
    case MUIA_ShowMe:           *msg->opg_Storage = (data->flags & FLG_DB_ShowMe) ? TRUE : FALSE; result=TRUE; break;
    default:                    result=DoSuperMethodA(cl,obj,(Msg)msg);
  }

  RETURN(result);
  return result;
}

/***********************************************************************/

static ULONG
mSets(struct IClass *cl,Object *obj,struct opSet *msg)
{
    struct data    *data = INST_DATA(cl,obj);
    struct TagItem *tag;
    struct TagItem *tstate;
    ULONG result = 0;

    ENTER();

    for(tstate = msg->ops_AttrList; (tag = NextTagItem(&tstate)); )
    {
        ULONG tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case MUIA_Pressed:
                if (tidata)
                {
                    struct Window *window = (struct Window *)xget(_win(obj), MUIA_Window_Window);
                    DoMethod(_app(obj),MUIM_Application_PushMethod,(ULONG)data->bar,4,MUIM_DoDrag,window->MouseX-_mleft(obj),window->MouseY-_mtop(obj),0);
                }
                break;

            case MUIA_Group_Horiz:
                if (tidata) data->flags |= FLG_DB_Horiz;
                else data->flags &= ~FLG_DB_Horiz;
                break;

            case MUIA_ShowMe:
                if (BOOLSAME(data->flags & FLG_DB_ShowMe,tidata)) tag->ti_Data = TAG_IGNORE;
                else
                {
                    if (tidata) data->flags |= FLG_DB_ShowMe;
                    else data->flags &= ~FLG_DB_ShowMe;
                }
                break;
        }
    }

    result =  DoSuperMethodA(cl,obj,(Msg)msg);

    RETURN(result);
    return result;
}

/***********************************************************************/

static ULONG
mSetup(struct IClass *cl,Object *obj,Msg msg)
{
    struct data *data = INST_DATA(cl,obj);
    APTR                 pen;
    ULONG                *val;

    ENTER();

    if(!(DoSuperMethodA(cl,obj,(Msg)msg)))
    {
      RETURN(FALSE);
      return FALSE;
    }

    if (!getconfigitem(cl,obj,MUICFG_TheBar_DragBarShinePen,&pen))
        pen = MUIDEF_TheBar_DragBarShinePen;
    data->pshine = MUI_ObtainPen(muiRenderInfo(obj),(struct MUI_PenSpec *)pen,0);

    if (!getconfigitem(cl,obj,MUICFG_TheBar_DragBarShadowPen,&pen))
        pen = MUIDEF_TheBar_DragBarShadowPen;
    data->pshadow = MUI_ObtainPen(muiRenderInfo(obj),(struct MUI_PenSpec *)pen,0);

    if (getconfigitem(cl,obj,MUICFG_TheBar_UseDragBarFillPen,&val) ? *val : MUIDEF_TheBar_UseDragBarFillPen)
    {
        if (!getconfigitem(cl,obj,MUICFG_TheBar_DragBarFillPen,&pen))
            pen = MUIDEF_TheBar_DragBarFillPen;
        data->pfill = MUI_ObtainPen(muiRenderInfo(obj),(struct MUI_PenSpec *)pen,0);

        data->flags |= FLG_DB_UseDragBarFill;
    }

    RETURN(TRUE);
    return TRUE;
}

/***********************************************************************/

static ULONG
mCleanup(struct IClass *cl,Object *obj,Msg msg)
{
    struct data *data = INST_DATA(cl,obj);
    ULONG result = 0;

    ENTER();

    MUI_ReleasePen(muiRenderInfo(obj),data->pshine);
    MUI_ReleasePen(muiRenderInfo(obj),data->pshadow);
    if (data->flags & FLG_DB_UseDragBarFill)
    {
        MUI_ReleasePen(muiRenderInfo(obj),data->pfill);
        data->flags &= ~FLG_DB_UseDragBarFill;
    }

    result = DoSuperMethodA(cl,obj,msg);

    RETURN(result);
    return result;
}

/***********************************************************************/

static ULONG
mAskMinMax(struct IClass *cl,Object *obj,struct MUIP_AskMinMax *msg)
{
  struct data *data = INST_DATA(cl,obj);

  ENTER();

  DoSuperMethodA(cl,obj,(Msg)msg);

  if(data->flags & FLG_Horiz)
  {
    msg->MinMaxInfo->MinWidth  += 9;
    msg->MinMaxInfo->DefWidth  += 9;
    msg->MinMaxInfo->MaxWidth  += 9;
    msg->MinMaxInfo->MaxHeight  = MBQ_MUI_MAXMAX;

    #if defined(VIRTUAL)
    msg->MinMaxInfo->MinHeight += 4;
    msg->MinMaxInfo->DefHeight += 4;
    #endif
  }
  else
  {
    msg->MinMaxInfo->MinHeight += 9;
    msg->MinMaxInfo->DefHeight += 9;
    msg->MinMaxInfo->MaxWidth   = MBQ_MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight += 9;

    #if defined(VIRTUAL)
    msg->MinMaxInfo->MinWidth  += 4;
    msg->MinMaxInfo->DefWidth  += 4;
    #endif
  }

  RETURN(0);
  return 0;
}

/***********************************************************************/

static ULONG
mDraw(struct IClass *cl,Object *obj,struct MUIP_Draw *msg)
{
    struct data *data = INST_DATA(cl,obj);

    ENTER();

    DoSuperMethodA(cl,obj,(Msg)msg);

    if (msg->flags & (MADF_DRAWUPDATE|MADF_DRAWOBJECT))
    {
        struct RastPort rp;
        WORD   l, t, r, b;

        memcpy(&rp,_rp(obj),sizeof(rp));

        l = _mleft(obj);
        t = _mtop(obj);
        r = _mright(obj);
        b = _mbottom(obj);

        if (data->flags & FLG_DB_Horiz)
        {
            SetAPen(&rp,MUIPEN(data->pshine));
            Move(&rp,l,b-1);
            Draw(&rp,l,t);
            Draw(&rp,l+3,t);

            if (data->flags & FLG_DB_UseDragBarFill)
            {
                SetAPen(&rp,MUIPEN(data->pfill));
                RectFill(&rp,l+1,t+1,l+2,b-1);
            }

            SetAPen(&rp,MUIPEN(data->pshadow));
            Move(&rp,l+3,t+1);
            Draw(&rp,l+3,b);
            Draw(&rp,l,b);
        }
        else
        {
            SetAPen(&rp,MUIPEN(data->pshine));
            Move(&rp,r-1,t);
            Draw(&rp,l,t);
            Draw(&rp,l,t+3);

            if (data->flags & FLG_DB_UseDragBarFill)
            {
                SetAPen(&rp,MUIPEN(data->pfill));
                RectFill(&rp,l+1,t+1,r-1,t+2);
            }

            SetAPen(&rp,MUIPEN(data->pshadow));
            Move(&rp,l+1,t+3);
            Draw(&rp,r,t+3);
            Draw(&rp,r,t);
        }
    }

    RETURN(0);
    return 0;
}

/***********************************************************************/

static ULONG
mCustomBackfill(struct IClass *cl,Object *obj,struct MUIP_CustomBackfill *msg)
{
  struct data *data = INST_DATA(cl,obj);
  ULONG result = 0;

  ENTER();

  if(data->bar)
  {
    result = DoMethod(data->bar, MUIM_CustomBackfill, msg->left,
                                                      msg->top,
                                                      msg->right,
                                                      msg->bottom,
                                                      msg->left+msg->xoffset,
                                                      msg->top+msg->yoffset,
                                                      0);
  }
  else
  {
    result = DoSuperMethod(cl, obj, MUIM_DrawBackground, msg->left,
                                                         msg->top,
                                                         msg->right-msg->left+1,
                                                         msg->bottom-msg->top+1,
                                                         msg->xoffset,
                                                         msg->yoffset,
                                                         0);
  }

  RETURN(result);
  return result;
}

/***********************************************************************/

DISPATCHER(DragBarDispatcher)
{
  switch(msg->MethodID)
  {
    case OM_NEW:              return mNew(cl,obj,(APTR)msg);
    case OM_GET:              return mGet(cl,obj,(APTR)msg);
    case OM_SET:              return mSets(cl,obj,(APTR)msg);
    case MUIM_AskMinMax:      return mAskMinMax(cl,obj,(APTR)msg);
    case MUIM_Draw:           return mDraw(cl,obj,(APTR)msg);
    case MUIM_CustomBackfill: return mCustomBackfill(cl,obj,(APTR)msg);
    case MUIM_Setup:          return mSetup(cl,obj,(APTR)msg);
    case MUIM_Cleanup:        return mCleanup(cl,obj,(APTR)msg);
    default:                  return DoSuperMethodA(cl,obj,msg);
  }
}

/***********************************************************************/

BOOL initDragBarClass(void)
{
  BOOL result = FALSE;

  ENTER();

  if((lib_dragBarClass = MUI_CreateCustomClass(NULL, (STRPTR)MUIC_Area, NULL, sizeof(struct data), ENTRY(DragBarDispatcher))))
  {
    if(lib_flags & BASEFLG_MUI20)
      lib_dragBarClass->mcc_Class->cl_ID = (STRPTR)"TheBar_DragBar";

    result = TRUE;
  }

  RETURN(result);
  return result;
}

/***********************************************************************/
