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
    static void VANGUARD_STOPGAME();
    static void VANGUARD_LOADGAME(const wxString &file);
    static std::string VANGUARD_GETGAMENAME();
    static void EERAM_POKEBYTE(long long addr, unsigned char val);
    static unsigned char EERAM_PEEKBYTE(long long addr);
    static void IOP_POKEBYTE(long long addr, unsigned char val);
    static unsigned char IOP_PEEKBYTE(long long addr);
    static void GS_POKEBYTE(long long addr, unsigned char val);
    static unsigned char GS_PEEKBYTE(long long addr);
    static void SPU_POKEBYTE(long long addr, unsigned char val);
    static unsigned char SPU_PEEKBYTE(long long addr);
    static std::string VANGUARD_SAVECONFIG();
    static void VANGUARD_LOADCONFIG(wxString cfg);
};

