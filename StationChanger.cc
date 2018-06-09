#include <unistd.h>
#include <cstring>

#include "StationChanger.h"
#include "utils.h"
#include "recv_consts.hpp"


StationChanger::StationChanger() {
    if (pipe(pipe_dsc) == -1)
        err("Error in pipe!");
}

int StationChanger::read_sock() {
    return pipe_dsc[0];
}

void StationChanger::change_station() {
    write(pipe_dsc[1], "STATION_CHANGED", strlen("STATION_CHANGED") + 1);
}

StationChanger::~StationChanger() {
    close(pipe_dsc[0]);
    close(pipe_dsc[1]);
}