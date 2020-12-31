#include "includes.h"

HMODULE hMyModule;

namespace console
{
    FILE* output_stream = nullptr;

    void attach(const char* name)
    {
        if (AllocConsole())
        {
            freopen_s(&output_stream, "conout$", "w", stdout);
        }
        SetConsoleTitle(name);
    }

    void detach()
    {
        if (output_stream)
        {
            fclose(output_stream);
        }
        FreeConsole();
    }
}

namespace memory_utils
{
    #ifdef _WIN64
        #define PTRMAXVAL ((PVOID)0x000F000000000000)
    #elif _WIN32
        #define PTRMAXVAL ((PVOID)0xFFF00000)
    #endif

    bool is_valid_ptr(PVOID ptr)
    {
        return (ptr >= (PVOID)0x10000) && (ptr < PTRMAXVAL) && ptr != nullptr && !IsBadReadPtr(ptr, sizeof(ptr));
    }

    template<class T>
    void write(std::vector<DWORD>address, T value)
    {
        size_t lengh_array = address.size() - 1;
        DWORD relative_address;
        relative_address = address[0];
        for (int i = 1; i < lengh_array + 1; i++)
        {
            if (is_valid_ptr((LPVOID)relative_address) == false)
                return;

            if (i < lengh_array)
                relative_address = *(DWORD*)(relative_address + address[i]);
            else
            {
                T* writable_address = (T*)(relative_address + address[lengh_array]);
                *writable_address = value;
            }
        }
    }

    template<class T>
    T read(std::vector<DWORD>address)
    {
        size_t lengh_array = address.size() - 1;
        DWORD relative_address;
        relative_address = address[0];
        for (int i = 1; i < lengh_array + 1; i++)
        {
            if (is_valid_ptr((LPVOID)relative_address) == false)
                return 0;

            if (i < lengh_array)
                relative_address = *(DWORD*)(relative_address + address[i]);
            else
            {
                T readable_address = *(T*)(relative_address + address[lengh_array]);
                return readable_address;
            }
        }
    }

    DWORD get_module_size(DWORD address)
    {
        return PIMAGE_NT_HEADERS(address + (DWORD)PIMAGE_DOS_HEADER(address)->e_lfanew)->OptionalHeader.SizeOfImage;
    }

    DWORD find_pattern(HMODULE module, const char* pattern, const char* mask)
    {
        DWORD base = (DWORD)module;
        DWORD size = get_module_size(base);

        DWORD patternLength = (DWORD)strlen(mask);

        for (DWORD i = 0; i < size - patternLength; i++)
        {
            bool found = true;
            for (DWORD j = 0; j < patternLength; j++)
            {
                found &= mask[j] == '?' || pattern[j] == *(char*)(base + i + j);
            }

            if (found)
            {
                return base + i;
            }
        }

        return NULL;
    }

    void patch_instruction(DWORD instruction_address, const char* instruction_bytes, int sizeof_instruction_byte)
    {
        DWORD dwOldProtection;

        VirtualProtect((LPVOID)instruction_address, sizeof_instruction_byte, PAGE_EXECUTE_READWRITE, &dwOldProtection);

        memcpy((LPVOID)instruction_address, instruction_bytes, sizeof_instruction_byte);    

        VirtualProtect((LPVOID)instruction_address, sizeof_instruction_byte, dwOldProtection, NULL);

        FlushInstructionCache(GetCurrentProcess(), (LPVOID)instruction_address, sizeof_instruction_byte);
    }
}

void infinity_energy(DWORD cry_game_module)
{
    static std::initializer_list<DWORD>energy_ptr{ cry_game_module, 0x0029CD54, 0x2C, 0x44, 0x54, 0x3C, 0x34 };

    constexpr auto my_custom_energy = 9999.f;
    memory_utils::write(energy_ptr, my_custom_energy * 2.f);
}

void god_mode(DWORD cry_game_module)
{
    static std::initializer_list<DWORD>health_ptr{ cry_game_module, 0x0029CCDC, 0x2C, 0x18, 0x3C, 0x4, 0x40 };

    constexpr auto my_custom_health = 228.f;
    memory_utils::write(health_ptr, my_custom_health * 2.f);
}

void infinity_ammo(DWORD ammo_instruction_address, bool is_enable)
{
    if (is_enable)
    {
        memory_utils::patch_instruction(ammo_instruction_address, "\x90\x90\x90", 3);
    }
    else
    {     
        memory_utils::patch_instruction(ammo_instruction_address, "\x89\x50\x14", 3);
    }
}

void no_recoil(DWORD weapon_recoil_instruction_address, bool is_enable)
{
    if (is_enable)
    {   
        memory_utils::patch_instruction(weapon_recoil_instruction_address, "\x90\x90", 2);
    }
    else
    {
        memory_utils::patch_instruction(weapon_recoil_instruction_address, "\x89\x01", 2);
    }
}

void no_take_damage(DWORD health_take_instruction_address, bool is_enable)
{
    if (is_enable)
    {
        memory_utils::patch_instruction(health_take_instruction_address, "\x90\x90\x90\x90\x90", 5);
    }
    else
    {
        memory_utils::patch_instruction(health_take_instruction_address, "\xF3\x0F\x11\x46\x40", 5);
    }
}

void start_hack()
{
    console::attach("crysis hax");

    std::system("cls");

    const char* you_are_welcome_message = {
        "\n"
        "Hello!\n"
        "hack for crysis version: 1.1.1.6115\n"
        "compilation date: %s\n"
        "hack function key:\n"
        "F1 - infinity energy\n"
        "F2 - god mode\n"
        "F3 - unlimited ammo\n"
        "F4 - no recoil\n"
        "\n"
    };

    char welcome_message[256];
    sprintf(welcome_message, you_are_welcome_message, __DATE__);

    std::cout << welcome_message << std::endl;
    
    HMODULE cry_game_hmodule = GetModuleHandle("CryGame.dll");

    DWORD cry_game_module = (DWORD)cry_game_hmodule;

    std::cout << "CryGame.dll address: 0x" << cry_game_module << std::endl << std::endl;

    DWORD ammo_instruction_address = memory_utils::find_pattern(cry_game_hmodule, "\x89\x50\x14\xEB\x21", "xxxxx");

    DWORD weapon_recoil_instruction_address = memory_utils::find_pattern(cry_game_hmodule, "\x89\x01\x89\x51\x04\xE8", "xxxxxx");

    /*DWORD health_take_damage_address = memory_utils::find_pattern(cry_game_hmodule, "\xF3\x0F\x11\x46\x40\x7B", "xxxxxx");*/

    bool enable_infinity_energy = false;
    bool enable_god_mode = false;
    bool enable_unlimited_ammo = false;
    bool enable_no_recoil = false;
    bool enable_no_take_damage = false;

    while (true)
    {
        if (GetAsyncKeyState(VK_DELETE))
            break;
   
        static bool pressed_infinity_energy_key = false;
        bool my_key_infinity_energy = GetAsyncKeyState(VK_F1);
        if (my_key_infinity_energy && pressed_infinity_energy_key == false)
        {
            enable_infinity_energy = !enable_infinity_energy;
            pressed_infinity_energy_key = true;
            std::cout << "infinity energy is: " << enable_infinity_energy << std::endl;
        }
        else if (my_key_infinity_energy == false)
        {
            pressed_infinity_energy_key = false;
        }

        if (enable_infinity_energy)
        {
            infinity_energy(cry_game_module);
        }

        static bool pressed_god_mode_key = false;
        bool my_key_god_mode = GetAsyncKeyState(VK_F2);
        if (my_key_god_mode && pressed_god_mode_key == false)
        {
            enable_god_mode = !enable_god_mode;
            pressed_god_mode_key = true;
            std::cout << "god mode is: " << enable_god_mode << std::endl;
        }
        else if (my_key_god_mode == false)
        {
            pressed_god_mode_key = false;
        }

        if (enable_god_mode)
        {
            god_mode(cry_game_module);
        }
  
        static bool pressed_unlimited_ammo_key = false;
        bool my_key_unlimited_ammo = GetAsyncKeyState(VK_F3);
        if (my_key_unlimited_ammo && pressed_unlimited_ammo_key == false)
        {
            enable_unlimited_ammo = !enable_unlimited_ammo;
            pressed_unlimited_ammo_key = true;
            infinity_ammo(ammo_instruction_address, enable_unlimited_ammo);
            std::cout << "unlimited ammo is: " << enable_unlimited_ammo << std::endl;
        }
        else if (my_key_unlimited_ammo == false)
        {
            pressed_unlimited_ammo_key = false;
        }

        static bool pressed_no_recoil_key = false;
        bool my_key_no_recoil = GetAsyncKeyState(VK_F4);
        if (my_key_no_recoil && pressed_no_recoil_key == false)
        {
            enable_no_recoil = !enable_no_recoil;
            pressed_no_recoil_key = true;
            no_recoil(weapon_recoil_instruction_address, enable_no_recoil);
            std::cout << "no recoil is: " << enable_no_recoil << std::endl;
        }
        else if (my_key_no_recoil == false)
        {
            pressed_no_recoil_key = false;
        }

        /*
        static bool pressed_no_take_damage_key = false;
        bool my_key_no_take_damage = GetAsyncKeyState('Q');
        if (my_key_no_take_damage && pressed_no_take_damage_key == false)
        {
            enable_no_take_damage = !enable_no_take_damage;
            pressed_no_take_damage_key = true;
            no_take_damage(health_take_damage_address, enable_no_take_damage);
            std::cout << "no take damage is: " << enable_no_take_damage << std::endl;
        }
        else if (my_key_no_take_damage == false)
        {
            pressed_no_take_damage_key = false;
        }*/

        Sleep(1);
    }

    if (enable_unlimited_ammo)
    {
        std::cout << "unlimited ammo bytes is patched, disable...\n";
        infinity_ammo(ammo_instruction_address, false);
    }
    
    if (enable_no_recoil)
    {
        std::cout << "no recoil bytes is patched, disable...\n";
        no_recoil(weapon_recoil_instruction_address, false);
    }

    std::cout << "free library\n";
    FreeLibraryAndExitThread(hMyModule, 0);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        hMyModule = hModule;
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)start_hack, 0, 0, 0);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

