idf_component_register(
    SRCS "ov64a40_snapshot_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_video ov64a40 imx_serial_img fatfs esp_driver_sdmmc esp_driver_jpeg esp_driver_uart
)

# Fail the build if the OV64A40 ISP tuning config did not resolve. esp_ipa's own
# existence check is spelled message(FETAL_ERROR ...) - not a real CMake mode -
# so it prints and carries on, and a missing file shows up much later as a
# stream with no auto-exposure and no white balance rather than as an error.
# By the time this component is configured the path is final, so check it here.
idf_build_get_property(ov64a40_ipa_json OV64A40_IPA_JSON_RESOLVED)
idf_build_get_property(ov64a40_project_dir PROJECT_DIR)
get_filename_component(ov64a40_ipa_abs "${ov64a40_ipa_json}"
    ABSOLUTE BASE_DIR "${ov64a40_project_dir}")
if(NOT EXISTS "${ov64a40_ipa_abs}")
    message(FATAL_ERROR
        "OV64A40 ISP tuning config not found at:\n  ${ov64a40_ipa_abs}\n"
        "esp_ipa would silently skip it and the camera would run with no "
        "auto-exposure and no white balance. Check the ESP_IPA_JSON_CONFIG_FILE_PATH "
        "block in the project CMakeLists.txt.")
endif()
