#ifndef RRULE_PARSER_HPP
#define RRULE_PARSER_HPP

#include <Arduino.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <map>
#include "PsramUtils.hpp"
#include "GeneralTimeConverter.hpp" // <-- DIE ENTSCHEIDENDE KORREKTUR

// Vorwärtsdeklaration, da die Implementierung in die .cpp-Datei umzieht
struct Event; 

// Funktionsdeklarationen
time_t parseICalDateTime(const char* line, size_t len, bool& isAllDay, const GeneralTimeConverter* converter = nullptr);
void parseVEvent(const char* veventBlock, size_t len, Event& event, const GeneralTimeConverter* converter = nullptr);

template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind = 15, const GeneralTimeConverter* converter = nullptr);


// Struct-Definition bleibt im Header, da sie von anderen Dateien benötigt wird
struct Event {
    PsramString summary;
    PsramString rrule;
    PsramString uid;
    time_t dtstart = 0;
    time_t dtend = 0;
    time_t recurrence_id = 0;
    std::vector<time_t, PsramAllocator<time_t>> exdates;
    bool isAllDay = false;
    time_t duration = 0;
};

// Die Template-Implementierung muss im Header bleiben.
template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind, const GeneralTimeConverter* converter) {
    if (masterEvent.rrule.empty() || masterEvent.dtstart == 0) return;
    occurrences.clear();

    PsramString rrule_str = masterEvent.rrule; 
    PsramString freq = "";
    int interval = 1;
    int count = -1;
    time_t until = 0;

    size_t last = 0;
    while (last < rrule_str.length()) {
        size_t next = rrule_str.find(';', last);
        if (next == PsramString::npos) next = rrule_str.length();
        PsramString part = rrule_str.substr(last, next - last);
        last = (next == rrule_str.length()) ? next : next + 1;

        if (part.rfind("FREQ=", 0) == 0) {
            freq = part.substr(5);
        } else if (part.rfind("INTERVAL=", 0) == 0) {
            interval = atoi(part.substr(9).c_str());
        } else if (part.rfind("COUNT=", 0) == 0) {
            count = atoi(part.substr(6).c_str());
        } else if (part.rfind("UNTIL=", 0) == 0) {
            PsramString u = part.substr(6);
            bool d; until = parseICalDateTime(u.c_str(), u.length(), d, converter);
        }
    }

    time_t now_t;
    time(&now_t);
    struct tm now_tm;
    gmtime_r(&now_t, &now_tm);

    struct tm start_tm;
    gmtime_r(&masterEvent.dtstart, &start_tm);

    struct tm current_tm = start_tm;
    if (freq == "YEARLY") {
        if (current_tm.tm_year < now_tm.tm_year) {
            int year_diff = now_tm.tm_year - current_tm.tm_year;
            int intervals_to_skip = year_diff / interval;
            current_tm.tm_year += intervals_to_skip * interval;
        }
    } else if (freq == "MONTHLY") {
         if (current_tm.tm_year < now_tm.tm_year || (current_tm.tm_year == now_tm.tm_year && current_tm.tm_mon < now_tm.tm_mon)) {
            int month_diff = (now_tm.tm_year - current_tm.tm_year) * 12 + (now_tm.tm_mon - current_tm.tm_mon);
            int intervals_to_skip = month_diff / interval;
            current_tm.tm_mon += intervals_to_skip * interval;
            current_tm.tm_year += current_tm.tm_mon / 12;
            current_tm.tm_mon %= 12;
        }
    }
    
    int eventsFound = 0;
    int futureEventsFound = 0;
    int loop_count = 0;

    while (futureEventsFound < numFutureEventsToFind) {
        if (loop_count++ > 500) break;

        time_t current_t = timegm(&current_tm);

        if (until && current_t > until) break;
        if (count != -1 && eventsFound >= count) break;

        if (current_t >= masterEvent.dtstart) {
            bool is_exdate = false;
            for(time_t ex : masterEvent.exdates) {
                if (ex == current_t) { is_exdate = true; break; }
            }
            if (!is_exdate) {
                occurrences.push_back(current_t);
                eventsFound++;
                if (current_t >= now_t) {
                    futureEventsFound++;
                }
            }
        }

        if (freq == "YEARLY") current_tm.tm_year += interval;
        else if (freq == "MONTHLY") current_tm.tm_mon += interval;
        else if (freq == "WEEKLY") current_tm.tm_mday += 7 * interval;
        else if (freq == "DAILY") current_tm.tm_mday += interval;
        else break;
        
        time_t temp_t = timegm(&current_tm);
        gmtime_r(&temp_t, &current_tm);
    }
}

#endif // RRULE_PARSER_HPP