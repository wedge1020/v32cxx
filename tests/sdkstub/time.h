// Minimal stand-in for the Vircon32 SDK's time.h, used by `make test` to run
// tests/90sample.cpp the way it runs when the real SDK headers are found
// (prescan then harvests these structs, and `native date_info;` must still
// be accepted). Only the shapes prescan reads matter here.
#ifndef TIME_H
#define TIME_H
int get_date() { return 0; }
int get_time() { return 0; }
struct date_info
{
    int year;
    int month;
    int day;
};
void translate_date( int date, date_info* translated ) { }
struct time_info
{
    int hours;
    int minutes;
    int seconds;
};
void translate_time( int time, time_info* translated ) { }
#endif
