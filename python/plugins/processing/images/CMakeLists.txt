FILE(GLOB IMAGE_FILES *.png)

PLUGIN_INSTALL(processing ./images ${IMAGE_FILES})
