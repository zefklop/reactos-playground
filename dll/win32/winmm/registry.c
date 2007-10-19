/*
    MME User-Mode Driver Support
    (c) Andrew Greenwood, 2007.

    This is intended for use with WINE's winmm.dll to allow it to find
    and load appropriate drivers from the correct (Windows) registry keys.

    By default, WINE's winmm.dll will look in:
    HKEY_CURRENT_USER\Software\Wine\Drivers

    Here it determines the driver to load based on a value called "Audio".

    Windows NT stores a list of drivers in:
    HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32

    TODO: Handle system.ini
*/

#include <windows.h>
#include "winemm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winmm);

BOOL LoadRegistryMMEDrivers(char* key)
{
    INT driver_count = 0;
    INT driver_index = 0;
    HKEY drivers_key;
    BOOL ret;

    DWORD value_name_length = 256;
    char value_name[value_name_length];

    DWORD value_data_length = 256;
    char value_data[value_data_length];

    DWORD value_type;

    if ( RegOpenKeyA(HKEY_LOCAL_MACHINE, key, &drivers_key) != ERROR_SUCCESS )
    {
        TRACE("ERROR: Could not open: HKEY_LOCAL_MACHINE\\%s\n", key);
        return 0;
    }

    while ( RegEnumValue(drivers_key,
                         driver_index,
                         value_name,
                         &value_name_length,
                         NULL,
                         &value_type,
                         value_data,
                         &value_data_length) == ERROR_SUCCESS )
    {
        BOOL valid_driver = FALSE;
        BOOL is_mapper = FALSE;

        if ( value_type == REG_SZ )
        {
            /* We look for mappers first so they don't match
               something else later on! */

            if ( ! stricmp("wavemapper", value_name) )
            {
                TRACE("Found a Wave-mapper: %s\n", value_data);
                valid_driver = TRUE;
                is_mapper = TRUE;
                driver_count ++;
            }
            else if ( ! stricmp("midimapper", value_name) )
            {
                TRACE("Found a MIDI-mapper: %s\n", value_data);
                valid_driver = TRUE;
                is_mapper = TRUE;
                driver_count ++;
            }
            else if ( ! strnicmp("wave", value_name, 4) )
            {
                TRACE("Found a Wave driver: %s\n", value_data);
                valid_driver = TRUE;
                driver_count ++;
            }
            else if ( ! strnicmp("midi", value_name, 4) )
            {
                TRACE("Found a MIDI driver: %s\n", value_data);
                valid_driver = TRUE;
                driver_count ++;
            }
            else if ( ! strnicmp("mixer", value_name, 5) )
            {
                TRACE("Found a mixer driver: %s\n", value_data);
                valid_driver = TRUE;
                driver_count ++;
            }
            else if ( ! strnicmp("aux", value_name, 4) )
            {
                TRACE("Found an aux driver: %s\n", value_data);
                valid_driver = TRUE;
                driver_count ++;
            }
            else
            {
                TRACE("Not supported: %s\n", value_data);
            }

            /* If we have a valid driver now, we get it "installed" in
               winmm itself so it can be used */

            if ( valid_driver )
            {
                if ( ! MMDRV_Install(value_name, value_data, is_mapper) )
                {
                    TRACE("FAILED when initializing %s\n", value_data);
                }
            }
        }
        else
        {
            TRACE("Invalid data format\n");
        }

        /* Reinitialize */
        value_name_length = 256;
        value_data_length = 256;
        memset(value_name, 0, value_name_length);
        memset(value_data, 0, value_data_length);

        driver_index ++;
    }

    TRACE("Found a total of %d drivers\n", driver_count);

    return driver_count;
}
