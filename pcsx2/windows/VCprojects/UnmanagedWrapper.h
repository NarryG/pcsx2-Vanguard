#pragma once
#include "wx/string.h"


class UnmanagedWrapper
{
public:
    static std::string VANGUARD_GameName;
    static void VANGUARD_EXIT();
    static void VANGUARD_LOADSTATE(const wxString &file);
    static void VANGUARD_SAVESTATE(const wxString &file);
    static std::string VANGUARD_GETGAMENAME();
};
