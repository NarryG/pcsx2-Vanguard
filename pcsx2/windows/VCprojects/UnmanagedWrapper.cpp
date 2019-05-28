#include "PrecompiledHeader.h"
#include "UnmanagedWrapper.h"
#include "App.h"
#include "AppSaveStates.h"
#include "GameDatabase.h"

//C++/CLI has various restrictions (no std::mutex for example), so we can't actually include certain headers directly
//What we CAN do is wrap those functions

// --------------------------------------------------------------------------------------
//  RunVanguardCodeEvent
// --------------------------------------------------------------------------------------
class RunVanguardCodeEvent : public SysExecEvent
{
protected:
    FnPtr_VanguardMethod m_method;
    bool m_IsCritical;

public:
    wxString GetEventName() const { return L"RunVanguardCodeEvent"; }
    virtual ~RunVanguardCodeEvent() = default;
    RunVanguardCodeEvent *Clone() const { return new RunVanguardCodeEvent(*this); }

    bool AllowCancelOnExit() const { return false; }
    bool IsCriticalEvent() const { return m_IsCritical; }

    RunVanguardCodeEvent(FnPtr_VanguardMethod method, bool critical = false)
    {
        m_method = method;
        m_IsCritical = critical;
    }

    RunVanguardCodeEvent &Critical()
    {
        m_IsCritical = true;
        return *this;
    }

protected:
    void InvokeEvent()
    {
        if (m_method)
            (*m_method)();
    }
};



void UnmanagedWrapper::VANGUARD_EXIT()
{
    wxGetApp().Exit();
}

void UnmanagedWrapper::VANGUARD_LOADSTATE(const wxString& file)
{
   StateCopy_LoadFromFile(file);
}

void UnmanagedWrapper::VANGUARD_SAVESTATE(const wxString& file)
{
    StateCopy_SaveToFile(file);
}

std::string UnmanagedWrapper::VANGUARD_GETGAMENAME()
{
    if(!curGameKey.IsEmpty())
    {
        if (IGameDatabase *GameDB = AppHost_GetGameDatabase()) {
            Game_Data game;
            if (GameDB->findGame(game, curGameKey)) {
                return game.getString("Name").ToStdString();
            }
          }
        }
    return "UNKNOWNGAME";
}

void UnmanagedWrapper::VANGUARD_INVOKEEMUTHREAD(void(*f)())
{
    GetSysExecutorThread().Rpc_TryInvoke(f);
}
