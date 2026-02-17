idf_component_register(
                    SRCS "lcd_i2c.cpp"        # ton fichier source C++
                    INCLUDE_DIRS "."          # dossier où se trouve lcd_i2c.h
                    REQUIRES driver freertos  # dépendances ESP-IDF utilisées
)