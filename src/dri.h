#include <opendroneid.h>

void dri_init(ODID_UAS_Data *data);
void dri_transmit(ODID_UAS_Data *data);
void dri_update_status(ODID_UAS_Data *data, ODID_status_t status);
void dri_update_location(
    ODID_UAS_Data *data,
    double latitude,
    double longitude,
    double alt,
    double relative_alt,
    float heading
);
void dri_update_operator(
    ODID_UAS_Data *data,
    double lat,
    double lon,
    float alt
);