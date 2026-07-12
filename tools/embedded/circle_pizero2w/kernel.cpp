#include <radixkernel/radixkernel.h>
#include <radboot.h>
#include <RADCore/RADCore.h>
#include <RADDsp/RADDsp.h>
#include <RADSettings/RADSettings.h>
#include <RADLogging/RADLogging.h>

#include "kernel.h"

#include <string.h>
#include <stdio.h>

extern "C" void rad_circle_bind_platform(void *serial_device, void *timer);
extern "C" void rad_circle_bind_platform_ex(void *serial_device, void *timer, void *screen_device);

namespace {
static const char FromKernel[] = "radixkernel";

void serial_write(const char *text, void *context) {
    CSerialDevice *serial = static_cast<CSerialDevice*>(context);
    if (!serial || !text) return;
    serial->Write(text, strlen(text));
}
}

CKernel::CKernel(void)
: m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
  m_Timer(&m_Interrupt),
  m_Logger(m_Options.GetLogLevel(), &m_Timer) {
}

CKernel::~CKernel(void) {
}

boolean CKernel::Initialize(void) {
    boolean ok = TRUE;
    if (ok) ok = m_Screen.Initialize();
    if (ok) ok = m_Serial.Initialize(115200);
    if (ok) {
        CDevice *target = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (target == 0) target = &m_Screen;
        ok = m_Logger.Initialize(target);
    }
    if (ok) ok = m_Interrupt.Initialize();
    if (ok) ok = m_Timer.Initialize();
    return ok;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write(FromKernel, LogNotice, "RADixKernel Circle smoke");
    rad_circle_bind_platform_ex(&m_Serial, &m_Timer, &m_Screen);

    rad_boot_info_t boot{};
    radboot_prepare_circle(&boot, "pi-zero-2w", "auto");

    rad_kernel_config_t config{};
    config.backend_name = "circle_pi";
    config.boot_info = &boot;
    rad_kernel_init(&config);

    rad_sd_config_t sd{};
    sd.mode = RAD_SD_MODE_AUTO;
    sd.mount_point = "/sd";
    rad_vfs_mount_sd(&sd);

    rad_terminal_execute("bootinfo", serial_write, &m_Serial);
    rad_terminal_execute("cores", serial_write, &m_Serial);
    rad_terminal_execute("mem", serial_write, &m_Serial);
    rad_terminal_execute("devices", serial_write, &m_Serial);

    RADCore::FileSystem::createDirectories("/sd/config");
    RADSettings::Settings settings("/sd/config/radlib-smoke.json");
    settings.setString("board", "pi-zero-2w");
    settings.setInt("boot_ms", static_cast<int64_t>(rad_time_millis()));
    settings.save();
    settings.load();

    RADLogging::Logger::instance().clearSinks();
    RADLogging::Logger::instance().addSink(RADLogging::consoleSink());
    RADLogging::Logger::instance().addSink(RADLogging::fileSink("/sd/log/radlib.log"));
    RADLOG_INFO("smoke") << "RADLib embedded smoke on pi-zero-2w";
    RADLogging::Logger::instance().flush();

    auto tone = RADDsp::sine(1000.0f, 0.002f, 48000, 0.2f);
    serial_write("dsp_samples=", &m_Serial);
    char dsp_line[32];
    snprintf(dsp_line, sizeof(dsp_line), "%u\n", static_cast<unsigned>(tone.size()));
    serial_write(dsp_line, &m_Serial);

    rad_terminal_execute("ls /sd/config", serial_write, &m_Serial);
    rad_terminal_execute("cat /sd/config/radlib-smoke.json", serial_write, &m_Serial);

    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        m_Timer.usDelay(1000);
    }

    rad_kernel_shutdown();
    return ShutdownHalt;
}
