#pragma once
static inline int monthdays(int y, int m) {
    static const int n[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return n[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}
// Sunday=0. Valid for Gregorian dates used by the RTC/calendar.
static inline int weekday(int y, int m, int d) {
    static const int a[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)y--;
    return (y + y / 4 - y / 100 + y / 400 + a[m - 1] + d) % 7;
}
static inline const char *monthname(int m) {
    static const char *const n[] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};
    return n[m >= 1 && m <= 12 ? m - 1 : 0];
}
static inline const char *dayname(int n) {
    static const char *const d[] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};
    return d[n % 7];
}
