//image fxns to load , free an img + check if its transparent

int ashi_image_load(const char *path , ashi_image_t *img);

void ashi_image_free(ashi_image_t *img);

bool ashi_check_has_alpha(const uint8_t *pixels, int pixel_count);


