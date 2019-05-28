#pragma once
#include "wx/string.h"


typedef void (*FnPtr_VanguardMethod)();
class UnmanagedWrapper
{
public:
    static void VANGUARD_EXIT();
    static void VANGUARD_LOADSTATE(const wxString &file);
    static void VANGUARD_SAVESTATE(const wxString &file);
    static std::string VANGUARD_GETGAMENAME();
    static void VANGUARD_INVOKEEMUTHREAD(void(*)());
};

