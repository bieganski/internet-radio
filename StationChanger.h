#ifndef NETWORK_RADIO_STATIONCHANGER_H
#define NETWORK_RADIO_STATIONCHANGER_H


class StationChanger {
private:
    int pipe_dsc[2];
public:
    StationChanger();

    int read_sock();

    void change_station();

    ~StationChanger();
};


#endif //NETWORK_RADIO_STATIONCHANGER_H
