# Welcome to this repo?

## How to switch between tinyML and Edge Impulse:

Go into platformio.ini, modify **default_envs** to either:
- tinyml        →  TFLite Micro on-device model
- ei_classifier →  Edge Impulse SDK EON compiled model

tinyML is liteRT micro lib generated from official liteRT repo. This will use:
    - tinyml.cpp (for anormaly detection for temperature/humidity)
    - tinyml_img.cpp (for garbage classification)
edge impulse use the library generated from project website. This will use:
    - ei_inference.cpp

Since both ultilize the liteRT micro library, they will clash if both of them are enabled at the same time. make sure that only one of them is enabled