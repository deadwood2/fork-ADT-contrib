/****************************************************************************
*** Includes ****************************************************************
****************************************************************************/

#include <proto/feelin.h>

#include <libraries/feelin.h>
#include <feelin/preference.h>

/****************************************************************************
*** Attributes and Methods **************************************************
****************************************************************************/

enum    {

        FA_Listview_List

        };

/****************************************************************************
*** Object ******************************************************************
****************************************************************************/

struct LocalObjectData
{
    FAreaPublic                    *AreaPublic;

    FObject                         list;
    FObject                         group;
    FObject                         vert;
};