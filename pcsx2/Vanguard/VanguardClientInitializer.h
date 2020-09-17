#pragma once
class VanguardClientInitializer
{
public:
    static void Initialize();

private:
    static void StartVanguardClient();
    static void ConfigureVisualStyles();
};
