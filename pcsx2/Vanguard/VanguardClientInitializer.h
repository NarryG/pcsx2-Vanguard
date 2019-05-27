#pragma once
class VanguardClientInitializer
{
public:
    static void Initialize();

private:
    static void StartVanguardClient();
};

static std::string VANGUARD_GameName = "";