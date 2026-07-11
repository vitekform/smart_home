//
// Created by ganamaga on 03.07.26.
//

#include "fastfetch.h"

#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_cpu.h"

std::string os_logo = R"(


                                                ******************
                                                ***********************
                                 ******             ***********************
                  **          ****************          *********************
                 *****     ************************         ********************
               *******   *****************************         *******************
              ******     *********************************       *******************
             *****       ***********************************        *****************
            *****        **************************************       *****************
          ******         *****************************************      ****************
         ******                      *******************************      ***************
         *****                             ***************************      **************
        *****          ************            *************************     **************
       *****       **********************          ***********************     *************
       *****     *****************************        *********************      ************
      *****    **********************************        ********************     ***********
      *****   **************************************       *******************     ***********
     *****   *****************************************       *******************    ***********
     *****   *******************************************       ******************    **********
     *****  **********************************************      ******************     ********
     *****  ***************      ***************************      *****************    ********
     ****   **************             ***********************     *****************    *******
     ****   *******************           *********************     *****************    ******
     ****    ************************        ********************    *****************    *****
     *****   ***************************       *******************    ****************     ****
     *****    *****************************      ******************    ****************
     *****     ******************************     ******************    ****************
     *****       ******************************     ****************     ***************
      *****        *****************************     ****************     ***************
      *****            **************************     ****************    ***************
       *****                   *******************     ***************     ***************
       *****                      *****************     ***************    ***************
        *****                       ****************     **************     **************
         *****           *******     ****************    ***************    **************
          *****        ***********    ***************    ***************    *************
           *****      *************    ***************    **************     ***********
            *****     *************    ***************    **************     *********
             ******   *************     **************    ***************    ********
              ******  ************      **************    ***************
                ******  *********      ***************    ***************
                 ******                ***************    **************             ***
                   ******             ***************    ***************           ******
                     *******         ****************    ***************         *******
                       ********         ************      ************        ********
                         *********         *********            *          *********
                            **********                                 **********
                               *************                     *************
                                   ***************************************
                                         ***************************


                                                                                                    )";
/*
 * Example fastfetch
*                   -`                     ganamaga@archlinux
                 .o+`                    ------------------
                `ooo/                    OS: Arch Linux x86_64
               `+oooo:                   Host: 83CV (Yoga Slim 7 14IMH9)
              `+oooooo:                  Kernel: Linux 6.18.37-1-lts
              -+oooooo+:                 Uptime: 17 mins
            `/:-:++oooo+:                Packages: 1048 (pacman)
           `/++++/+++++++:               Shell: fish 4.7.1
          `/++++++++++++++:              Display (LEN68C6): 2560x1440 in 27", 144 Hz [External]
         `/+++ooooooooooooo/`            DE: KDE Plasma 6.7.2
        ./ooosssso++osssssso+`           WM: KWin (Wayland)
       .oossssso-````/ossssss+`          WM Theme: Breeze
      -osssssso.      :ssssssso.         Theme: Breeze (Dark) [Qt], Breeze-Dark [GTK2], Breeze [GTK3]
     :osssssss/        osssso+++.        Icons: breeze-dark [Qt], breeze-dark [GTK2/3/4]
    /ossssssss/        +ssssooo/-        Font: Noto Sans (10pt) [Qt], Noto Sans (10pt) [GTK2/3/4]
  `/ossssso+/:-        -:/+osssso+-      Cursor: breeze (24px)
 `+sso+:-`                 `.-/+oso:     Terminal: kitty 0.47.1
`++:.                           `-/+/    Terminal Font: NotoSansMono-Regular (11pt)
.`                                 `/    CPU: Intel(R) Core(TM) Ultra 7 155H (12+8+2) @ 4.80 GHz
                                         GPU: Intel Arc Graphics @ 2.25 GHz [Integrated]
                                         Memory: 11.44 GiB / 30.72 GiB (37%)
                                         Swap: Disabled
                                         Disk (/): 129.29 GiB / 898.30 GiB (14%) - zfs
                                         Local IP (wlp0s20f3): 192.168.68.107/24
                                         Battery (L22M4PF1): 95% [AC Connected, Charging]
                                         Locale: C.UTF-8
 */

void runHwProbe(){
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0;
    }

    int64_t uptime_us = esp_timer_get_time();
    int64_t uptime_s = uptime_us / 1000000;
    int h = uptime_s / 3600;
    int m = (uptime_s % 3600) / 60;
    int s = uptime_s % 60;

    size_t free_heap = esp_get_free_heap_size();
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);

    std::vector<std::string> info;
    info.push_back("ganamaga@esp32");
    info.push_back("--------------");
    info.push_back("OS: ESP-IDF " + std::string(esp_get_idf_version()));
    
    std::string chip_model;
    switch(chip_info.model) {
        case CHIP_ESP32: chip_model = "ESP32"; break;
        case CHIP_ESP32S2: chip_model = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model = "ESP32-C3"; break;
        case CHIP_ESP32H2: chip_model = "ESP32-H2"; break;
        default: chip_model = "Unknown ESP32"; break;
    }
    info.push_back("Host: " + chip_model + " (Rev " + std::to_string(chip_info.revision) + ")");
    
    info.push_back("CPU: " + std::to_string(chip_info.cores) + " core(s) @ " + std::to_string(240) + "MHz");
    
    std::stringstream ss_mem;
    ss_mem << (total_heap - free_heap) / 1024 << " KiB / " << total_heap / 1024 << " KiB";
    info.push_back("Memory: " + ss_mem.str());
    
    info.push_back("Flash: " + std::to_string(flash_size / (1024 * 1024)) + " MB");
    
    std::stringstream ss_uptime;
    if (h > 0) ss_uptime << h << "h ";
    if (m > 0 || h > 0) ss_uptime << m << "m ";
    ss_uptime << s << "s";
    info.push_back("Uptime: " + ss_uptime.str());

    // BT and BLE are hardware capabilities of the SoC but are explicitly
    // disabled in software (CONFIG_BT_ENABLED=n in sdkconfig.defaults).
    std::string features = "";
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) features += "WiFi ";
    // Bluetooth intentionally omitted — disabled via sdkconfig.defaults
    if (!features.empty()) info.push_back("Features: " + features);

    std::vector<std::string> logo_lines;
    std::stringstream ss_logo(os_logo);
    std::string line;
    while (std::getline(ss_logo, line)) {
        logo_lines.push_back(line);
    }

    size_t logo_width = 0;
    for (const auto& l : logo_lines) {
        if (l.length() > logo_width) logo_width = l.length();
    }

    size_t info_start_line = (logo_lines.size() > info.size()) ? (logo_lines.size() - info.size()) / 2 : 0;

    size_t max_lines = std::max(logo_lines.size(), info_start_line + info.size());
    for (size_t i = 0; i < max_lines; ++i) {
        std::string out = "";
        if (i < logo_lines.size()) {
            out = logo_lines[i];
        }
        
        if (i >= info_start_line && (i - info_start_line) < info.size()) {
            int pad = (int)logo_width - (int)out.length();
            if (pad > 0) out += std::string(pad, ' ');
            out += "  " + info[i - info_start_line];
        }
        
        std::cout << out << std::endl;
    }
};