#include <radkernel/radkernel.h>
#include <radboot.h>
#include <RADCore/RADCore.h>
#include <RADDsp/RADDsp.h>
#include <RADSettings/RADSettings.h>
#include <RADLogging/RADLogging.h>

#include "pico/stdlib.h"

#include <cstdio>

namespace {
void serial_write(const char *text, void*) {
    if (!text) return;
    while (*text) {
        putchar_raw(*text++);
    }
}
}

int main() {
    stdio_init_all();
    sleep_ms(100);

    rad_boot_info_t boot{};
    radboot_prepare_rp235x(&boot, PICO_BOARD, RAD_KERNEL_SD_MODE);

    rad_kernel_config_t config{};
    config.backend_name = "rp235x_pico";
    config.boot_info = &boot;
    rad_kernel_init(&config);

    rad_sd_config_t sd{};
    sd.mode = RAD_SD_MODE_PIO_SDIO;
    sd.mount_point = "/sd";
    rad_vfs_mount_sd(&sd);

    rad_terminal_execute("bootinfo", serial_write, nullptr);
    rad_terminal_execute("cores", serial_write, nullptr);
    rad_terminal_execute("mem", serial_write, nullptr);
    rad_terminal_execute("devices", serial_write, nullptr);

    RADCore::FileSystem::createDirectories("/sd/config");
    RADSettings::Settings settings("/sd/config/radlib-smoke.json");
    settings.setString("board", PICO_BOARD);
    settings.setInt("boot_ms", static_cast<int64_t>(rad_time_millis()));
    settings.save();
    settings.load();

    RADLogging::Logger::instance().clearSinks();
    RADLogging::Logger::instance().addSink(RADLogging::consoleSink());
    RADLogging::Logger::instance().addSink(RADLogging::fileSink("/sd/log/radlib.log"));
    RADLOG_INFO("smoke") << "RADLib embedded smoke on " << PICO_BOARD;
    RADLogging::Logger::instance().flush();

    auto tone = RADDsp::sine(1000.0f, 0.002f, 48000, 0.2f);
    serial_write("dsp_samples=", nullptr);
    char dsp_line[32];
    snprintf(dsp_line, sizeof(dsp_line), "%u\n", static_cast<unsigned>(tone.size()));
    serial_write(dsp_line, nullptr);

    rad_terminal_execute("ls /sd/config", serial_write, nullptr);
    rad_terminal_execute("cat /sd/config/radlib-smoke.json", serial_write, nullptr);

    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        sleep_ms(1);
    }

    rad_kernel_shutdown();
    return 0;
}
