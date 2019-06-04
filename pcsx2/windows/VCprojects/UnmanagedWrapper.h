#pragma once
#include "wx/string.h"


typedef void (*FnPtr_VanguardMethod)();
class UnmanagedWrapper
{
public:
    static void VANGUARD_EXIT();
    static void VANGUARD_LOADSTATE(const wxString &file);
    static void VANGUARD_SAVESTATE(const wxString &file);
    static void VANGUARD_RESUMEEMULATION();
    static void VANGUARD_LOADGAME(const wxString &file);
    static std::string VANGUARD_GETGAMENAME();
    static void EERAM_POKEBYTE(long long addr, unsigned char val);
    static unsigned char EERAM_PEEKBYTE(long long addr);
};

