cc_binary(
    name = "main",
    srcs =
        glob(["src/**/*.cpp"]) + glob(["src/**/*.c"]) + glob([
            "src/**/*.h",
            #    ]) + glob([
            #        "lib/roo_display/*.h",
            #    ]) + glob([
            #        "lib/roo_display/*.cpp",
        ]) + glob([
            "lib/roo_display/roo_fonts/**/*",
        ]) + glob([
            "lib/roo_display/roo_display/**/*.h",
        ]) + glob([
            "lib/roo_display/roo_display/**/*.cpp",
        ]) + glob([
            "lib/unzipLIB/src/**/*",
        ]),
    # ]) + glob([
    #     "lib/roo_display_adapter_awind/*.h",
    # ]) + [
    #     "//roo_testing/devices/roo_display:src",
    #],
    #data = [":fs_root"],
    defines = [
        "ROO_TESTING",
        "ARDUINO=10805",
        "ESP32",
    ],
    includes = [
        "lib/DallasTemperature-3.8.0",
        "lib/Dusk2Dawn",
        "lib/roo_display",
        "lib/roo_display_adapter_awind",
        #        "lib/roo_logging",
        "lib/roo_monitoring",
        "lib/roo_powersafefs",
        "lib/roo_temperature",
        "lib/unzipLIB/src",
        "src",
        "src/ux",
    ],
    linkstatic = 1,
    deps = [
        "//lib/roo_dashboard",
        "//lib/roo_display",
        "//lib/roo_logging",
        "//lib/roo_icons",
        "//lib/roo_windows",
#        "//lib/roo_toolkit",
        "//lib/roo_time",
        "//lib/roo_scheduler",
        "//roo_testing/devices/display/ili9486:spi",
        "//roo_testing/devices/display/ili9488:spi",
        "//roo_testing/devices/display/ili9341:spi",
        "//roo_testing/devices/touch/xpt2046:spi",
        "//roo_testing/frameworks/arduino-esp32-2.0.4/cores/esp32:main",
        "//roo_testing/transducers/time:default_system_clock",
        "//roo_testing/transducers/ui/viewport:flex",
        "//roo_testing/transducers/ui/viewport/fltk",

        "//roo_testing/devices/display/st77xx:spi",
    ],
)

# filegroup(
#     name = "fs_root",
#     srcs = glob(["fs_root/**"]),
#     visibility = ["//visibility:public"],
# )
