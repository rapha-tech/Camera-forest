# SPDX-License-Identifier: Apache-2.0

set(srcs)
set(include_dirs)
set(priv_include_dirs)

if(CONFIG_CAMERA_OV64A40)
    list(APPEND srcs "ov64a40.c")
    list(APPEND include_dirs "include")
    list(APPEND priv_include_dirs "private_include")
endif()

# ISP tuning (IPA) configs live in sensors/<name>/cfg/<name>_default.json, but
# they CANNOT be registered from here: esp_ipa reads ESP_IPA_JSON_CONFIG_FILE_PATH
# while its own CMakeLists is processed, which has already happened by the time a
# component's CMakeLists runs. The app must register them in its project
# CMakeLists, after include(project.cmake) and before project() - see
# examples/imx708_snapshot/CMakeLists.txt. Without that registration
# esp_ipa_pipeline_get_config("IMX708") returns NULL, esp_video logs
# "failed to get configuration to initialize ISP controller", and you get no
# auto-exposure and no white balance.

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_INCLUDE_DIRS ${priv_include_dirs}
    REQUIRES esp_cam_sensor
    PRIV_REQUIRES esp_driver_gpio esp_sccb_intf esp_timer
)

# Force the linker to keep each sensor's detect object so its entry lands in the
# esp_cam_sensor auto-detect array. Without this the linker drops the object,
# since nothing in the app references it directly.
if(CONFIG_CAMERA_OV64A40)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov64a40_detect")
endif()
