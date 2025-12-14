# SofaScore Module - Known Issues and Fixes

## Status: Analysis Phase

This document tracks known issues in the SofaScoreLiveModule and their fixes.

## Branch
- **Branch**: `copilot/refactor-sofascore-module`
- **Created**: 2025-12-14
- **Purpose**: Fix errors in Sofascore module JSON parsing and data handling

## Known Issues

### Issue #1: NULL shortName Causes Crashes ✅ FIXED
**Status**: FIXED  
**Severity**: HIGH - Caused crashes

**Problem**: 
- Some players have `null` or missing `shortName` in JSON
- Original code at line 689-698 didn't handle this properly
- When `shortName` is null, `strlen(homeName)` would crash

**Example Events with null shortName**:
- Event 15142775: Tata J. has `shortName: null`
- Event 15211627: Chrysospathis P. has `shortName: null`
- Found 40+ occurrences across test JSON files

**Fix Applied**:
```cpp
// Check NULL before any string operations
const char* homeName = event["homeTeam"]["shortName"];
if (!homeName) {  // Check NULL first - SAFE
    homeName = event["homeTeam"]["name"];
    Log.printf("[SofaScore]   ⚠️  Home shortName is NULL, using full name\n");
} else if (*homeName == '\0') {  // Empty string check
    homeName = event["homeTeam"]["name"];
}
```

**Result**: No more crashes, proper fallback to full name with logging

---

### Issue #2: Empty Score Objects Not Handled ✅ FIXED
**Status**: FIXED  
**Severity**: MEDIUM

**Problem**:
- Scheduled matches (status: "notstarted") have empty score objects `{}`
- Code accessed fields without checking if object was empty
- Found 48 occurrences in test files

**Fix Applied**:
```cpp
JsonObject homeScore = event["homeScore"];
if (!homeScore.isNull() && homeScore.size() > 0) {  // Check size!
    match.homeScore = homeScore["current"] | 0;
    // ... parse period scores
} else {
    Log.printf("[SofaScore]   ⚠️  Home score object is empty (scheduled match)\n");
}
```

**Result**: Scheduled matches handled correctly, no invalid field access

---

### Issue #3: Date Filtering Logic - Timezone Bug ✅ FIXED
**Status**: FIXED  
**Severity**: CRITICAL - Caused "No matches today"

**Problem**:
User reported date switch happens at 1 AM instead of midnight (00:00).

**Root Cause**: 
The `isSameDay()` function in GeneralTimeConverter was broken! It applied timezone conversion TWICE:
1. `toLocal()` converted UTC to local time correctly
2. But then `localtime_r()` applied SYSTEM timezone AGAIN
3. This caused double offset (e.g., UTC+1 became UTC+2)

**Original Broken Code** (GeneralTimeConverter.cpp:83-94):
```cpp
bool GeneralTimeConverter::isSameDay(time_t epoch1, time_t epoch2) const {
    time_t local1 = toLocal(epoch1);  // Converts to local: UTC -> UTC+1
    time_t local2 = toLocal(epoch2);

    struct tm tm1, tm2;
    localtime_r(&local1, &tm1);  // BUG: Applies timezone AGAIN!
    localtime_r(&local2, &tm2);  // Double conversion: UTC+1 -> UTC+2
    // ...
}
```

**Fix Applied**:
```cpp
bool GeneralTimeConverter::isSameDay(time_t epoch1, time_t epoch2) const {
    time_t local1 = toLocal(epoch1);  // Already in local time
    time_t local2 = toLocal(epoch2);

    struct tm tm1, tm2;
    gmtime_r(&local1, &tm1);  // FIXED: Interpret as UTC (no double conversion)
    gmtime_r(&local2, &tm2);
    
    return (tm1.tm_year == tm2.tm_year &&
            tm1.tm_mon == tm2.tm_mon &&
            tm1.tm_mday == tm2.tm_mday);
}
```

**Result**: Date comparison now works correctly at midnight (00:00) in configured timezone

**Test Results**:
- Python test shows correct filtering by date
- File `2025-12-14.json` correctly filters to Dec 14 matches only
- No more "No matches today" when matches exist

---

### Issue #4: Page Numbering Starts at 0 ✅ VERIFIED OK
**Status**: VERIFIED - Working correctly  
**Severity**: N/A

**Analysis**:
- Internal `_currentPage` starts at 0 (line 325)
- Display correctly shows `_currentPage + 1` (lines 934, 1097)
- Pages display as "1/5", "2/5", etc. - CORRECT

**Verification**: Code is correct, no fix needed

---

### Issue #5: Missing Logging for Debugging ✅ FIXED
**Status**: FIXED  
**Severity**: LOW - But critical for debugging

**Fix Applied**: Added extensive logging with emoji indicators:

**parseDailyEventsJson**:
- 📅 Current date (local timezone)
- 📊 Total events found in JSON
- 🎯 Each event being parsed with ID
- 👤 Player names (with warnings for NULL shortName)
- 📊 Scores (with warnings for empty objects)
- 🏆 Tournament info
- 🔴 Status (LIVE/FINISHED/SCHEDULED)
- 🕐 Match times in local timezone
- ✅ Summary: total events, filtered counts, live matches

**parseTournamentsJson**:
- 📊 Tournament groups found
- 🏆 Each tournament discovered
- ✅ Enabled/disabled status based on filter
- 🎯 Final enabled count

**parseMatchStatistics**:
- 📊 Statistics being parsed
- ✅ Match found/not found in liveMatches
- 📊 Average values
- 🎯 180s counts
- 💯 Checkout percentages
- ✅ Final summary of all stats

**groupMatchesByTournament**:
- 📁 Grouping process
- 📂 New tournament groups created
- 📊 Match counts per group
- 📄 Pages needed per group
- ✅ Final group count

**Result**: Easy to diagnose issues from log output

---

### Issue #6: Tournament ID Mismatch ⚠️ TO INVESTIGATE
**Status**: To Investigate  
**Severity**: MEDIUM

**Problem**: 
JSON shows different tournament IDs for "PDC World Championship":
- `tournament.id: 169922` → "PDC World Championship 25/26"
- `tournament.id: 171078` → "PDC World Championship 2026"

These are SEASONAL tournament IDs, not the unique tournament ID.

**Current Code**: Uses `event["tournament"]["id"]` (line 669) for filtering

**Question**: Should we use:
1. `tournament.id` (seasonal ID) - current implementation
2. `tournament.uniqueTournament.id` (unique ID 616 for PDC WC)

**Impact**: Users need to configure the correct seasonal IDs, not unique tournament ID

**Recommendation**: Document that tournament IDs are seasonal IDs in configuration

---

### Issue #7: Additional Defensive Checks ✅ FIXED
**Status**: FIXED  
**Severity**: LOW

**Fix Applied**: Added NULL checks for:
- Tournament name (line 791)
- Status type with fallback to SCHEDULED (lines 795-812)
- Comprehensive logging for all NULL/missing values

**Result**: Code is more robust against unexpected JSON structures

---

## Analysis Methodology

1. **Collect JSON samples** from SofaScore API
2. **Review parsing functions** in SofaScoreLiveModule.cpp:
   - `parseTournamentsJson()`
   - `parseDailyEventsJson()`
   - `parseMatchStatistics()`
3. **Identify mismatches** between expected and actual JSON structure
4. **Test edge cases** (missing fields, unexpected values, etc.)
5. **Fix parsing logic** to handle real-world data
6. **Verify fixes** with provided JSON samples

---

## Testing Plan

### Phase 1: JSON Structure Analysis
- [ ] Receive JSON files from user
- [ ] Parse and validate JSON structure
- [ ] Compare against current parsing code
- [ ] Document structural differences

### Phase 2: Issue Identification
- [ ] Identify parsing errors
- [ ] Identify missing null checks
- [ ] Identify incorrect field access
- [ ] Identify data type mismatches

### Phase 3: Code Fixes
- [ ] Fix parsing logic
- [ ] Add defensive programming
- [ ] Handle edge cases
- [ ] Add error logging

### Phase 4: Validation
- [ ] Test with all provided JSON samples
- [ ] Verify no crashes or errors
- [ ] Verify correct data extraction
- [ ] Document any API changes

---

## Previous Issues (Fixed)

### FreeRTOS Mutex Deadlock
- **Status**: ✅ Fixed in commit 7a3140a
- **Description**: Mutex deadlock in updateLiveMatchStats() causing assertion failures
- **Solution**: Refactored to separate data collection from network access

---

## Notes

- SofaScore API is unofficial and undocumented
- API structure may change without notice
- Defensive programming is essential
- Always check for null/undefined fields in JSON

---

Last updated: 2025-12-14
