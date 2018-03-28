

int greenLight_isGreen(GreenLight *item) {
    if (item->active) {
        if (acp_readSensorFTS(&item->sensor)) {
            if (item->sensor.value.value == item->green_value) {
                return 1;
            }
        }
        return 0;
    }
    return 1;
}