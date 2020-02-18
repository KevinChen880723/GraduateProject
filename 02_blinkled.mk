NAME := apps_ww101_02_02_blinkled
$(NAME)_SOURCES := 02_blinkled.c

GLOBAL_DEFINES += WICED_DISABLE_STDIO

WIFI_CONFIG_DCT_H := wifi_config_dct.h

$(NAME)_COMPONENTS := protocols/MQTT

$(NAME)_RESOURCES  := apps/aws_iot/rootca.cer \
                      apps/aws_iot/client.cer \
                      apps/aws_iot/privkey.cer