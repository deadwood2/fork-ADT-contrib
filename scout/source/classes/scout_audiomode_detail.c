/**
 * Scout - The Amiga System Monitor
 *
 *------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * You must not use this source code to gain profit of any kind!
 *
 *------------------------------------------------------------------
 *
 * @author Andreas Gelhausen
 * @author Richard K�rber <rkoerber@gmx.de>
 */

#include "system_headers.h"

struct AudioModesDetailWinData {
    TEXT amdwd_Title[WINDOW_TITLE_LENGTH];
    APTR amdwd_Texts[30];
    struct AudioModeEntry *amdwd_AudioMode;
};

#ifdef __MORPHOS__
extern struct Library *AHIBase;
#endif

STATIC void SetDetails( struct IClass *cl,
                        Object *obj )
{
    struct AudioModesDetailWinData *amdwd = INST_DATA(cl, obj);
    struct AudioModeEntry *ame = amdwd->amdwd_AudioMode;
    STRPTR tmp;

    if ((tmp = tbAllocVecPooled(globalPool, PATH_LENGTH)) != NULL) {
        struct MsgPort *mp;

        if ((mp = CreateMsgPort()) != NULL) {
            struct AHIRequest *ahir;

            if ((ahir = (struct AHIRequest *)CreateIORequest(mp, sizeof(struct AHIRequest))) != NULL) {
                ahir->ahir_Version = 4;

                if (OpenDevice(AHINAME, 0, (struct IORequest *)ahir, 0) == 0) {
                #ifndef __MORPHOS__
                    struct Device *AHIBase;
                #if defined(__amigaos4__)
                    struct AHIIFace *IAHI;
                #endif
                #endif
                    ULONG bits, maxchannels, minfreq, maxfreq, numfreqs, maxplaysamp, maxrecsamp;
                    Fixed minmonivol, maxmonivol;
                    Fixed mininputgain, maxinputgain;
                    Fixed minoutputvol, maxoutputvol;
                    ULONG volume, stereo, panning, hifi, pingpong, record, fullduplex, realtime;

                    AHIBase = ahir->ahir_Std.io_Device;

                    if (GETINTERFACE(IAHI, AHIBase)) {
                        MySetContents(amdwd->amdwd_Texts[ 0], ame->ame_Name);
                        MySetContents(amdwd->amdwd_Texts[ 1], ame->ame_Id);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Driver, tmp, AHIDB_BufferLen, PATH_LENGTH, TAG_DONE);
                        MySetContents(amdwd->amdwd_Texts[ 2], "DEVS:AHI/%s.audio", tmp);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Version, tmp, AHIDB_BufferLen, PATH_LENGTH, TAG_DONE);
                        MySetContentsHealed(amdwd->amdwd_Texts[ 3], "%s", tmp);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Author, tmp, AHIDB_BufferLen, PATH_LENGTH, TAG_DONE);
                        MySetContentsHealed(amdwd->amdwd_Texts[ 4], "%s", tmp);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Copyright, tmp, AHIDB_BufferLen, PATH_LENGTH, TAG_DONE);
                        MySetContentsHealed(amdwd->amdwd_Texts[ 5], "%s", tmp);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Annotation, tmp, AHIDB_BufferLen, PATH_LENGTH, TAG_DONE);
                        MySetContentsHealed(amdwd->amdwd_Texts[ 6], "%s", tmp);

                        AHI_GetAudioAttrs(ame->ame_ModeID, NULL, AHIDB_Bits, &bits,
                                                                 AHIDB_MaxChannels, &maxchannels,
                                                                 AHIDB_MinMixFreq, &minfreq,
                                                                 AHIDB_MaxMixFreq, &maxfreq,
                                                                 AHIDB_MaxMixFreq, &maxfreq,
                                                                 AHIDB_Frequencies, &numfreqs,
                                                                 AHIDB_MaxPlaySamples, &maxplaysamp,
                                                                 AHIDB_MaxRecordSamples, &maxrecsamp,
                                                                 AHIDB_Volume, &volume,
                                                                 AHIDB_Stereo, &stereo,
                                                                 AHIDB_Panning, &panning,
                                                                 AHIDB_HiFi, &hifi,
                                                                 AHIDB_PingPong, &pingpong,
                                                                 AHIDB_Record, &record,
                                                                 AHIDB_FullDuplex, &fullduplex,
                                                                 AHIDB_Realtime, &realtime,
                                                                 AHIDB_MinMonitorVolume, &minmonivol,
                                                                 AHIDB_MaxMonitorVolume, &maxmonivol,
                                                                 AHIDB_MinInputGain, &mininputgain,
                                                                 AHIDB_MaxInputGain, &maxinputgain,
                                                                 AHIDB_MinOutputVolume, &minoutputvol,
                                                                 AHIDB_MaxOutputVolume, &maxoutputvol,
                                                                 TAG_DONE);

                        MySetContents(amdwd->amdwd_Texts[ 7], "%lD", bits);
                        MySetContents(amdwd->amdwd_Texts[ 8], "%lD", maxchannels);
                        if (numfreqs > 1) {
                            MySetContents(amdwd->amdwd_Texts[ 9], "%6lD Hz..%6lD Hz", minfreq, maxfreq);
                        } else {
                            MySetContents(amdwd->amdwd_Texts[ 9], "---");
                        }
                        MySetContents(amdwd->amdwd_Texts[10], "%lD", maxplaysamp);
                        MySetContents(amdwd->amdwd_Texts[11], "%lD", maxrecsamp);

                        if (minmonivol == maxmonivol && minmonivol == 0) {
                            MySetContents(amdwd->amdwd_Texts[12], txtNone);
                        } else {
                            MySetContents(amdwd->amdwd_Texts[12], "%ld%%..%ld%%", minmonivol * 100 >> 16, maxmonivol * 100 >> 16);
                        }
                        if (mininputgain == maxinputgain) {
                            MySetContents(amdwd->amdwd_Texts[13], txtNone);
                        } else {
                            MySetContents(amdwd->amdwd_Texts[13], "%ld%%..%ld%%", mininputgain * 100 >> 16, maxinputgain * 100 >> 16);
                        }
                        if (minoutputvol == maxoutputvol) {
                            MySetContents(amdwd->amdwd_Texts[14], txtNone);
                        } else {
                            MySetContents(amdwd->amdwd_Texts[14], "%ld%%..%ld%%", minoutputvol * 100 >> 16, maxoutputvol * 100 >> 16);
                        }

                        MySetContents(amdwd->amdwd_Texts[ 15], "%s", (volume) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 16], "%s", (stereo) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 17], "%s", (panning) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 18], "%s", (hifi) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 19], "%s", (pingpong) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 20], "%s", (record) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 21], "%s", (fullduplex) ? msgYes : msgNo);
                        MySetContents(amdwd->amdwd_Texts[ 22], "%s", (realtime) ? msgYes : msgNo);

                        DROPINTERFACE(IAHI);
                    }

                    CloseDevice((struct IORequest *)ahir);
                } else {
                    MyRequest(msgErrorContinue, msgCantOpenAHIDevice, AHINAME, ahir->ahir_Version);
                }

                DeleteIORequest((struct IORequest *)ahir);
            }

            DeleteMsgPort(mp);
        }

        tbFreeVecPooled(globalPool, tmp);
    }

    set(obj, MUIA_Window_Title, MyGetChildWindowTitle(txtAudioModesDetailTitle, ame->ame_Name, amdwd->amdwd_Title, sizeof(amdwd->amdwd_Title)));
}

STATIC ULONG mNew( struct IClass *cl,
                   Object *obj,
                   struct opSet *msg )
{
    APTR group, texts[30], exitButton;

    if ((obj = (Object *)DoSuperNew(cl, obj,
        MUIA_HelpNode, "AudioModes",
        MUIA_Window_ID, MakeID('.','A','M','D'),
        WindowContents, VGroup,

            Child, group = ScrollgroupObject,
                MUIA_CycleChain, TRUE,
                MUIA_Scrollgroup_FreeHoriz, FALSE,
                MUIA_Scrollgroup_Contents, VGroupV,
                    MUIA_Background, MUII_GroupBack,
                    Child, VGroup,
                        GroupFrame,
                        Child, ColGroup(2),
                            Child, MyLabel2(txtName2),
                            Child, texts[ 0] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeID2),
                            Child, texts[ 1] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeDriver),
                            Child, texts[ 2] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeVersion),
                            Child, texts[ 3] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeAuthor),
                            Child, texts[ 4] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeCopyright),
                            Child, texts[ 5] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeAnnotation),
                            Child, texts[ 6] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeBits2),
                            Child, texts[ 7] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeMaxChannels),
                            Child, texts[ 8] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeMixFrequency),
                            Child, texts[ 9] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeMaxPlaySamples),
                            Child, texts[10] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeMaxRecordSamples),
                            Child, texts[11] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeMonitorVolume),
                            Child, texts[12] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeInputGain),
                            Child, texts[13] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeOutputVolume),
                            Child, texts[14] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeVolume),
                            Child, texts[15] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeStereo),
                            Child, texts[17] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModePanning),
                            Child, texts[19] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeHiFi),
                            Child, texts[21] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModePingPong),
                            Child, texts[16] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeRecord),
                            Child, texts[18] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeFullDuplex),
                            Child, texts[20] = MyTextObject6(),
                            Child, MyLabel2(txtAudioModeRealTime),
                            Child, texts[22] = MyTextObject6(),
                        End,
                    End,
                End,
            End,
            Child, MyVSpace(4),
            Child, exitButton = MakeButton(txtExit),
        End,
        TAG_MORE, msg->ops_AttrList)) != NULL)
    {
        struct AudioModesDetailWinData *amdwd = INST_DATA(cl, obj);
        APTR parent;

        CopyMemQuick(texts, amdwd->amdwd_Texts, sizeof(amdwd->amdwd_Texts));

        parent = (APTR)GetTagData(MUIA_Window_ParentWindow, (ULONG)NULL, msg->ops_AttrList);

        set(obj, MUIA_Window_DefaultObject, group);

        DoMethod(parent,         MUIM_Window_AddChildWindow, obj);
        DoMethod(obj,            MUIM_Notify, MUIA_Window_CloseRequest, TRUE,  MUIV_Notify_Application, 5, MUIM_Application_PushMethod, parent, 2, MUIM_Window_RemChildWindow, obj);
        DoMethod(exitButton,     MUIM_Notify, MUIA_Pressed,             FALSE, obj,                     3, MUIM_Set, MUIA_Window_CloseRequest, TRUE);
    }

    return (ULONG)obj;
}

STATIC ULONG mDispose( struct IClass *cl,
                       Object *obj,
                       Msg msg )
{
    set(obj, MUIA_Window_Open, FALSE);

    return (DoSuperMethodA(cl, obj, msg));
}

STATIC ULONG mSet( struct IClass *cl,
                   Object *obj,
                   struct opSet *msg )
{
    struct AudioModesDetailWinData *amdwd = INST_DATA(cl, obj);
    struct TagItem *tags, *tag;

    tags = msg->ops_AttrList;
    while ((tag = NextTagItem(&tags)) != NULL) {
        switch (tag->ti_Tag) {
            case MUIA_Window_ParentWindow:
                DoMethod(obj, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 5, MUIM_Application_PushMethod, tag->ti_Data, 2, MUIM_Window_RemChildWindow, obj);
                break;

            case MUIA_AudioModesDetailWin_AudioMode:
                amdwd->amdwd_AudioMode = (struct AudioModeEntry *)tag->ti_Data;
                SetDetails(cl, obj);
                break;
        }
    }

    return (DoSuperMethodA(cl, obj, (Msg)msg));
}

DISPATCHER(AudioModesDetailWinDispatcher)
{
    switch (msg->MethodID) {
        case OM_NEW:     return (mNew(cl, obj, (APTR)msg));
        case OM_DISPOSE: return (mDispose(cl, obj, (APTR)msg));
        case OM_SET:     return (mSet(cl, obj, (APTR)msg));
    }

    return (DoSuperMethodA(cl, obj, msg));
}
DISPATCHER_END

APTR MakeAudioModesDetailWinClass( void )
{
    return MUI_CreateCustomClass(NULL, NULL, ParentWinClass, sizeof(struct AudioModesDetailWinData), DISPATCHER_REF(AudioModesDetailWinDispatcher));
}
